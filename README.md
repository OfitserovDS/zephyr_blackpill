# Black Pill STM32F411 - Zephyr RTOS Demo Project

## Overview
A demonstration RTOS application for the **Black Pill STM32F411CEU6** board featuring:
- **Two concurrent tasks** using Zephyr RTOS
- **USB CDC virtual COM port** for serial communication
- **Button-controlled mode switching** with debouncing
- **LED visual feedback** with mode-dependent blink rates

## Quick Start

### Prerequisites (Ubuntu/WSL)
```bash
sudo apt update && sudo apt install -y \
    git cmake ninja-build gperf ccache dfu-util \
    device-tree-compiler python3 python3-pip python3-venv

# 1. Create Python virtual environment
python3 -m venv ~/zephyr-venv
source ~/zephyr-venv/bin/activate
pip install west

# 2. Install Zephyr SDK
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.5/zephyr-sdk-0.16.5_linux-x86_64.tar.xz
tar xvf zephyr-sdk-0.16.5_linux-x86_64.tar.xz
cd zephyr-sdk-0.16.5 && ./setup.sh && cd ~

# 3. Initialize Zephyr workspace
west init ~/zephyrproject
cd ~/zephyrproject
west update
west zephyr-export
pip install -r zephyr/scripts/requirements.txt

# 4. Add to ~/.bashrc (permanent setup)
echo 'export ZEPHYR_BASE=~/zephyrproject/zephyr' >> ~/.bashrc
echo 'export ZEPHYR_TOOLCHAIN_VARIANT=zephyr' >> ~/.bashrc
source ~/.bashrc
```
### Project structure
```bash
zephyr_blackpill/
├── src/main.c          # Application source code
├── prj.conf           # Zephyr configuration
├── app.overlay        # Device tree overlay
└── CMakeLists.txt     # Build configuration
```
### How to build
```bash
# Clone and setup project
git clone <repository-url>
cd zephyr_blackpill

# Configure and build
mkdir build && cd build
cmake -GNinja -DBOARD=blackpill_f411ce ..
ninja
```
