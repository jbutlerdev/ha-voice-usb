# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the **Home Assistant Voice USB Edition** firmware - a specialized ESPHome-based firmware that enables direct USB communication between Home Assistant Voice hardware and a macOS application, eliminating WiFi dependencies.

## Essential Commands

### Environment Setup
```bash
python3 -m venv venv
source venv/bin/activate
pip install esphome
```

### Build and Flash
```bash
# Primary build command - NEVER use PlatformIO directly
esphome run home-assistant-voice.usb-comm.yaml

# Compile only (for testing)
esphome compile home-assistant-voice.usb-comm.yaml

# Clean build cache
esphome clean home-assistant-voice.usb-comm.yaml
```

### Apple Silicon Compatibility
If encountering "bad CPU type" errors on Apple Silicon, install Rosetta 2:
```bash
softwareupload --install-rosetta
```

## Architecture Overview

### Core Configuration
- `home-assistant-voice.usb-comm.yaml` - Main firmware configuration (25,000+ lines)
- Hardware: ESP32-S3 with 240MHz CPU, 16MB flash, optimized memory layout
- Communication: USB Serial/JTAG via GPIO41/42 using JSON protocol

### Custom Components

**USB Communication Component** (`components/usb_communication/`):
- Core USB communication implementation with JSON protocol
- Handles heartbeat, status updates, and audio streaming
- 16KB audio buffer for real-time streaming

**Voice Kit Component** (`components/voice_kit/`):
- Hardware abstraction layer for Home Assistant Voice hardware
- Device automation and LED control

### Key Features
- **USB-Only Operation**: No WiFi dependencies, pure USB communication
- **Audio Processing**: Wake word detection, bidirectional audio streaming
- **LED Indicators**: Connection status and voice assistant phase feedback
- **Configuration Management**: Local wake word sensitivity and device settings

## Development Guidelines

### Critical Rules
- **NEVER flash firmware automatically** - always require manual confirmation
- **ALWAYS use ESPHome commands** - never use PlatformIO directly
- **USB Serial logging disabled** - to prevent interference with communication protocol

### Communication Protocol
- JSON-based message format between device and macOS application
- Heartbeat system for connection monitoring
- Structured audio chunk streaming
- Comprehensive error reporting and status updates

### Hardware Specifications
- Target: ESP32-S3 DevKit (240MHz, 16MB flash)
- Audio: 16KB USB buffer, real-time streaming capability
- Memory: PSRAM optimization for instructions and data caching
- Communication: USB-C for power and data (GPIO41/42)

### Testing Approach
- Use `esphome compile` for syntax and configuration validation
- Manual flashing required for hardware testing
- Monitor USB serial output for debugging (when not interfering with protocol)

## Project Structure
- Main config: `home-assistant-voice.usb-comm.yaml`
- Grove configurations: `grove-i2c.yaml`, `grove-power.yaml`
- Audio assets: Various `.flac` and `.mp3` files for user feedback
- Custom components: `components/` directory with USB and voice kit implementations

## Licensing
- C++ runtime code: GPLv3
- Python code: MIT
- ESPHome framework: MIT/GPLv3 combination