// tray_manager.cpp - Implementation of the system tray functionality

#include "stdafx.h"
#include "tray_manager.h"
#include "resource.h"
#include "preferences.h"
#include "popup_window.h"
#include "control_panel.h"

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
    wcscpy_s(m_nid.szTip, L"foobar2000 - Tray Controls");

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
    
    // Create hidden window
    m_tray_window = CreateWindowEx(
        0,
        L"TrayControlsWindow",
        L"Tray Controls",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,  // Message-only window
        nullptr,
        g_hIns,
        nullptr);
        
    return m_tray_window != nullptr;
}

void tray_manager::update_tooltip(metadb_handle_ptr p_track) {
    if (!m_initialized || !p_track.is_valid()) {
        // Default tooltip if no valid track
        wcscpy_s(m_nid.szTip, L"foobar2000 - No Track");
        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
        return;
    }
    
    try {
        // Get track info
        file_info_impl info;
        p_track->get_info(info);
        
        pfc::string8 path = p_track->get_path();
        bool is_stream = strstr(path.get_ptr(), "://") != nullptr;
        
        // Build tooltip string with artist and title
        pfc::string8 artist, title, tooltip;
        
        // Check for standard metadata first
        if (info.meta_exists("ARTIST")) {
            artist = info.meta_get("ARTIST", 0);
        }
        if (info.meta_exists("TITLE")) {
            title = info.meta_get("TITLE", 0);
        }
        
        // For streaming sources, try multiple approaches
        if (is_stream) { // This is a stream
            // First, try to get current track metadata using titleformat (this should get what foobar2000 displays)
            try {
                static_api_ptr_t<playback_control> pc;
                static_api_ptr_t<titleformat_compiler> compiler;
                service_ptr_t<titleformat_object> script;
                
                // Try to get the formatted title that foobar2000 is currently displaying
                if (compiler->compile(script, "[%artist%]|[%title%]")) {
                    pfc::string8 formatted_title;
                    if (pc->playback_format_title(nullptr, formatted_title, script, nullptr, playback_control::display_level_all)) {
                        const char* separator = strstr(formatted_title.get_ptr(), "|");
                        if (separator && strlen(formatted_title.get_ptr()) > 1) { // Make sure we have actual content
                            pfc::string8 tf_artist(formatted_title.get_ptr(), separator - formatted_title.get_ptr());
                            pfc::string8 tf_title(separator + 1);
                            
                            // Only use titleformat result if it has meaningful content
                            if (!tf_artist.is_empty() && !tf_title.is_empty()) {
                                artist = tf_artist;
                                title = tf_title;
                            }
                        }
                    }
                }
            } catch (...) {
                // Ignore titleformat errors
            }
            
            // If titleformat didn't work, try alternative metadata fields
            if (artist.is_empty() && info.meta_exists("ALBUMARTIST")) {
                artist = info.meta_get("ALBUMARTIST", 0);
            }
            if (artist.is_empty() && info.meta_exists("PERFORMER")) {
                artist = info.meta_get("PERFORMER", 0);
            }
            
            // Check for ICY metadata (common in internet radio)
            if (title.is_empty() && info.meta_exists("STREAMTITLE")) {
                title = info.meta_get("STREAMTITLE", 0);
            }
            if (title.is_empty() && info.meta_exists("ICY_TITLE")) {
                title = info.meta_get("ICY_TITLE", 0);
            }
            
            // Try some other common streaming metadata fields
            if (title.is_empty() && info.meta_exists("DESCRIPTION")) {
                title = info.meta_get("DESCRIPTION", 0);
            }
            if (title.is_empty() && info.meta_exists("COMMENT")) {
                title = info.meta_get("COMMENT", 0);
            }
            
            // For radio streams, use server name as last resort
            if (artist.is_empty() && title.is_empty()) {
                if (info.meta_exists("SERVER")) {
                    title = info.meta_get("SERVER", 0);
                }
                if (title.is_empty() && info.meta_exists("server")) {
                    title = info.meta_get("server", 0);
                }
            }
            
            // If still no metadata, try to get it from the filename/URL
            if (artist.is_empty() && title.is_empty()) {
                // Extract station name from URL if possible
                const char* url_title = path.get_ptr();
                const char* last_slash = strrchr(url_title, '/');
                if (last_slash && last_slash[1]) {
                    pfc::string8 url_part = last_slash + 1;
                    // Don't show just the codec name, show something more meaningful
                    if (strcmp(url_part.get_ptr(), "aac") == 0 || 
                        strcmp(url_part.get_ptr(), "mp3") == 0 || 
                        strcmp(url_part.get_ptr(), "ogg") == 0) {
                        title = "Internet Radio Stream";
                    } else {
                        title = url_part;
                    }
                } else {
                    title = "Internet Radio Stream";
                }
            }
        }
        
        if (!artist.is_empty() && !title.is_empty()) {
            tooltip = artist;
            tooltip += " - ";
            tooltip += title;
        } else if (!title.is_empty()) {
            tooltip = title;
        } else {
            // Use filename/URL if no metadata
            tooltip = p_track->get_path();
            const char* filename = strrchr(tooltip.get_ptr(), '\\');
            if (filename) {
                tooltip = filename + 1;
            } else {
                const char* url_filename = strrchr(tooltip.get_ptr(), '/');
                if (url_filename) {
                    tooltip = url_filename + 1;
                } else {
                    tooltip = "Unknown Track";
                }
            }
        }
        
        // Ensure we have some text
        if (tooltip.is_empty()) {
            tooltip = "foobar2000 - Playing";
        }
        
        // Convert to wide string and update tooltip
        pfc::stringcvt::string_wide_from_utf8 wide_tooltip(tooltip.get_ptr());
        wcscpy_s(m_nid.szTip, wide_tooltip.get_ptr());
        
        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
        
        // Popup will be triggered by playback callbacks for actual track changes
    }
    catch (...) {
        // Fallback tooltip
        wcscpy_s(m_nid.szTip, L"foobar2000 - Error");
        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
    }
}

