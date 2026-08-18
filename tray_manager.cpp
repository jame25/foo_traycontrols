// tray_manager.cpp - Implementation of the system tray functionality

#include "stdafx.h"
#include "tray_manager.h"
#include "resource.h"
#include "preferences.h"
#include "popup_window.h"
#include "control_panel.h"
#include "volume_popup.h"

// External declaration from main.cpp
extern HINSTANCE g_hIns;

// Tray icon message constants
const UINT WM_TRAYICON = WM_USER + 1;
const UINT TRAY_ID = 1;

// Menu command IDs
const UINT IDM_PLAY = 1001;
const UINT IDM_PAUSE = 1002;
const UINT IDM_PREV = 1003;
const UINT IDM_NEXT = 1004;
const UINT IDM_RESTORE = 1005;
const UINT IDM_UPDATE_TOOLTIP = 1006;
const UINT IDM_EXIT = 1007;
const UINT IDM_TOGGLE_MINIPLAYER = 1008;
const UINT IDM_SETTINGS = 1009;

// Static instance
tray_manager* tray_manager::s_instance = nullptr;

// Static mouse hook handle
HHOOK tray_manager::s_mouse_hook = nullptr;

tray_manager& tray_manager::get_instance() {
    if (!s_instance) {
        s_instance = new tray_manager();
    }
    return *s_instance;
}

tray_manager::tray_manager()
    : m_main_window(nullptr)
    , m_tray_window(nullptr)
    , m_tray_added(false)
    , m_initialized(false)
    , m_was_visible(true)
    , m_was_minimized(false)
    , m_processing_minimize(false)
    , m_ignore_next_lbuttonup(false)
    , m_last_dblclk_time(0)
    , m_original_wndproc(nullptr)
{
    memset(&m_nid, 0, sizeof(m_nid));
}

tray_manager::~tray_manager() {
    cleanup();
}

void tray_manager::initialize() {
    if (m_initialized) return;

    // Find foobar2000 main window
    m_main_window = find_main_window();

    if (!m_main_window) {
        m_initialized = true;
        return;
    }

    // Create hidden window for tray messages
    if (!create_tray_window()) {
        m_initialized = true;
        return;
    }

    // Set up tray icon
    m_nid.cbSize = sizeof(NOTIFYICONDATA);
    m_nid.hWnd = m_tray_window;  // Use our dedicated window
    m_nid.uID = TRAY_ID;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    // Load the tray icon
    m_nid.hIcon = LoadIcon(g_hIns, MAKEINTRESOURCE(IDI_TRAY_ICON));
    if (!m_nid.hIcon) {
        // Fallback to default application icon
        m_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    }
    wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), L"foobar2000 - Tray Controls", _TRUNCATE);

    // Add tray icon immediately - always visible
    Shell_NotifyIcon(NIM_ADD, &m_nid);
    m_tray_added = true;

    // Install low-level mouse hook for wheel volume control over tray icon
    if (!s_mouse_hook) {
        s_mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, low_level_mouse_proc, g_hIns, 0);
    }

    // Use window subclassing for minimize detection only
    m_original_wndproc = (WNDPROC)SetWindowLongPtr(m_main_window, GWLP_WNDPROC, (LONG_PTR)window_proc);
    
    
    // Store the initial window state
    m_was_visible = IsWindowVisible(m_main_window);
    m_was_minimized = IsIconic(m_main_window);

    // Try to get current playing track for initial tooltip
    try {
        static_api_ptr_t<playback_control> pc;
        if (pc->is_playing()) {
            metadb_handle_ptr track;
            if (pc->get_now_playing(track) && track.is_valid()) {
                update_tooltip(track);
            } else {
                // If no track info available, show playback state
                update_playback_state("Playing");
            }
        } else {
            // Not playing, show stopped state
            update_playback_state("Stopped");
        }
    } catch (...) {
        // Keep default tooltip if anything fails
    }

    // Mouse hook removed - was causing system freezing conflicts with artwork downloading components

    // Initialize popup window and control panel
    popup_window::get_instance().initialize();
    control_panel::get_instance().initialize();
    
    // Start timer to periodically check for track changes and window visibility (every 500ms)
    if (m_tray_window) {
        SetTimer(m_tray_window, TOOLTIP_TIMER_ID, 500, tooltip_timer_proc);
    }

    m_initialized = true;
}

