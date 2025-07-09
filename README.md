# Home Assistant Voice USB Edition

This is the USB communication version of the Home Assistant Voice firmware that enables direct communication with a local macOS application without requiring WiFi.

## Key Features

- **No WiFi Required**: Completely removes WiFi dependencies
- **USB-Only Communication**: Uses GPIO41/42 for serial communication with Mac app
- **JSON Protocol**: Heartbeat, status updates, and configuration over USB
- **LED Indicators**: Shows USB connection status with green blinking LED
- **Local Control**: Wake word sensitivity and device configuration via USB

## Build Environment Setup

### Prerequisites
- Python 3.9+
- ESPHome installed in virtual environment
- Device connected via USB

### Installation Steps
```bash
# Create and activate virtual environment
python3 -m venv venv
source venv/bin/activate

# Install ESPHome
pip install esphome
```

### Apple Silicon Mac Issue & Solution
**Problem**: ESPHome compilation fails on Apple Silicon Macs with error:
```
zsh: bad CPU type in executable: /Users/[user]/.platformio/packages/tool-ninja/ninja
```

**Solution**: Install Rosetta 2 to run x86 binaries:
```bash
softwareupdate --install-rosetta
```

## Compilation and Flashing

### Quick Start
```bash
# Activate virtual environment
source venv/bin/activate

# Build and flash USB communication firmware
esphome run home-assistant-voice.usb-comm.yaml
```

### Detailed Commands

#### Compile Only (Test Without Flashing)
```bash
# Activate environment
source venv/bin/activate

# Compile firmware to check for errors
esphome compile home-assistant-voice.usb-comm.yaml
```

#### Clean Build Cache
```bash
# Clear build artifacts if needed
esphome clean home-assistant-voice.usb-comm.yaml
```

#### Full Build and Flash Process
```bash
# 1. Activate virtual environment
source venv/bin/activate

# 2. Connect device via USB-C cable

# 3. Build and flash firmware (will prompt for device selection)
esphome run home-assistant-voice.usb-comm.yaml

# 4. Monitor serial output (optional)
esphome logs home-assistant-voice.usb-comm.yaml
```

### Device Connection
1. Connect Home Assistant Voice device to Mac via USB-C cable
2. Device should appear as a serial port (e.g., `/dev/cu.usbmodem*`)
3. ESPHome will automatically detect and prompt for device selection during flashing

## Important Notes

- **CRITICAL**: Always use ESPHome commands (`esphome run`), NEVER use PlatformIO directly
- **NEVER automatically flash firmware** - always run commands manually for safety
- Device requires ESPHome 2025.5.1+ (specified in YAML configuration)
- USB firmware disables serial logging to prevent interference with communication protocol
- Designed for direct communication with Swift macOS application

## Troubleshooting

### Common Issues
- **Build fails with git errors**: Ensure you're in the project's git repository
- **"bad CPU type" errors on Apple Silicon**: Install Rosetta 2 (`softwareupdate --install-rosetta`)
- **Device not detected**: Check USB-C cable connection and try different USB port
- **Compilation errors**: Clear build cache with `esphome clean`
- **Flash timeout**: Try holding BOOT button on device during flash initiation

### Development Commands
```bash
# Test compilation without hardware
esphome compile home-assistant-voice.usb-comm.yaml

# Clean all build artifacts
esphome clean home-assistant-voice.usb-comm.yaml

# View device logs (after successful flash)
esphome logs home-assistant-voice.usb-comm.yaml
```