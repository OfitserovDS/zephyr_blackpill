#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#define LED_NODE DT_ALIAS(led0)      // PC13
#define BUTTON_NODE DT_ALIAS(sw0)    // PA0

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

// Семафоры и очереди
K_SEM_DEFINE(button_sem, 0, 1);           
K_SEM_DEFINE(usb_ready_sem, 0, 1);       
K_MSGQ_DEFINE(usb_msgq, sizeof(char[128]), 10, 4);  // Queue for USB messages

// Flags and counters
static volatile uint32_t button_press_count = 0;
static struct gpio_callback button_cb_data;

// Modes
typedef enum {
    MODE_ECHO = 0,   
    MODE_REVERSE,       
    MODE_COUNT
} EchoMode;

static EchoMode current_mode = MODE_ECHO;
static const char* mode_names[] = {"Echo", "Reverse"};

void led_task(void *p1, void *p2, void *p3);
void usb_task(void *p1, void *p2, void *p3);
void button_pressed_callback(const struct device *dev, 
                             struct gpio_callback *cb,
                             uint32_t pins);
void send_usb_message(const char *format, ...);
void send_usb_char(char c);
void process_string_reverse(const char *input, char *output);


// Thread stacks
#define LED_TASK_STACK_SIZE 1024
#define USB_TASK_STACK_SIZE 2048

K_THREAD_STACK_DEFINE(led_stack, LED_TASK_STACK_SIZE);
K_THREAD_STACK_DEFINE(usb_stack, USB_TASK_STACK_SIZE);

// Thread structs
static struct k_thread led_thread;
static struct k_thread usb_thread;

// Task priorities
#define LED_TASK_PRIORITY 5
#define USB_TASK_PRIORITY 4

void button_pressed_callback(const struct device *dev, 
                             struct gpio_callback *cb,
                             uint32_t pins)
{
    static uint64_t last_press_time = 0;
    uint64_t now = k_uptime_get();
    
    if (now - last_press_time < 300) {
        return;
    }
    last_press_time = now;
    
    button_press_count++;
    
    current_mode = (EchoMode)((current_mode + 1) % MODE_COUNT);
    
    k_sem_give(&button_sem);
}

void process_string_reverse(const char *input, char *output)
{
    int len = strlen(input);
    for (int i = 0; i < len; i++) {
        output[i] = input[len - 1 - i];
    }
    output[len] = '\0';
}

void send_usb_message(const char *format, ...)
{
    char buffer[128];
    va_list args;
    
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len > 0 && len < (int)sizeof(buffer)) {
        if (k_msgq_put(&usb_msgq, buffer, K_NO_WAIT) != 0) {
            // Ignore if queue is full
        }
    }
}

void send_usb_char(char c)
{
    char buffer[2] = {c, '\0'};
    if (k_msgq_put(&usb_msgq, buffer, K_NO_WAIT) != 0) {
        // Ignore if queue is full
    }
}

void led_task(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    if (!device_is_ready(led.port)) {
        return;
    }
    
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    
    uint32_t blink_counter = 0;
    bool led_state = false;
    
    send_usb_message("[LED] Task started\r\n");
    
    while (1) {
        blink_counter++;
        
        int delay_ms;
        switch (current_mode) {
            case MODE_ECHO:    delay_ms = 500; break;    // 2 Гц
            case MODE_REVERSE: delay_ms = 250; break;    // 4 Гц
            default:          delay_ms = 500; break;
        }
        
        led_state = !led_state;
        gpio_pin_set_dt(&led, led_state);
        
        if (blink_counter % 20 == 0) {
            send_usb_message("[LED] Blinks: %u, Mode: %s\r\n",
                           blink_counter, mode_names[current_mode]);
        }
        k_msleep(delay_ms);
    }
}