void tray_manager::update_tooltip_with_dynamic_info(const file_info & p_info) {
    if (!m_initialized) return;
    
    try {
        // Build tooltip string with dynamic info
        pfc::string8 artist, title, tooltip;
        
        // Check for metadata in dynamic info
        if (p_info.meta_exists("ARTIST")) {
            artist = p_info.meta_get("ARTIST", 0);
        }
        if (p_info.meta_exists("TITLE")) {
            title = p_info.meta_get("TITLE", 0);
        }
        
        // Check for streaming metadata
        if (title.is_empty() && p_info.meta_exists("STREAMTITLE")) {
            title = p_info.meta_get("STREAMTITLE", 0);
        }
        if (title.is_empty() && p_info.meta_exists("ICY_TITLE")) {
            title = p_info.meta_get("ICY_TITLE", 0);
        }
        
        // Check for alternative artist fields
        if (artist.is_empty() && p_info.meta_exists("ALBUMARTIST")) {
            artist = p_info.meta_get("ALBUMARTIST", 0);
        }
        if (artist.is_empty() && p_info.meta_exists("PERFORMER")) {
            artist = p_info.meta_get("PERFORMER", 0);
        }
        
        // Try additional dynamic metadata fields
        if (title.is_empty() && p_info.meta_exists("DESCRIPTION")) {
            title = p_info.meta_get("DESCRIPTION", 0);
        }
        if (title.is_empty() && p_info.meta_exists("COMMENT")) {
            title = p_info.meta_get("COMMENT", 0);
        }
        
        if (!artist.is_empty() && !title.is_empty()) {
            tooltip = artist;
            tooltip += " - ";
            tooltip += title;
        } else if (!title.is_empty()) {
            tooltip = title;
        } else {
            // If no useful dynamic metadata found, try to get the current track and force an update
            try {
                static_api_ptr_t<playback_control> pc;
                metadb_handle_ptr track;
                if (pc->get_now_playing(track) && track.is_valid()) {
                    update_tooltip(track);
                }
            } catch (...) {
                // Ignore errors
            }
            return;
        }
        
        // Convert to wide string and update tooltip
        pfc::stringcvt::string_wide_from_utf8 wide_tooltip(tooltip.get_ptr());
        wcscpy_s(m_nid.szTip, wide_tooltip.get_ptr());
        
        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
    }
    catch (...) {
        // Fallback tooltip
        wcscpy_s(m_nid.szTip, L"foobar2000 - Playing");
        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
    }
}

