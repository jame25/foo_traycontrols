# Tray Controls Component for foobar2000

A foobar2000 component that provides comprehensive system tray functionality with popup controls, notifications, and minimize-to-tray behavior.

<img width="739" height="544" alt="foo_traytools" src="https://github.com/user-attachments/assets/9e6712a9-6ba2-454a-bbb2-9e7b83f55aea" />
<img width="331" height="122" alt="foo_trayctrls_popup" src="https://github.com/user-attachments/assets/b8ac2ab1-0db0-41fa-b452-0da49d91afa6" />


## Features

### Core Functionality
- **Always Visible Tray Icon**: System tray icon is always present for quick access
- **Smart Minimize to Tray**: Optional setting to hide to system tray when minimizing instead of taskbar
- **Mouse Wheel Volume Control**: Scroll mouse wheel over tray icon to adjust volume up/down
- **Real-time Track Information**: Hover tooltips show current artist and track information
- **Popup Control Panel**: Single-click the tray icon to show/hide an elegant control panel with:
  
  - High-quality album artwork display (80x80 pixels)
  - Current track title and artist information
  - Playback time display
  - Previous, Play/Pause, and Next control buttons with sharp icon rendering
- **Multiple Display Modes**: The control panel adapts to your needs with multiple display modes:
  
  - Docked Mode: Standard tray popup attached to the taskbar area.
  - Undocked (Miniplayer): Detachable window that can be dragged anywhere on screen. (Double-click artwork to toggle)
  - Compact Mode: A tiny, unobtrusive strip showing just artwork and essential info. (Double-click artwork in Undocked mode)
  - Expanded Artwork Mode: A large, immersive view focusing on high-quality album art. (Left-click artwork to toggle)
- **Popup Track Notifications**: Optional popup notifications on track changes featuring:
  - Album artwork display
  - Track and artist information
  - Smooth slide-in animation
- **Simple Tray Menu**: Right-click context menu provides:
  - Show/Hide foobar2000 window
  - Exit application

### Preferences
- **Settings Integration**: Accessible via foobar2000's Preferences → Tools → Tray Controls
- **Always Minimize to Tray**: When enabled, clicking the minimize button hides the window to the system tray
- **Show Popup Notification**: Enable/disable popup notifications on track changes
- **Persistent Configuration**: Settings are saved in foobar2000's configuration system

## Technical Implementation

### Architecture
- **Timer-based Detection**: Uses 500ms polling to detect window state changes (necessary due to foobar2000's custom window handling)
- **Window Subclassing**: Intercepts window messages for minimize detection
- **Global Mouse Hook**: Low-level mouse hook (`WH_MOUSE_LL`) for mouse wheel detection over tray icon
- **Service Integration**: Properly integrates with foobar2000's service system:
  - `initquit` service for component lifecycle
  - `play_callback_static` for real-time playback events
  - `preferences_page_v3` for settings UI
- **Singleton Pattern**: Uses singleton managers for reliable state management
- **GDI+ Image Processing**: High-quality album art rendering with aspect ratio preservation
- **Animation System**: Smooth slide-out animations with easing curves for enhanced user experience

### Real-time Updates
- **Track Change Detection**: Automatically updates tooltips, control panel, and popup notifications when songs change
- **Playback State Monitoring**: Shows current playback state (Playing/Paused/Stopped)
- **Metadata Integration**: Extracts artist and title from track metadata, with fallback to filename
- **Album Art Processing**: Dynamically loads and processes album artwork with high-quality scaling
- **Position Updates**: Real-time playback position display in control panel

## Files Structure

### Core Implementation
- `main.cpp` - Component entry point, service factories, and version declaration
- `tray_manager.h/cpp` - Main tray functionality and window management
- `control_panel.h/cpp` - Popup control panel with album art and playback controls
- `popup_window.h/cpp` - Track change notification popup with slide animations
- `preferences.h/cpp` - Settings page implementation with persistent storage
- `stdafx.h/cpp` - Precompiled headers for faster compilation

### Resources
- `foo_traycontrols.rc` - Dialog resource definitions
- `resource.h` - Resource ID definitions
- `tray_icon.ico` - Custom tray icon (16x16 and 32x32 sizes)
- `tray_icon.png` - Source icon image
- `play_icon.ico`, `pause_icon.ico`, `previous_icon.ico`, `next_icon.ico` - Control button icons

### Build System
- `foo_traycontrols.vcxproj` - Visual Studio project file
- `foo_traycontrols.def` - Export definitions
- `build-simple-traycontrols-x64.bat` - Quick build script

## Building

### Quick Build (Recommended)
```bash
# Run the automated build script
build-simple-traycontrols-x64.bat

# Output will be in x64\Release\foo_traycontrols.dll
# Copy to your foobar2000\components\ folder
```

### Manual Build
1. Open `foo_traycontrols.vcxproj` in Visual Studio 2022+
2. Ensure foobar2000 SDK is in `foobar2000_SDK\` directory  
3. Build for x64 platform (Release configuration recommended)
4. Copy resulting DLL to foobar2000 components folder

## Installation

1. Build the component using one of the methods above
2. Copy `foo_traycontrols.dll` to your foobar2000 `components` folder
3. Restart foobar2000
4. Configure via Preferences → Tools → Tray Controls

## Usage

### Basic Operation
1. The tray icon appears automatically when the component loads
2. Single-click the tray icon to show/hide the popup control panel
3. Right-click the tray icon for window controls (Show/Hide foobar2000, Exit)
4. Scroll mouse wheel over the tray icon to adjust volume (always enabled)
5. Hover over the icon to see current track information
6. Control panel automatically hides after 7 seconds of inactivity (with gliding animation)
7. Optional popup notifications appear on track changes (if enabled in preferences)

### Minimize to Tray
1. Go to Preferences → Tools → Tray Controls
2. Enable "Always minimize to system tray"
3. Click Apply
4. Now clicking the minimize button will hide to tray instead of taskbar

### Popup Notifications
1. Go to Preferences → Tools → Tray Controls
2. Enable "Show popup notification on track change"
3. Click Apply
4. Popup notifications will appear at top-left corner when tracks change
5. Each notification displays album art, track title, and artist information

## Requirements

- **foobar2000**: Version 1.6+ (64-bit)
- **Windows**: 7 or later (Windows 10+ recommended)
- **Build Tools**: Visual Studio 2022+ with v143 toolset and Windows 10/11 SDK
- **Dependencies**: GDI+ (included with Windows), shlwapi.lib

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

## License

This component is provided as-is for educational and personal use. Built using the foobar2000 SDK which has its own licensing terms.
