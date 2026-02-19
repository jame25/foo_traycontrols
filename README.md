# Tray Controls Component for foobar2000

A foobar2000 component that provides comprehensive system tray functionality with popup controls, notifications, and minimize-to-tray behavior.

<img width="738" height="540" alt="trayctrlsprefs" src="https://github.com/user-attachments/assets/cbae815b-3090-4e48-bf2c-3024c453aec6" />
<img width="336" height="118" alt="dark" src="https://github.com/user-attachments/assets/8912d914-58b6-43f1-9572-c310242ff8f2" />
<img width="338" height="120" alt="light" src="https://github.com/user-attachments/assets/64ecfa0f-873a-494f-aee8-8049ac1b0527" />


## Features

### Core Functionality
- **Always Visible Tray Icon**: System tray icon is always present for quick access
- **Smart Minimize to Tray**: Optional setting to hide to system tray when minimizing instead of taskbar
- **Mouse Wheel Volume Control**: Scroll mouse wheel over tray icon to adjust volume up/down
- **Real-time Track Information**: Hover tooltips show current artist and track information
- **Popup Control Panel**: Single-click the tray icon to show/hide an elegant control panel with:
  
  - High-quality album artwork display
  - Current track title and artist information
  - Playback time display with seek bar
  - Previous, Play/Pause, Next, Shuffle, and Repeat control buttons
### Multiple Display Modes
The control panel adapts to your needs with four distinct display modes:

1. **Docked Mode**: Standard tray popup attached to the taskbar area
   - Simplified controls: Previous, Play/Pause, Next only (Shuffle/Repeat hidden)
   - Auto-hides after 5 seconds
   
2. **Undocked (MiniPlayer)**: Detachable window that can be dragged anywhere on screen
   - Full playback controls including Shuffle and Repeat
   - Remembers window position between sessions
   - **Collapse Triangle**: Small white triangle in top-right corner to switch to Compact mode
   
3. **Compact Mode**: A tiny, unobtrusive strip showing just artwork and essential info
   
4. **Expanded Artwork Mode**: A large, immersive view focusing on high-quality album art
   - Hover overlays with smooth fade animations for controls
   - Respects album art aspect ratio
   - **Collapse Triangle**: Small white triangle in top-right corner to restore Undocked mode

### Dark / Light Mode Support
- **Auto-detect**: Automatically follows foobar2000's dark mode setting (Default UI)
- **Force Dark/Light**: Override with forced dark or light mode via preferences
- **Real-time Updates**: Theme changes apply immediately
- **Applies to All Modes**: Docked, Undocked, Expanded, Compact, and Popup notifications
     
### Slide-to-Side Panel (Panel Peek)
- **Panel Peek**: Single-click on the edge of the panel (Expanded, Undocked, or Compact modes) slides the panel to the side of the screen, leaving 70px visible ("peeking")
- **Restore**: Click anywhere on the slid panel to slide it back to its original position
- **Always Slide-to-Side Option**: When enabled, the MiniPlayer slides to the side instead of closing
   
### Toolbar Button
- **Launch MiniPlayer**: Accessible via View → Tray Controls → Launch MiniPlayer
- Can be added to the foobar2000 toolbar for quick access
- Toggles the MiniPlayer visibility (shows/hides, or slides back if peeking)

### Popup Track Notifications
Optional popup notifications on track changes featuring:
- Album artwork display
- Track and artist information
- Smooth slide-in animation
- Configurable display position (top-left, top-right, bottom-left, bottom-right)
- Configurable display duration (1-10 seconds)
  
### Preferences (Tabbed Interface)
Accessible via foobar2000's Preferences → Tools → Tray Controls

#### General Tab
- **Always Minimize to Tray**: When enabled, clicking the minimize button hides the window to the system tray
- **Show Popup Notification**: Enable/disable popup notifications on track changes
- **Popup Position**: Choose where track change notifications appear
- **Popup Duration**: Adjust how long notifications remain visible (1-10 seconds)
- **Disable MiniPlayer**: Option to disable the MiniPlayer functionality
- **Disable Slide-to-Side**: Disable the panel slide peek functionality
- **Always Slide-to-Side**: MiniPlayer slides instead of closing when toggle button is clicked
- **Slide Duration**: Configure the animation speed for slide animations
- **Use Rounded Corners**: Toggle Windows 11 style rounded corners on/off
- **Theme Mode**: Choose Auto (follows foobar2000), Force Dark, or Force Light mode

#### Fonts Tab
Mode-specific font customization for all display modes:
- **Docked Control Panel**: Artist and Track fonts
- **Undocked Control Panel**: Artist and Track fonts
- **Expanded Artwork Mode**: Artist and Track fonts
- **Compact Mode**: Artist and Track fonts
- **Reset All Button**: Restore all fonts to system defaults (Segoe UI)

## Technical Implementation

