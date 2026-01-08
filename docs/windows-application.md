# SparkinWin Windows Application Guide

## Overview

SparkinWin is the Windows supporting software for the MYNOVA-Sparkin wireless Bluetooth fingerprint recognition device. It provides the necessary components to integrate the device with Windows systems, enabling fingerprint-based authentication and system login.

## System Architecture

The Windows application suite consists of three main components:

```
┌─────────────────────────────────────────────────────────────────┐
│                       SparkinWin Suite                          │
└─────────────┬───────────────────┬───────────────────────────────┘
              │                   │
┌─────────────▼─────────┐ ┌───────▼─────────────┐ ┌────────────────────┐
│ Sparkin Service       │ │ Sparkin Config      │ │ Sparkin Credential │
│ (Background Service)  │ │ (Configuration App) │ │ Provider           │
└─────────────┬─────────┘ └─────────────────────┘ └─────────┬──────────┘
              │                                             │
┌─────────────▼──────────┐                       ┌──────────▼──────────┐
│ Bluetooth Communication│                       │ Windows Credential  │
│ (BLE Protocol)         │                       │ Manager Integration │
└────────────────────────┘                       └─────────────────────┘
```

## Core Components

### 1. Sparkin Service

**Type**: Windows Background Service

A Windows service that runs in the background to manage communication between the Sparkin device and Windows system:

- **Bluetooth Management**: Handles BLE connections with the Sparkin device
- **Message Processing**: Interprets messages from the device
- **System Integration**: Communicates with Windows Credential Manager
- **Automatic Startup**: Launches automatically when Windows starts
- **Error Handling**: Logs errors and provides diagnostics

### 2. Sparkin Config

**Type**: Windows Desktop Application

A user-friendly configuration client for managing the Sparkin device:

- **Device Pairing**: Connects and pairs with Sparkin devices
- **Fingerprint Management**: Enrolls, deletes, and manages fingerprints
- **System Login Setup**: Configures Windows login integration
- **Device Settings**: Adjusts sleep timeout, Bluetooth settings, etc.
- **Device Information**: Displays battery level, firmware version, etc.

### 3. Sparkin Credential Provider

**Type**: Windows Credential Provider DLL

Integrates with Windows Credential Manager to enable fingerprint login:

- **Login Screen Integration**: Adds fingerprint authentication option to Windows login screen
- **Secure Authentication**: Validates fingerprint matches
- **Password Management**: Securely stores and uses Windows login credentials
- **Multi-user Support**: Works with multiple Windows user accounts

## File Structure

```
SparkinWin/
├── SparkinCredentialProvider/   # Windows Credential Provider
│   ├── SparkinCredentialProvider.vcxproj
│   ├── CSampleProvider.cpp/h    # Main provider implementation
│   ├── CSampleCredential.cpp/h  # Credential implementation
│   ├── CPipeListener.cpp/h      # Inter-process communication
│   ├── helpers.cpp/h            # Utility functions
│   └── common.h                 # Common definitions
├── DigiCert/                    # Code signing certificates
│   ├── CoreLink.cer
│   ├── CoreLink.pfx
│   └── sign_files.ps1           # Signing script
└── NSIS_SetupSkin/              # Installation package resources
    ├── NSIS/                    # NSIS installer files
    └── FilesToInstall/          # Files to be installed
```

## Development Environment

### Prerequisites

- **IDE**: Visual Studio 2022
- **SDK**: Windows 10/11 SDK (latest version)
- **Libraries**: 
  - Windows Bluetooth API
  - Windows Credential Provider Framework
  - C++ Standard Library

### Build Instructions

1. Open `Sparkin.sln` in Visual Studio 2022
2. Select the appropriate build configuration (Debug/Release)
3. Build the solution
4. Sign the resulting binaries (optional, but recommended for Windows 10/11)

### Code Signing

The project includes code signing resources in the `DigiCert` directory:

```powershell
# Sign the compiled files using PowerShell
.ign_files.ps1
```

## Installation

### Using the Installer