void tray_manager::cleanup() {
    // Cleanup popup window and control panel
    popup_window::get_instance().cleanup();
    control_panel::get_instance().cleanup();

    // Kill the tooltip update timer
    if (m_tray_window) {
        KillTimer(m_tray_window, TOOLTIP_TIMER_ID);
    }

    // Remove low-level mouse hook
    if (s_mouse_hook) {
        UnhookWindowsHookEx(s_mouse_hook);
        s_mouse_hook = nullptr;
    }

    if (m_tray_added) {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        m_tray_added = false;
    }

    if (m_main_window && m_original_wndproc) {
        SetWindowLongPtr(m_main_window, GWLP_WNDPROC, (LONG_PTR)m_original_wndproc);
        m_original_wndproc = nullptr;
    }

    if (m_tray_window) {
        KillTimer(m_tray_window, TRAY_SINGLE_CLICK_TIMER_ID);
        DestroyWindow(m_tray_window);
        m_tray_window = nullptr;
    }

    m_initialized = false;
}

bool tray_manager::create_tray_window() {
    // Register window class
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = tray_window_proc;
    wcex.hInstance = g_hIns;
    wcex.lpszClassName = L"TrayControlsWindow";
    
    RegisterClassEx(&wcex);
    
    // Create hidden window (WS_POPUP without WS_VISIBLE gives a top-level handle for Shell_NotifyIconGetRect)
    m_tray_window = CreateWindowEx(
        0,
        L"TrayControlsWindow",
        L"Tray Controls",
        WS_POPUP,
        0, 0, 0, 0,
        nullptr,
        nullptr,
        g_hIns,
        nullptr);
        
    return m_tray_window != nullptr;
}

static bool is_remote_stream_path(const char* path) {
    if (!path || path[0] == '\0') return false;
    const char* proto = strstr(path, "://");
    if (!proto) return false;
    if (strncmp(path, "file://", 7) == 0 || strstr(path, "file://") != nullptr) return false;
    return true;
}

void tray_manager::update_tooltip(metadb_handle_ptr p_track) {
    if (!m_initialized || !p_track.is_valid()) {
        wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), L"foobar2000 - No Track", _TRUNCATE);
        if (m_tray_added) {
            m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
        return;
    }
    
    try {
        pfc::string8 path = p_track->get_path();
        bool is_stream = is_remote_stream_path(path.get_ptr());

        if (p_track != m_last_loaded_track) {
            m_last_loaded_track = p_track;
            if (!is_stream) {
                m_last_stream_artist = "";
                m_last_stream_title = "";
            }
        }

        pfc::string8 tooltip;
        if (is_stream && (!m_last_stream_title.is_empty() || !m_last_stream_artist.is_empty())) {
            if (!m_last_stream_artist.is_empty() && !m_last_stream_title.is_empty()) {
                tooltip = m_last_stream_artist;
                tooltip += " - ";
                tooltip += m_last_stream_title;
            } else if (!m_last_stream_title.is_empty()) {
                tooltip = m_last_stream_title;
            } else {
                tooltip = m_last_stream_artist;
            }
        } else {
            pfc::string8 line1, line2;
            format_display_lines_track(p_track, line1, line2);

            if (!line1.is_empty() && !line2.is_empty()) {
                tooltip = line1;
                tooltip += " - ";
                tooltip += line2;
            } else if (!line1.is_empty()) {
                tooltip = line1;
            } else if (!line2.is_empty()) {
                tooltip = line2;
            } else {
                tooltip = "foobar2000 - Playing";
            }
        }

        if (tooltip == m_last_track_metadata && !tooltip.is_empty()) {
            return;
        }
        m_last_track_metadata = tooltip;

        pfc::stringcvt::string_wide_from_utf8 wide_tooltip(tooltip.get_ptr());
        wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), wide_tooltip.get_ptr(), _TRUNCATE);
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        
        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
    }
    catch (...) {
        wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), L"foobar2000 - Error", _TRUNCATE);
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
    }
}