### Architecture
- **Timer-based Detection**: Uses 500ms polling to detect window state changes (necessary due to foobar2000's custom window handling)
- **Window Subclassing**: Intercepts window messages for minimize detection
- **Global Mouse Hook**: Low-level mouse hook (`WH_MOUSE_LL`) for mouse wheel detection over tray icon
- **Service Integration**: Properly integrates with foobar2000's service system:
  - `initquit` service for component lifecycle
  - `play_callback_static` for real-time playback events
  - `preferences_page_v3` for settings UI
  - `mainmenu_commands` for toolbar integration
- **Singleton Pattern**: Uses singleton managers for reliable state management
- **GDI+ Image Processing**: High-quality album art rendering with aspect ratio preservation
- **Animation System**: Smooth slide-out animations with ease-out quadratic curves for enhanced user experience

### Real-time Updates
- **Track Change Detection**: Automatically updates tooltips, control panel, and popup notifications when songs change
- **Playback State Monitoring**: Shows current playback state (Playing/Paused/Stopped)
- **Metadata Integration**: Extracts artist and title from track metadata, with fallback to filename
- **Album Art Processing**: Dynamically loads and processes album artwork with high-quality scaling
- **Position Updates**: Real-time playback position display in control panel
- **Shuffle/Repeat State Sync**: Synchronizes with foobar2000's playback order settings

## Files Structure

### Core Implementation
- `main.cpp` - Component entry point, service factories, and version declaration
- `tray_manager.h/cpp` - Main tray functionality and window management
- `control_panel.h/cpp` - Popup control panel with album art and playback controls
- `popup_window.h/cpp` - Track change notification popup with slide animations
- `preferences.h/cpp` - Settings page implementation with tabbed interface and persistent storage
- `mainmenu_commands.cpp` - Toolbar button command registration
- `stdafx.h/cpp` - Precompiled headers for faster compilation

### Resources
- `foo_traycontrols.rc` - Dialog resource definitions
- `resource.h` - Resource ID definitions
- `tray_icon.ico` - Custom tray icon (16x16 and 32x32 sizes)
- `tray_icon.png` - Source icon image
- `play_icon.ico`, `pause_icon.ico`, `previous_icon.ico`, `next_icon.ico` - Control button icons
- `miniplayer_icon.ico` - MiniPlayer window icon

### Build System
- `foo_traycontrols.vcxproj` - Visual Studio project file
- `foo_traycontrols.def` - Export definitions
- `build-simple-traycontrols-x64.bat` - Quick build script for 64-bit
- `build-simple-traycontrols-x86.bat` - Quick build script for 32-bit
- `rebuild-all-v143-x64.bat` - Full rebuild script with v143 toolset

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
2. Single-click the tray icon to show/hide the popup control panel (Docked mode)
3. Right-click the tray icon for window controls (Show/Hide foobar2000, Exit)
4. Scroll mouse wheel over the tray icon to adjust volume
5. Hover over the icon to see current track information
6. Control panel automatically hides after 5 seconds of inactivity (Docked mode)

### MiniPlayer Operation
1. Drag the MiniPlayer window anywhere on screen
2. Double-click artwork again to switch to Compact mode
3. Left-click artwork to expand to full Expanded Artwork mode
4. Click the collapse triangle (top-right) to return to previous mode
5. Click the right edge of the panel to slide it to the side ("peek" mode)
6. Click the peeking panel to slide it back

### Toolbar Button
1. Right-click the foobar2000 toolbar → Customize...
2. Find "Tray Controls: Launch MiniPlayer" in the available buttons
3. Icon available [here](https://github.com/jame25/foo_traycontrols/blob/main/miniplayer_icon.ico)
4. Add it to your toolbar
5. Click the button to toggle the MiniPlayer (or use View → Tray Controls → Launch MiniPlayer)

### Minimize to Tray
1. Go to Preferences → Tools → Tray Controls
2. Enable "Always minimize to system tray"
3. Click Apply
4. Now clicking the minimize button will hide to tray instead of taskbar

### Popup Notifications
1. Go to Preferences → Tools → Tray Controls
2. Enable "Show popup notification on track change"
3. Configure popup position and duration
4. Click Apply
5. Popup notifications will appear when tracks change

### Custom Fonts
1. Go to Preferences → Tools → Tray Controls
2. Switch to the "Fonts" tab
3. Click "Choose..." next to any font setting to customize
4. Use "Reset All" to restore default fonts

## Requirements

- **foobar2000**: Version 2.0+ (64-bit recommended for dark mode support)
- **Windows**: 7 or later (Windows 10/11 recommended for rounded corners support)
- **Build Tools**: Visual Studio 2022+ with v143 toolset and Windows 10/11 SDK
- **Dependencies**: GDI+ (included with Windows), shlwapi.lib, dwmapi.lib

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

## Support Development

If you find these components useful, consider supporting development:

| Platform | Payment Methods |
|----------|----------------|
| **[Ko-fi](https://ko-fi.com/Jame25)** | Cards, PayPal |
| **[Stripe](https://buy.stripe.com/3cIdR874Bg1NfRdaJf1sQ02)** | Alipay, WeChat Pay, Cards, Apple Pay, Google Pay |

Your support helps cover development time and enables new features. Thank you! 🙏

---

## 支持开发

如果您觉得这些组件有用，请考虑支持开发：

| 平台 | 支付方式 |
|------|----------|
| **[Ko-fi](https://ko-fi.com/Jame25)** | 银行卡、PayPal |
| **[Stripe](https://buy.stripe.com/dRmcN474B8zlfRd2cJ1sQ01)** | 支付宝、微信支付、银行卡、Apple Pay、Google Pay |

您的支持有助于支付开发时间并实现新功能。谢谢！🙏

---

**Feature Requests:** Paid feature requests are available for supporters. [Contact me on Telegram](https://t.me/j4m31) to discuss.

**功能请求：** 为支持者提供付费功能请求。[请在 Telegram 上联系我](https://t.me/j4m31) 进行讨论。
