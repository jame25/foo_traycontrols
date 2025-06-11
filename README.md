# Tray Controls Component for foobar2000

A foobar2000 64-bit component that provides system tray functionality with playback controls and track information.

![foo_traycontrols](https://github.com/user-attachments/assets/80c43e91-53b6-4f83-a872-de01108735f3)

A foobar2000 64-bit component that provides comprehensive system tray functionality with playback controls, track information, and minimize-to-tray behavior.

## Features

### Core Functionality
- **Always Visible Tray Icon**: System tray icon is always present for quick access
- **Smart Minimize to Tray**: Optional setting to hide to system tray when minimizing instead of taskbar
- **Real-time Track Information**: Hover tooltips show current artist and track information
- **Comprehensive Playback Controls**: Right-click context menu provides:
  - Play/Pause track
  - Previous Track  
  - Next Track
  - Show/Hide foobar2000 window
  - Exit application
- **Quick Window Toggle**: Double-click the tray icon to show/hide the main window

### Preferences
- **Settings Integration**: Accessible via foobar2000's Preferences → Tools → Tray Controls
- **Always Minimize to Tray**: When enabled, clicking the minimize button hides the window to the system tray
- **Persistent Configuration**: Settings are saved in foobar2000's configuration system

## Technical Implementation

### Architecture
- **Timer-based Detection**: Uses 500ms polling to detect window state changes (necessary due to foobar2000's custom window handling)
- **Window Subclassing**: Intercepts window messages for minimize detection
- **Service Integration**: Properly integrates with foobar2000's service system:
  - `initquit` service for component lifecycle
  - `play_callback_static` for real-time playbook events
  - `preferences_page_v3` for settings UI
- **Singleton Pattern**: Uses singleton tray manager for reliable state management

### Real-time Updates
- **Track Change Detection**: Automatically updates tooltip when songs change
- **Playback State Monitoring**: Shows current playback state (Playing/Paused/Stopped)
- **Metadata Integration**: Extracts artist and title from track metadata, with fallback to filename

## Files Structure

### Core Implementation
- `main.cpp` - Component entry point, service factories, and version declaration
- `tray_manager.h/cpp` - Main tray functionality and window management
- `preferences.h/cpp` - Settings page implementation with persistent storage
- `stdafx.h/cpp` - Precompiled headers for faster compilation

### Resources
- `foo_traycontrols.rc` - Dialog resource definitions
- `resource.h` - Resource ID definitions
- `tray_icon.ico` - Custom tray icon (16x16 and 32x32 sizes)
- `tray_icon.png` - Source icon image

### Build System
- `foo_traycontrols.vcxproj` - Visual Studio project file
- `foo_traycontrols.def` - Export definitions
- `build-simple-traycontrols-x64.bat` - Quick build script

## Building

### Quick Build
```bash
# Run the automated build script
build-simple-traycontrols-x64.bat

# Output will be in x64\Release\foo_traycontrols.dll
# Copy to your foobar2000\components\ folder
```

## Installation

1. Build the component using one of the methods above
2. Copy `foo_traycontrols.dll` to your foobar2000 `components` folder
3. Restart foobar2000
4. Configure via Preferences → Tools → Tray Controls

## Usage

### Basic Operation
1. The tray icon appears automatically when the component loads
2. Right-click the tray icon for playback controls
3. Double-click the tray icon to show/hide the main window
4. Hover over the icon to see current track information

### Minimize to Tray
1. Go to Preferences → Tools → Tray Controls
2. Enable "Always minimize to system tray"
3. Click Apply
4. Now clicking the minimize button will hide to tray instead of taskbar

## Requirements

- **foobar2000**: Version 1.6+ (64-bit)
- **Windows**: 7 or later (Windows 10+ recommended)
- **Build Tools**: Visual Studio 2022+ with v143 toolset and Windows 10/11 SDK
- **Dependencies**: No external dependencies (ATL/MFC not required)

## Limitations

- **"Minimize on Close" Not Supported**: Due to foobar2000 SDK limitations, intercepting the close button to minimize instead of exit is not reliably possible
- **Timer-based Detection**: Uses polling for window state changes due to foobar2000's custom message handling
- **Windows Only**: Component is designed specifically for Windows system tray

## Development Notes

### SDK Integration
- Built against foobar2000 SDK (included in `foobar2000_SDK/` directory)
- Uses proper service factory registration for all components
- Follows foobar2000 coding conventions and component guidelines

### Configuration System
- Settings stored in foobar2000's internal configuration using `cfg_int` with unique GUIDs
- Preferences dialog implemented using Win32 dialog resources
- Proper change detection and Apply button state management

### Error Handling
- Graceful fallbacks for missing metadata
- Safe window handle management
- Resource cleanup on component unload

## Version History

**v1.0.1** - Initial Release
- Always-visible system tray icon
- Minimize to tray functionality
- Real-time track information tooltips
- Complete playback control context menu
- Integrated preferences page
- Timer-based window state detection

## License

This component is provided as-is for educational and personal use. Built using the foobar2000 SDK which has its own licensing terms.