void tray_manager::update_tooltip_from_playback() {
    if (!m_initialized) return;
    try {
        pfc::string8 line1, line2, tooltip;
        format_display_lines(line1, line2);

        if (!line1.is_empty() && !line2.is_empty()) {
            tooltip = line1;
            tooltip += " - ";
            tooltip += line2;
        } else if (!line1.is_empty()) {
            tooltip = line1;
        } else if (!line2.is_empty()) {
            tooltip = line2;
        } else {
            tooltip = "foobar2000 - Playing";
        }

        if (tooltip == m_last_track_metadata && !tooltip.is_empty()) {
            return;
        }
        m_last_track_metadata = tooltip;

        pfc::stringcvt::string_wide_from_utf8 wide_tooltip(tooltip.get_ptr());
        wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), wide_tooltip.get_ptr(), _TRUNCATE);
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;

        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
    } catch (...) {}
}

static const char* safe_meta_get_tray(const file_info& info, const char* name) {
    t_size index = info.meta_find(name);
    if (index != pfc_infinite && info.meta_enum_value_count(index) > 0) {
        const char* val = info.meta_enum_value(index, 0);
        if (val && val[0] != '\0') return val;
    }
    return nullptr;
}

static bool is_inverted_stream_tray(const file_info& info, metadb_handle_ptr track = nullptr) {
    try {
        if (track.is_valid()) {
            pfc::string8 path = track->get_path();
            if (!path.is_empty()) {
                std::string path_str = path.c_str();
                std::transform(path_str.begin(), path_str.end(), path_str.begin(), ::tolower);
                if (path_str.find("?inverted") != std::string::npos ||
                    path_str.find("&inverted") != std::string::npos ||
                    path_str.find("#inverted") != std::string::npos) {
                    return true;
                }
            }
        }
        
        t_size index = info.meta_find("STREAM_INVERTED");
        if (index != pfc_infinite && info.meta_enum_value_count(index) > 0) {
            const char* val = info.meta_enum_value(index, 0);
            if (val && strcmp(val, "1") == 0) return true;
        }
    } catch (...) {}
    return false;
}

void tray_manager::update_tooltip_with_dynamic_info(const file_info & p_info) {
    if (!m_initialized) return;
    
    try {
        pfc::string8 artist, title;

        const char* p_artist = safe_meta_get_tray(p_info, "ARTIST");
        const char* p_title = safe_meta_get_tray(p_info, "TITLE");

        const char* stream_title = safe_meta_get_tray(p_info, "STREAMTITLE");
        if (!stream_title) {
            stream_title = safe_meta_get_tray(p_info, "ICY_TITLE");
        }

        if (stream_title) {
            const char* dash = strstr(stream_title, " - ");
            if (dash) {
                if (!p_artist) {
                    artist.set_string(stream_title, dash - stream_title);
                }
                if (!p_title) {
                    title.set_string(dash + 3);
                }
            } else if (!p_title) {
                title = stream_title;
            }
        }

        if (p_artist && artist.is_empty()) artist = p_artist;
        if (p_title && title.is_empty()) title = p_title;

        if (artist.is_empty() && !title.is_empty()) {
            pfc::string8 temp = title;
            const char* dash = strstr(temp.get_ptr(), " - ");
            if (dash) {
                artist.set_string(temp.get_ptr(), dash - temp.get_ptr());
                title = dash + 3;
            }
        }

        if (artist.is_empty()) {
            const char* val = safe_meta_get_tray(p_info, "ALBUMARTIST");
            if (val) artist = val;
        }
        if (artist.is_empty()) {
            const char* val = safe_meta_get_tray(p_info, "PERFORMER");
            if (val) artist = val;
        }
        if (title.is_empty()) {
            const char* val = safe_meta_get_tray(p_info, "DESCRIPTION");
            if (val) title = val;
        }
        if (title.is_empty()) {
            const char* val = safe_meta_get_tray(p_info, "COMMENT");
            if (val) title = val;
        }
        
        if (artist.is_empty() && title.is_empty()) {
            return;
        }

        artist.trim(' ');
        title.trim(' ');

        if (is_inverted_stream_tray(p_info, m_last_loaded_track)) {
            pfc::string8 temp = artist;
            artist = title;
            title = temp;
        }

        m_last_stream_artist = artist;
        m_last_stream_title = title;
        
        pfc::string8 tooltip;
        if (!artist.is_empty() && !title.is_empty()) {
            tooltip = artist;
            tooltip += " - ";
            tooltip += title;
        } else if (!title.is_empty()) {
            tooltip = title;
        } else {
            tooltip = artist;
        }

        if (tooltip == m_last_track_metadata && !tooltip.is_empty()) {
            return;
        }
        m_last_track_metadata = tooltip;
        
        // Convert to wide string and update tooltip
        pfc::stringcvt::string_wide_from_utf8 wide_tooltip(tooltip.get_ptr());
        wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), wide_tooltip.get_ptr(), _TRUNCATE);
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        
        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
    }
    catch (...) {
        // Fallback tooltip
        wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), L"foobar2000 - Playing", _TRUNCATE);
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
    }
}