void tray_manager::update_playback_state(const char* state) {
    if (!m_initialized) return;
    
    // Update tooltip with playback state
    pfc::string8 tooltip = "foobar2000 - ";
    tooltip += state;
    
    pfc::stringcvt::string_wide_from_utf8 wide_tooltip(tooltip.get_ptr());
    wcscpy_s(m_nid.szTip, wide_tooltip.get_ptr());
    
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
        ShowWindow(m_main_window, SW_HIDE);
        // Tray icon is already added, just update tooltip if needed
        m_was_visible = false;
    }
}

void tray_manager::restore_from_tray() {
    if (m_main_window && m_initialized) {
        ShowWindow(m_main_window, SW_RESTORE);
        SetForegroundWindow(m_main_window);
        // Keep tray icon visible, just update state
        m_was_visible = true;
    }
}

void tray_manager::on_settings_changed() {
    // Mouse hook removed - no longer needed
    
    // Update popup window settings
    popup_window::get_instance().on_settings_changed();
}

void tray_manager::show_context_menu(int x, int y) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    
    // Show appropriate menu item based on window visibility
    bool is_visible = IsWindowVisible(m_main_window);
    AppendMenu(menu, MF_STRING, IDM_RESTORE, is_visible ? L"Hide foobar2000" : L"Show foobar2000");
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
    case IDM_RESTORE:
        // Toggle window visibility
        if (IsWindowVisible(m_main_window)) {
            minimize_to_tray();
        } else {
            restore_from_tray();
        }
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
        wcscpy_s(m_nid.szTip, wide_debug.get_ptr());
        
        if (m_tray_added) {
            Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }
        
    } catch (...) {
        wcscpy_s(m_nid.szTip, L"Debug: Exception occurred");
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

// Dedicated window procedure for tray messages
LRESULT CALLBACK tray_manager::tray_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (s_instance && s_instance->m_initialized) {
        switch (msg) {
        case WM_TRAYICON: // Tray icon message
            switch (LOWORD(lparam)) {
            case WM_RBUTTONUP:
            case WM_CONTEXTMENU:
                {
                    POINT pt;
                    GetCursorPos(&pt);
                    s_instance->show_context_menu(pt.x, pt.y);
                }
                return 0;
                
            case WM_LBUTTONUP:
                // Single-click behavior for tray icon
                {
                    auto& panel = control_panel::get_instance();
                    bool is_visible = panel.get_control_window() && IsWindowVisible(panel.get_control_window());
                    bool is_miniplayer = panel.is_undocked() || panel.is_artwork_expanded() || panel.is_compact_mode();

                    if (is_visible && is_miniplayer) {
                        // Check if MiniPlayer is slid to side - if so, slide it back
                        if (panel.is_slid_to_side()) {
                            panel.slide_back_from_side();
                        }
                        // If "Always Slide-to-Side" is enabled, slide instead of hiding
                        else if (get_always_slide_to_side()) {
                            panel.slide_to_side();
                        } else {
                            // Miniplayer (any non-docked mode) is visible - hide it and remember state/position
                            panel.hide_and_remember_miniplayer();
                        }
                    } else if (panel.has_saved_miniplayer_state()) {
                        // Was in a miniplayer mode before - restore it at saved position
                        panel.show_miniplayer_at_saved_position();
                    } else if (is_visible) {
                        // Docked panel is visible - hide it
                        panel.hide_control_panel_immediate();
                    } else {
                        // Nothing visible, no saved state - show docked panel
                        panel.show_control_panel_simple();
                    }
                }
                return 0;
                
            case WM_LBUTTONDBLCLK:
                // Double-click functionality removed - no action
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
    
    // Get tray area bounds
    RECT tray_rect;
    HWND tray_wnd = FindWindow(L"Shell_TrayWnd", nullptr);
    if (!tray_wnd) return false;
    
    HWND notification_area = FindWindowEx(tray_wnd, nullptr, L"TrayNotifyWnd", nullptr);
    if (!notification_area) return false;
    
    if (!GetWindowRect(notification_area, &tray_rect)) return false;
    
    // Check if cursor is within tray area (with some tolerance)
    return (cursor_pos.x >= tray_rect.left && cursor_pos.x <= tray_rect.right &&
            cursor_pos.y >= tray_rect.top && cursor_pos.y <= tray_rect.bottom);
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
                static_api_ptr_t<playback_control> pc;
                float current_volume = pc->get_volume();
                // Volume is in dB, typically -100 to 0
                // Adjust by 2 dB per wheel notch (120 units = 1 notch)
                float volume_change = (wheelDelta > 0) ? 2.0f : -2.0f;
                float new_volume = current_volume + volume_change;
                // Clamp to valid range
                if (new_volume > 0.0f) new_volume = 0.0f;
                if (new_volume < -100.0f) new_volume = -100.0f;
                pc->set_volume(new_volume);
            } catch (...) {
                // Ignore volume control errors
            }

            // Don't consume the message - let other apps handle it too
            // return 1; // Uncomment to consume the message
        }
    }

    return CallNextHookEx(s_mouse_hook, nCode, wParam, lParam);
}