void usb_task(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);
    
    int ret;
    
    ret = usb_enable(NULL);
    if (ret != 0) {
        return;
    }
    
    k_msleep(2000);
    
    const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (!device_is_ready(uart_dev)) {
        return;
    }
    
    if (gpio_is_ready_dt(&button)) {
        ret = gpio_pin_configure_dt(&button, GPIO_INPUT | GPIO_PULL_UP);
        if (ret == 0) {
            ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_FALLING);
            if (ret == 0) {
                gpio_init_callback(&button_cb_data, button_pressed_callback, BIT(button.pin));
                gpio_add_callback(button.port, &button_cb_data);
                send_usb_message("[USB] Button ready on PA0\r\n");
            }
        }
    }
    
    send_usb_message("\r\n\r\n");
    send_usb_message("========================================\r\n");
    send_usb_message("   BLACK PILL F411 - RTOS DEMO\r\n");
    send_usb_message("========================================\r\n");
    send_usb_message("RTOS with 2 Tasks:\r\n");
    send_usb_message("  1. LED Task - blinks LED\r\n");
    send_usb_message("  2. USB Task - handles serial I/O\r\n");
    send_usb_message("========================================\r\n");
    send_usb_message("Modes (toggle with PA0 button):\r\n");
    send_usb_message("  • Echo (LED: 2Hz) - direct echo\r\n");
    send_usb_message("  • Reverse (LED: 4Hz) - reverse text\r\n");
    send_usb_message("========================================\r\n");
    send_usb_message("Current mode: %s\r\n", mode_names[current_mode]);
    send_usb_message("Button presses: %u\r\n", button_press_count);
    send_usb_message("Ready! Type something and press Enter...\r\n");
    send_usb_message("> ");
    
    k_sem_give(&usb_ready_sem);
    
    char input_buffer[64];
    int input_pos = 0;
    
    while (1) {
        if (k_sem_take(&button_sem, K_NO_WAIT) == 0) {
            send_usb_message("\r\n");
            send_usb_message("[BUTTON] Press #%u\r\n", button_press_count);
            send_usb_message("[MODE] %s\r\n", mode_names[current_mode]);
            send_usb_message("> ");
            
            input_pos = 0;
        }
        
        char msg_buffer[128];
        while (k_msgq_get(&usb_msgq, msg_buffer, K_NO_WAIT) == 0) {
            for (int i = 0; msg_buffer[i] != '\0'; i++) {
                uart_poll_out(uart_dev, msg_buffer[i]);
            }
        }
        
        uint8_t c;
        ret = uart_poll_in(uart_dev, &c);
        
        if (ret == 0) {
            send_usb_char(c);
            if (c == '\r' || c == '\n') {
                if (input_pos > 0) {
                    input_buffer[input_pos] = '\0';

                    if (current_mode == MODE_ECHO) {
                        send_usb_message("\r\n[ECHO] %s\r\n", input_buffer);
                    } 
                    else if (current_mode == MODE_REVERSE) {
                        char reversed[64];
                        process_string_reverse(input_buffer, reversed);
                        send_usb_message("\r\n[REVERSE] '%s' -> '%s'\r\n", 
                                       input_buffer, reversed);
                    }
                    
                    send_usb_message("> ");
                    input_pos = 0;
                } else {
                    send_usb_message("\r\n> ");
                }
            } 
            else if (c == 8 || c == 127) { // backspace
                if (input_pos > 0) {
                    input_pos--;
                    send_usb_message("\b \b");
                }
            }
            else if (input_pos < (int)sizeof(input_buffer) - 1) {
                input_buffer[input_pos++] = c;
            }
        }
        k_msleep(10);
    }
}

int main(void)
{
    k_thread_create(&led_thread, led_stack,
                    K_THREAD_STACK_SIZEOF(led_stack),
                    led_task,
                    NULL, NULL, NULL,
                    LED_TASK_PRIORITY, 0, K_NO_WAIT);
    
    k_thread_create(&usb_thread, usb_stack,
                    K_THREAD_STACK_SIZEOF(usb_stack),
                    usb_task,
                    NULL, NULL, NULL,
                    USB_TASK_PRIORITY, 0, K_NO_WAIT);
    
    k_sem_take(&usb_ready_sem, K_FOREVER);
    
    while (1) {
        k_sleep(K_FOREVER);
    }
    
    return 0;
}