void tray_manager::update_playback_state(const char* state) {
    if (!m_initialized) return;
    
    if (strcmp(state, "Playing") == 0) {
        if (!m_last_track_metadata.is_empty()) {
            pfc::stringcvt::string_wide_from_utf8 wide_tooltip(m_last_track_metadata.get_ptr());
            wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), wide_tooltip.get_ptr(), _TRUNCATE);
            m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            if (m_tray_added) Shell_NotifyIcon(NIM_MODIFY, &m_nid);
            return;
        }
    } else if (strcmp(state, "Stopped") == 0) {
        m_last_track_metadata = "";
        m_last_stream_artist = "";
        m_last_stream_title = "";
        m_last_loaded_track = nullptr;
    }

    pfc::string8 tooltip = "foobar2000 - ";
    tooltip += state;
    
    pfc::stringcvt::string_wide_from_utf8 wide_tooltip(tooltip.get_ptr());
    wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), wide_tooltip.get_ptr(), _TRUNCATE);
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    
    if (m_tray_added) {
        Shell_NotifyIcon(NIM_MODIFY, &m_nid);
    }
}

HWND tray_manager::find_main_window() {
    HWND result = nullptr;
    
    // First try direct window title search
    result = FindWindow(nullptr, L"foobar2000");
    if (result && IsWindowVisible(result) && !GetParent(result)) {
        return result;
    }
    
    // Enumerate all windows to find foobar2000
    EnumWindows(find_window_callback, (LPARAM)&result);
    return result;
}

BOOL CALLBACK tray_manager::find_window_callback(HWND hwnd, LPARAM lparam) {
    HWND* result = (HWND*)lparam;
    
    wchar_t title[256];
    wchar_t class_name[256];
    
    if (GetWindowText(hwnd, title, sizeof(title) / sizeof(wchar_t)) &&
        GetClassName(hwnd, class_name, sizeof(class_name) / sizeof(wchar_t))) {
        
        // Skip dialog windows
        if (wcscmp(class_name, L"#32770") == 0) {
            return TRUE;
        }
        
        // Look for foobar2000 main window
        if ((wcsstr(title, L"foobar2000") && !wcsstr(title, L"crashed")) ||
            wcscmp(class_name, L"{E7076D1C-A7BF-4f39-B771-BCBE88F2A2A8}") == 0) {
            
            if (IsWindowVisible(hwnd) && !GetParent(hwnd)) {
                *result = hwnd;
                return FALSE;
            }
        }
    }
    return TRUE;
}

void tray_manager::minimize_to_tray() {
    if (m_main_window && m_initialized) {
        // NOTE: The control panel / MiniPlayer is intentionally NOT hidden here.
        // Double-clicking the tray icon (or minimizing the main window) must never
        // close the MiniPlayer; it stays open in whatever mode it was in.

        ShowWindow(m_main_window, SW_HIDE);
        // Tray icon is already added, just update tooltip if needed
        m_was_visible = false;
    }
}

void tray_manager::restore_from_tray() {
    if (m_main_window && m_initialized) {
        // NOTE: The control panel / MiniPlayer is intentionally NOT hidden here so
        // double-clicking the tray icon never closes the MiniPlayer.

        if (IsIconic(m_main_window)) {
            ShowWindow(m_main_window, SW_RESTORE);
        } else {
            ShowWindow(m_main_window, SW_SHOW);
        }
        SetForegroundWindow(m_main_window);
        // Keep tray icon visible, just update state
        m_was_visible = true;
    }
}

void tray_manager::on_settings_changed() {
    // Mouse hook removed - no longer needed
    
    // Update popup window settings
    popup_window::get_instance().on_settings_changed();
    
    // Refresh tray icon tooltip with updated title formatting
    force_update_tooltip();
}

