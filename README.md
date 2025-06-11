# Tray Controls Component for foobar2000

A foobar2000 64-bit component that provides system tray functionality with playback controls.

![foo_traycontrols](https://github.com/user-attachments/assets/0ea2d58d-979d-46cc-b26e-4b9f3f09cc91)


## Features

- **Minimize to System Tray**: Minimize the foobar2000 main window to the system tray instead of the taskbar
- **Tray Icon Context Menu**: Right-click the tray icon to access playback controls:
  - Play/Pause track
  - Previous Track
  - Next Track
  - Restore Window
- **Double-click Restore**: Double-click the tray icon to restore the main window

## Files

- `main.cpp` - Component version declaration and entry point
- `tray_manager.h/cpp` - Main tray functionality implementation
- `stdafx.h/cpp` - Precompiled headers
- `resource.h` - Resource definitions for menu items and icons
- `foo_traycontrols.rc` - Resource script
- `foo_traycontrols.vcxproj` - Visual Studio project file
- `tray_icon.ico` - Tray icon (placeholder - needs actual icon file)

## Building

### Option 1: Quick Build (Recommended)
1. Run `build-simple-traycontrols-x64.bat` from the project directory
2. The script will build the component automatically
3. Copy the resulting `x64\Release\foo_traycontrols.dll` to your foobar2000 components folder

### Option 2: Manual Build
1. Open `foo_traycontrols.vcxproj` in Visual Studio 2022 or later (requires v143 build tools)
2. The foobar2000 SDK should be located in `lib\foobar2000_SDK\`
3. Build for x64 platform (Debug or Release configuration)
4. Copy the resulting `foo_traycontrols.dll` to your foobar2000 components folder

## Implementation Notes

- Uses the foobar2000 SDK `initquit` service for initialization
- Integrates with `playback_control` service for media controls
- Subclasses the main window to intercept minimize messages
- Creates system tray icon only when minimized to reduce resource usage
- Follows foobar2000 component architecture and naming conventions

## Requirements

- foobar2000 64-bit
- Windows 7 or later
- Visual Studio 2022+ with v143 build tools and Windows 10 SDK
- **Note**: ATL/MFC libraries are NOT required for this component
