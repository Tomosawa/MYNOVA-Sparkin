# SparkinFW Firmware Architecture

## Overview

SparkinFW is the firmware for the MYNOVA-Sparkin wireless Bluetooth fingerprint recognition device. It's built on the Arduino framework for the ESP32-C3 microcontroller and implements a modular architecture to manage all device functionalities.

## System Architecture

The firmware follows a modular design pattern with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────────┐
│                         Main Program                            │
│                        (SparkinFW.ino)                          │
└─────────────┬───────────────────┬───────────────────┬───────────┘
              │                   │                   │
┌─────────────▼─────────┐ ┌───────▼─────────────┐ ┌───▼───────────────┐
│  Fingerprint Module   │ │  Bluetooth Module   │ │ Power Management  │
│  (Fingerprint.cpp/h)  │ │  (BleKeyboard.cpp/h,│ │ (BatteryManager.  │
│                       │ │  BluetoothManager.  │ │  cpp/h, Sleep.cpp │
│                       │ │  cpp/h, BluetoothHa │ │  /h)              │
│                       │ │  ndle.cpp/h)        │ │                   │
└─────────────┬─────────┘ └───────┬─────────────┘ └─┬─────────────────┘
              │                   │                 │
┌─────────────▼─────────┐ ┌───────▼─────────────┐ ┌─▼─────────────────┐
│ Button Management     │ │ OTA Update          │ │ Config Management │
│ (ButtonHandle.cpp/h,  │ │ (BluetoothOTA.cpp/h)│ │ (ConfigManager.   │
│ ButtonTimer.cpp/h)    │ │                     │ │  cpp/h)           │
└─────────────┬─────────┘ └─────────────────────┘ └───────────────────┘
              │
┌─────────────▼─────────┐
│ Common Utilities      │
│ (Common.cpp/h, IOPin. │
│  h, Version.h)        │
└───────────────────────┘
```

## Core Modules

### 1. Main Program (SparkinFW.ino)

The main entry point that coordinates all modules:

- **setup()**: Initializes all hardware and software modules
- **loop()**: Main program loop handling continuous operations
- **Event handling**: Processes events from different modules
- **State management**: Maintains the overall device state

### 2. Fingerprint Module

**File**: `Fingerprint.cpp/h`

Responsible for all fingerprint-related operations:

- Initialization and configuration of the optical fingerprint sensor
- Fingerprint enrollment (storing new fingerprints)
- Fingerprint identification (matching against stored fingerprints)
- Fingerprint database management
- Fingerprint template encryption

### 3. Bluetooth Module

**Files**: `BluetoothManager.cpp/h`, `BleKeyboard.cpp/h`, `BluetoothHandle.cpp/h`

Handles all wireless communication:

- **BLE Server**: Manages Bluetooth connections and services
- **Characteristic Management**: Handles data exchange between device and Windows
- **Message Protocol**: Implements custom message format for communication
- **BleKeyboard**: Emulates keyboard input for Windows login
- **Advertising**: Manages device discovery and pairing
- **Connection Handling**: Manages Bluetooth connection states and events

Key Bluetooth UUIDs:
- Service UUID: `0000180f-0000-1000-8000-00805f9b34fb`
- Characteristic UUIDs: Custom for fingerprint data and commands

### 4. Power Management

**Files**: `BatteryManager.cpp/h`, `Sleep.cpp/h`

Manages device power consumption:

- **Battery Monitoring**: Measures battery voltage and level
- **Charging Management**: Handles Type-C charging state
- **Sleep Mode**: Automatic sleep after period of inactivity
- **Wake-up**: Fingerprint sensor or button wake-up triggers
- **Power Optimization**: Controls peripheral power states

### 5. Button Management

**Files**: `ButtonHandle.cpp/h`, `ButtonTimer.cpp/h`

Handles physical button operations:

- **Button Press Detection**: Monitors pairing button state
- **Long Press Handling**: 3-second press for pairing mode
- **Button Debouncing**: Eliminates false triggers
- **Timer Functions**: Implements timed button operations

### 6. OTA Update

**File**: `BluetoothOTA.cpp/h`

Enables over-the-air firmware updates:

- **Secure Updates**: Validates firmware signatures
- **Update Process**: Manages firmware download and installation
- **Rollback**: Automatic recovery if update fails
- **Version Check**: Ensures compatible firmware versions

### 7. Configuration Management

**File**: `ConfigManager.cpp/h`

Stores and retrieves device configuration:

- **Persistent Storage**: Uses ESP32 non-volatile storage
- **Configuration Parameters**:
  - Bluetooth name and settings
  - Sleep timeout
  - Fingerprint matching threshold
  - Device ID and settings
- **Factory Reset**: Restores default configuration

### 8. Common Utilities

**Files**: `Common.cpp/h`, `IOPin.h`, `Version.h`

Shared functionality across modules:

- **Pin Definitions**: Centralized IO pin mapping
- **Debug Functions**: Logging and debugging utilities
- **Version Information**: Firmware version tracking
- **Data Structures**: Common data types and structures
- **Helper Functions**: String manipulation, data conversion, etc.

## Main Program Flow

### Initialization Flow

```
1. System boot
2. Initialize common utilities and pin mappings
3. Initialize configuration manager
4. Initialize battery management
5. Initialize fingerprint module
6. Initialize Bluetooth services
7. Initialize button handling
8. Initialize sleep management
9. Start main loop
```

### Fingerprint Recognition Flow

```
1. Fingerprint sensor detects touch
2. Capture fingerprint image
3. Extract fingerprint features
4. Match against stored templates
5. If match found:
   a. Send success message via Bluetooth
   b. Trigger Windows login if configured