void tray_manager::show_context_menu(int x, int y) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    
    // Show/Hide foobar2000 main window menu item
    bool main_window_visible = m_main_window && IsWindowVisible(m_main_window);
    AppendMenu(menu, MF_STRING, IDM_RESTORE, main_window_visible ? L"Hide foobar2000" : L"Show foobar2000");
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);

    // Show MiniPlayer toggle menu item
    auto& panel = control_panel::get_instance();
    bool miniplayer_visible = panel.get_control_window() && IsWindowVisible(panel.get_control_window()) &&
                              (panel.is_undocked() || panel.is_artwork_expanded() || panel.is_compact_mode());
    AppendMenu(menu, MF_STRING, IDM_TOGGLE_MINIPLAYER, miniplayer_visible ? L"Close MiniPlayer" : L"Open MiniPlayer");
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(menu, MF_STRING, IDM_SETTINGS, L"Settings");
    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(menu, MF_STRING, IDM_EXIT, L"Exit");
    
    // Ensure the menu appears in front
    SetForegroundWindow(m_main_window);
    int cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, x, y, 0, m_main_window, nullptr);
    PostMessage(m_main_window, WM_NULL, 0, 0); // Required for proper menu dismissal
    
    if (cmd > 0) {
        handle_menu_command(cmd);
    }
    DestroyMenu(menu);
}

void tray_manager::handle_menu_command(int cmd) {
    switch (cmd) {
    case IDM_TOGGLE_MINIPLAYER:
        {
            // Toggle MiniPlayer visibility
            auto& panel = control_panel::get_instance();
            bool miniplayer_visible = panel.get_control_window() && IsWindowVisible(panel.get_control_window()) &&
                                      (panel.is_undocked() || panel.is_artwork_expanded() || panel.is_compact_mode());
            if (miniplayer_visible) {
                // Close the MiniPlayer and save state for later restoration via menu
                panel.hide_and_remember_miniplayer();
            } else {
                // Open the MiniPlayer (restores saved position/mode if available)
                panel.show_undocked_miniplayer();
            }
        }
        break;
        
    case IDM_RESTORE:
        // Toggle window visibility
        if (IsWindowVisible(m_main_window)) {
            minimize_to_tray();
        } else {
            restore_from_tray();
        }
        break;
        
    case IDM_SETTINGS:
        show_preferences_page();
        break;

    case IDM_EXIT:
        if (m_main_window) {
            PostMessage(m_main_window, WM_CLOSE, 0, 0);
        }
        break;
    }
}

void tray_manager::force_update_tooltip() {
    if (!m_initialized) return;
    
    try {
        static_api_ptr_t<playback_control> pc;
        
        // Try multiple approaches to get current track info
        if (pc->is_playing()) {
            metadb_handle_ptr track;
            if (pc->get_now_playing(track) && track.is_valid()) {
                // Method 1: Direct track info update
                update_tooltip(track);
                return;
            }
        }
        
        // Method 2: Get playback state and show that
        if (pc->is_playing()) {
            if (pc->is_paused()) {
                update_playback_state("Paused");
            } else {
                update_playback_state("Playing");
            }
        } else {
            update_playback_state("Stopped");
        }
        
        // Method 3: Show debug info
        pfc::string8 debug_info = "Debug: Playing=";
        debug_info += pc->is_playing() ? "true" : "false";
        debug_info += ", Paused=";
        debug_info += pc->is_paused() ? "true" : "false";
        
        pfc::stringcvt::string_wide_from_utf8 wide_debug(debug_info.get_ptr());
        wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), wide_debug.get_ptr(), _TRUNCATE);
        
        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
        
    } catch (...) {
        wcsncpy_s(m_nid.szTip, _countof(m_nid.szTip), L"Debug: Exception occurred", _TRUNCATE);
        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
    }
}