1. Download the latest installer from the [Releases](https://github.com/yourusername/MYNOVA-Sparkin/releases) page
2. Run the installer with administrator privileges
3. Follow the on-screen instructions
4. After installation, the Sparkin service will start automatically

### Manual Installation

1. Build the solution in Visual Studio
2. Copy the compiled files to the target directory
3. Register the credential provider DLL
4. Install and start the Sparkin service

## Configuration

### Initial Setup

1. Launch the Sparkin Config application
2. Ensure Bluetooth is enabled on your Windows system
3. Put the Sparkin device in pairing mode (press and hold the button for 3 seconds)
4. Click "Add Device" in the config app
5. Select "Sparkin FP01" from the list of available devices
6. Follow the pairing instructions

### Fingerprint Enrollment

1. Ensure the device is connected
2. Go to the "Fingerprint Management" tab
3. Click "Enroll New Fingerprint"
4. Follow the on-screen instructions to scan your fingerprint multiple times
5. Assign a name to the fingerprint for easy identification

### System Login Integration

1. Go to the "Login Settings" tab
2. Enable "Automatic Login with Fingerprint"
3. Enter your Windows login password when prompted
4. Click "Save" to complete the setup

## Communication Protocol

### Bluetooth BLE Profile

The Sparkin device uses Bluetooth BLE 5.0 for communication with Windows:

- **Service UUID**: `0000180f-0000-1000-8000-00805f9b34fb`
- **Characteristic UUIDs**:
  - `00002a19-0000-1000-8000-00805f9b34fb` (Battery Level)
  - Custom UUIDs for fingerprint data and commands

### Message Types

The system uses a binary protocol for communication:

| Message ID | Description | Direction |
|------------|-------------|-----------|
| 0x01       | Fingerprint Search Result | Device → PC |
| 0x02       | Enroll Fingerprint Request | PC → Device |
| 0x03       | Enroll Fingerprint Result | Device → PC |
| 0x04       | Delete Fingerprint Request | PC → Device |
| 0x05       | Device Status Request | PC → Device |
| 0x06       | Device Status Response | Device → PC |
| 0x07       | Configuration Update | PC → Device |

## System Login Integration

### Windows Credential Provider Flow

```
1. User sees Windows login screen
2. User selects "Fingerprint" authentication option
3. Credential provider sends request to Sparkin Service
4. Service sends request to Sparkin device
5. Device waits for fingerprint scan
6. User scans fingerprint
7. Device processes and matches fingerprint
8. Device sends result to Service
9. Service sends result to Credential Provider
10. If successful, Credential Provider authenticates user
11. Windows logs in the user
```

### Security Considerations

- Windows login credentials are stored securely using Windows Credential Manager
- Fingerprint data never leaves the Sparkin device (processed locally)
- Bluetooth communication is encrypted using BLE security protocols
- All components are signed with a trusted certificate to prevent tampering

## Troubleshooting

### Common Issues

1. **Device Not Found**
   - Ensure Bluetooth is enabled on your Windows system
   - Ensure the Sparkin device is in pairing mode
   - Restart the Sparkin service

2. **Fingerprint Not Recognized**
   - Try scanning your fingerprint again with a different angle
   - Re-enroll your fingerprint
   - Check if the device battery is low

3. **Login Option Not Showing**
   - Ensure the Sparkin Credential Provider is installed correctly
   - Restart your computer
   - Check if the Sparkin service is running

4. **Service Won't Start**
   - Check the Windows Event Viewer for error messages
   - Ensure you have the latest Windows updates installed
   - Reinstall the Sparkin software

### Log Files

- **Sparkin Service Logs**: `%ProgramData%\MYNOVA\Sparkin\logs\service.log`
- **Windows Event Log**: Application and Services Logs > Sparkin

## Development

### Extending the Application

#### Adding New Features

1. **Service**: Add new functionality to the Sparkin Service by extending the `SparkinService` class
2. **Configuration App**: Add new UI elements and functionality to the configuration application
3. **Credential Provider**: Extend the credential provider to support additional authentication scenarios

#### Modifying the Communication Protocol

1. Update the message definitions in both the Windows service and device firmware
2. Ensure backward compatibility if modifying existing messages
3. Test thoroughly with both new and old firmware versions

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0   | 2023-12-15 | Initial release |
| 1.0.1   | 2023-12-20 | Improved Bluetooth connection stability |
| 1.1.0   | 2024-01-05 | Added support for multiple user accounts |

---

[Back to README](../README.md)