// Timer procedure for periodic tooltip updates and window monitoring
VOID CALLBACK tray_manager::tooltip_timer_proc(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time) {
    if (s_instance && timer_id == TOOLTIP_TIMER_ID && s_instance->m_initialized) {
        s_instance->check_for_track_changes();
        s_instance->check_window_visibility();
    }
}

// Check if the current track has changed and update tooltip accordingly
void tray_manager::check_for_track_changes() {
    if (!m_initialized) return;
    
    try {
        static_api_ptr_t<playback_control> pc;
        
        if (pc->is_playing()) {
            metadb_handle_ptr track;
            if (pc->get_now_playing(track) && track.is_valid()) {
                pfc::string8 current_path = track->get_path();
                
                bool is_stream = strstr(current_path.get_ptr(), "://") != nullptr;
                
                if (is_stream) {
                    // For streams, use metadata as identifier for track changes
                    pfc::string8 metadata_identifier;
                    try {
                        static_api_ptr_t<titleformat_compiler> compiler;
                        service_ptr_t<titleformat_object> script;
                        
                        if (compiler->compile(script, "[%artist%]|[%title%]")) {
                            pfc::string8 formatted_title;
                            if (pc->playback_format_title(nullptr, formatted_title, script, nullptr, playback_control::display_level_all)) {
                                metadata_identifier = formatted_title;
                            }
                        }
                    } catch (...) {
                        metadata_identifier = current_path;
                    }
                    
                    // Check if metadata has actually changed
                    if (metadata_identifier != m_last_track_metadata && !metadata_identifier.is_empty()) {
                        m_last_track_metadata = metadata_identifier;
                        m_last_track_path = current_path;
                        update_tooltip(track);
                        // Show popup notification for stream metadata change
                        popup_window::get_instance().show_track_info(track);
                    } else {
                        // Just update tooltip periodically without popup
                        static int update_counter = 0;
                        update_counter++;
                        if (update_counter >= 10) { // Every 5 seconds
                            update_counter = 0;
                            update_tooltip(track);
                        }
                    }
                } else {
                    // For local files, use path as identifier
                    if (current_path != m_last_track_path) {
                        m_last_track_path = current_path;
                        m_last_track_metadata = ""; // Clear metadata for local files
                        update_tooltip(track);
                        // Show popup notification for track change
                        popup_window::get_instance().show_track_info(track);
                    }
                }
            }
        } else {
            // Not playing - clear last track and update state
            if (!m_last_track_path.is_empty()) {
                m_last_track_path = "";
                m_last_track_metadata = "";
                update_playback_state("Stopped");
            }
        }
    } catch (...) {
        // Ignore timer errors
    }
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