LRESULT CALLBACK tray_manager::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    
    if (s_instance && s_instance->m_initialized) {
        switch (msg) {
        case WM_SIZE:
            if (wparam == SIZE_MINIMIZED) {
                s_instance->minimize_to_tray();
                return 0;
            }
            break;
            
        case WM_SYSCOMMAND:
            if (wparam == SC_MINIMIZE) {
                // Check if "always minimize to tray" is enabled
                bool minimize_setting = get_always_minimize_to_tray();
                if (minimize_setting) {
                    s_instance->minimize_to_tray();
                    return 0;  // Prevent default minimize behavior
                }
                // Otherwise let the default processing happen, then we'll catch it in WM_SIZE
                break;
            }
            break;
            
        }
    }
    
    if (s_instance && s_instance->m_original_wndproc) {
        return CallWindowProc(s_instance->m_original_wndproc, hwnd, msg, wparam, lparam);
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void tray_manager::execute_single_click() {
    auto& panel = control_panel::get_instance();
    bool is_visible = panel.get_control_window() && IsWindowVisible(panel.get_control_window());
    bool is_miniplayer = panel.is_undocked() || panel.is_artwork_expanded() || panel.is_compact_mode();

    if (is_visible && is_miniplayer) {
        // MiniPlayer is visible.
        // - If it is slid to the side, slide it back out.
        // - Otherwise, if "Always Slide-to-Side" is enabled in Preferences, slide it to the side.
        // - Otherwise, do nothing (single-click never closes or hides the MiniPlayer).
        if (panel.is_slid_to_side()) {
            panel.slide_back_from_side();
        } else if (get_always_slide_to_side()) {
            panel.slide_to_side();
        }
    } else if (is_visible) {
        // Docked panel is visible - hide it
        panel.hide_control_panel_immediate();
    } else {
        // Nothing visible - always show docked panel (ignore saved MiniPlayer state)
        // User can restore MiniPlayer via the tray menu "Open MiniPlayer" option
        panel.show_control_panel_simple();
    }
}

VOID CALLBACK tray_manager::single_click_timer_proc(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time) {
    KillTimer(hwnd, TRAY_SINGLE_CLICK_TIMER_ID);
    if (s_instance && s_instance->m_initialized) {
        s_instance->execute_single_click();
    }
}

// Dedicated window procedure for tray messages
LRESULT CALLBACK tray_manager::tray_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (s_instance && s_instance->m_initialized) {
        switch (msg) {
        case WM_TRAYICON: // Tray icon message
            switch (LOWORD(lparam)) {
            case WM_RBUTTONUP:
            case WM_CONTEXTMENU:
                {
                    KillTimer(hwnd, TRAY_SINGLE_CLICK_TIMER_ID);
                    s_instance->m_ignore_next_lbuttonup = false;
                    POINT pt;
                    GetCursorPos(&pt);
                    s_instance->show_context_menu(pt.x, pt.y);
                }
                return 0;
                
            case WM_LBUTTONUP:
            case NIN_SELECT:
            case NIN_KEYSELECT:
                if (s_instance->m_last_dblclk_time != 0 && (GetTickCount() - s_instance->m_last_dblclk_time) < 400) {
                    return 0; // Suppress trailing single-click messages following double-click (Windows 11 Shell)
                }
                if (s_instance->m_ignore_next_lbuttonup) {
                    s_instance->m_ignore_next_lbuttonup = false;
                    return 0;
                }
                // Defer single-click action with a fast 100ms TIMERPROC callback so single-click opens instantly (~100ms) without delay or double-click flash
                SetTimer(hwnd, TRAY_SINGLE_CLICK_TIMER_ID, 100, single_click_timer_proc);
                return 0;
                
            case WM_LBUTTONDBLCLK:
                // Cancel pending single-click timer and set flag to ignore the trailing WM_LBUTTONUP from the second click
                KillTimer(hwnd, TRAY_SINGLE_CLICK_TIMER_ID);
                s_instance->m_ignore_next_lbuttonup = true;
                s_instance->m_last_dblclk_time = GetTickCount();
                // Toggle foobar2000 main window visibility on double-click IF enabled in Preferences
                if (get_double_click_actions()) {
                    if (s_instance->m_main_window && IsWindowVisible(s_instance->m_main_window)) {
                        s_instance->minimize_to_tray();
                    } else {
                        s_instance->restore_from_tray();
                    }
                }
                return 0;
                
            }
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

bool tray_manager::is_cursor_over_tray_icon() {
    if (!m_initialized || !m_tray_added) return false;

    // Get cursor position
    POINT cursor_pos;
    if (!GetCursorPos(&cursor_pos)) return false;

    // Get the exact bounding rectangle of THIS app's tray icon
    NOTIFYICONIDENTIFIER nii = {};
    nii.cbSize = sizeof(nii);
    nii.hWnd = m_nid.hWnd;
    nii.uID = m_nid.uID;

    RECT icon_rect = {};
    HRESULT hr = Shell_NotifyIconGetRect(&nii, &icon_rect);
    if (SUCCEEDED(hr)) {
        // Add padding to icon bounds for DPI / cursor rounding tolerances
        InflateRect(&icon_rect, 4, 4);

        if (cursor_pos.x >= icon_rect.left && cursor_pos.x <= icon_rect.right &&
            cursor_pos.y >= icon_rect.top && cursor_pos.y <= icon_rect.bottom) {
            return true;
        }
    }

    // NOTE: No broad notification-area fallback here. Accepting the whole tray
    // (ToolbarWindow32 / TrayNotifyWnd / SysPager / Shell_TrayWnd) would make the
    // volume OSD trigger when scrolling over OTHER tray icons or empty tray space.
    // Only THIS app's exact icon rectangle (via Shell_NotifyIconGetRect) qualifies.

    return false;
}

// Low-level mouse hook for wheel volume control over tray icon
LRESULT CALLBACK tray_manager::low_level_mouse_proc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_MOUSEWHEEL && s_instance && s_instance->m_initialized) {
        MSLLHOOKSTRUCT* hookData = (MSLLHOOKSTRUCT*)lParam;

        // Check if cursor is over the notification area (system tray)
        if (s_instance->is_cursor_over_tray_icon()) {
            // Get wheel delta from hook data (HIWORD of mouseData)
            short wheelDelta = HIWORD(hookData->mouseData);

            try {
                // Adjust volume with mouse wheel using position-based stepping
                // matching Now Bar Control Panel volume controls (position_step = 20/1000).
                static_api_ptr_t<playback_control> pc;
                constexpr float position_step = 20.0f / 1000.0f;
                float current_db = pc->get_volume();

                // Convert dB to slider position (0.0 to 1.0): position = 2^(dB/10)
                double current_pos = std::pow(2.0, static_cast<double>(current_db) / 10.0);
                if (current_pos < 0.0) current_pos = 0.0;
                if (current_pos > 1.0) current_pos = 1.0;

                double new_pos = current_pos + ((wheelDelta > 0) ? position_step : -position_step);
                if (new_pos < 0.0) new_pos = 0.0;
                if (new_pos > 1.0) new_pos = 1.0;

                // Convert slider position to dB: dB = 10 * log2(position)
                float new_volume_db = -100.0f;
                if (new_pos > 0.0) {
                    new_volume_db = static_cast<float>(10.0 * std::log2(new_pos));
                    if (new_volume_db < -100.0f) new_volume_db = -100.0f;
                    if (new_volume_db > 0.0f) new_volume_db = 0.0f;
                }

                pc->set_volume(new_volume_db);

                if (get_show_volume_feedback()) {
                    volume_popup::get_instance().show_feedback();
                }
            } catch (...) {
                // Ignore volume control errors
            }



            // Consume the message so system volume or other windows (e.g. Explorer) do not process the scroll
            return 1;
        }
    }

    return CallNextHookEx(s_mouse_hook, nCode, wParam, lParam);
}

// Timer procedure for periodic tooltip updates and window monitoring
VOID CALLBACK tray_manager::tooltip_timer_proc(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time) {
    if (s_instance && timer_id == TOOLTIP_TIMER_ID && s_instance->m_initialized) {
        s_instance->check_window_visibility();
    }
}

// Check if the current track has changed and update tooltip accordingly
void tray_manager::check_for_track_changes() {
    // No-op: Playback callbacks in main.cpp (tray_play_callback) handle all track and playback state changes
    // event-driven from foobar2000, eliminating periodic playback_control polling on the main UI thread.
}

// Check for window visibility changes and handle minimize behavior
void tray_manager::check_window_visibility() {
    if (!m_initialized || !m_main_window || m_processing_minimize) return;

    bool current_visible = IsWindowVisible(m_main_window);
    bool is_minimized = IsIconic(m_main_window);

    // Only trigger on actual state changes
    if (current_visible != m_was_visible || is_minimized != m_was_minimized) {

        // Check if user just minimized the window and setting is enabled
        if (!m_was_minimized && is_minimized && get_always_minimize_to_tray()) {
            m_processing_minimize = true;

            // Hide the window to tray
            ShowWindow(m_main_window, SW_HIDE);

            m_processing_minimize = false;
        }


        // Update stored state
        m_was_visible = current_visible;
        m_was_minimized = is_minimized;
    }
}