6. If no match:
   a. Send failure message via Bluetooth
   b. Optionally alert user (LED feedback)
```

## Communication Protocol

The device uses a custom binary protocol over Bluetooth BLE for communication with the Windows application:

### Message Format

```
┌────────────┬────────────┬───────────┐
│ Message ID │ Data Length│   Data    │
└────────────┴────────────┴───────────┘
  (1 byte)    (1 byte)    (N bytes)
```

### Key Message Types

| Message ID | Description | Direction |
|------------|-------------|-----------|
| 0x01       | Fingerprint Search Result | Device → PC |
| 0x02       | Enroll Fingerprint Request | PC → Device |
| 0x03       | Enroll Fingerprint Result | Device → PC |
| 0x04       | Delete Fingerprint Request | PC → Device |
| 0x05       | Device Status Request | PC → Device |
| 0x06       | Device Status Response | Device → PC |
| 0x07       | Configuration Update | PC → Device |
| 0x08       | Sleep Mode Request | PC → Device |
| 0x09       | Wake-up Request | PC → Device |

## Power States

| State | Description | Power Consumption |
|-------|-------------|-------------------|
| Active | Full functionality | High |
| Standby | Bluetooth active, fingerprint sensor ready | Medium |
| Sleep | Bluetooth off, fingerprint sensor on standby | Low |
| Deep Sleep | Minimal functionality, wake-up on touch | Very Low |

## Development Guide

### Prerequisites

- Arduino IDE 2.3.6+
- ESP32 Core 3.1.1+
- ESP32-C3 development board
- Required libraries:
  - ESP32 BLE Arduino
  - ArduinoJson
  - Adafruit Fingerprint Sensor Library

### Building and Flashing

1. Open `SparkinFW.ino` in Arduino IDE
2. Select "ESP32C3 Dev Module" as the board
3. Select the appropriate port
4. Click "Upload" to flash the firmware

### Debugging

- Use Serial Monitor at 115200 baud for debug messages
- Check Bluetooth logs in Windows application
- Use LED indicators for device state (pairing, charging, etc.)

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0   | 2023-12-15 | Initial release |
| 1.0.1   | 2023-12-20 | Fixed battery reporting |
| 1.1     | 2024-01-05 | Added OTA update support |

---

[Back to README](../README.md)