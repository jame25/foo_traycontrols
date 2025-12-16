#include "stdafx.h"
#include "control_panel.h"
#include "preferences.h"
#include "preferences.h"
#include "volume_popup.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// Timer constants
#define TIMEOUT_TIMER_ID 9999  // Use unique timer ID to avoid conflicts
#define ANIMATION_TIMER_ID 4003
#define OVERLAY_TIMER_ID 4004
#define FADE_TIMER_ID 4005
#define BUTTON_FADE_TIMER_ID 9001
#define SLIDE_TIMER_ID 4010

// Static instance
control_panel* control_panel::s_instance = nullptr;

// External declaration from main.cpp
extern HINSTANCE g_hIns;

//=============================================================================
// control_panel - Media control panel popup
//=============================================================================

control_panel::control_panel()
    : m_control_window(nullptr)
    , m_initialized(false)
    , m_visible(false)
    , m_current_time(0.0)
    , m_track_length(0.0)
    , m_is_playing(false)
    , m_is_paused(false)
    , m_is_undocked(false)
    , m_is_artwork_expanded(false)
    , m_last_click_time(0)
    , m_saved_undocked_width(338)
    , m_saved_undocked_height(120)
    , m_saved_expanded_width(400)
    , m_saved_expanded_height(400)
    , m_saved_miniplayer_x(-1)
    , m_saved_miniplayer_y(-1)
    , m_saved_miniplayer_width(0)
    , m_saved_miniplayer_height(0)
    , m_has_saved_miniplayer_state(false)
    , m_saved_was_undocked(false)
    , m_saved_was_expanded(false)
    , m_saved_was_compact(false)
    , m_overlay_visible(false)
    , m_last_mouse_move_time(0)
    , m_overlay_opacity(0)
    , m_fade_start_time(0)
    , m_undocked_overlay_visible(false)
    , m_undocked_overlay_opacity(0)
    , m_undocked_fade_start_time(0)
    , m_is_dragging(false)
    , m_buttons_visible(true)
    , m_button_opacity(100)
    , m_button_fade_start_time(0)
    , m_last_button_mouse_time(0)
    , m_is_compact_mode(false)
    , m_saved_normal_width(338)
    , m_saved_normal_height(120)
    , m_saved_compact_width(320)
    , m_was_compact_before_expanded(false)
    , m_is_rolling_animation(false)
    , m_rolling_to_compact(false)
    , m_roll_animation_step(0)
    , m_roll_animation_start_time(0)
    , m_cover_art_bitmap(nullptr)
    , m_cover_art_bitmap_large(nullptr)
    , m_cover_art_bitmap_original(nullptr)
    , m_original_art_width(0)
    , m_original_art_height(0)
    , m_artist_font(nullptr)
    , m_track_font(nullptr)
    , m_animating(false)
    , m_closing(false)
    , m_animation_step(0)
    , m_start_x(0)
    , m_start_y(0)
    , m_final_x(0)
    , m_final_y(0)
    , m_compact_controls_visible(false)
    , m_last_compact_mouse_time(0)
    , m_mouse_over_close_button(false)
    , m_mouse_in_window(false) // For collapse triangle hover visibility
    , m_hovered_button(0) // Initialize hover state
    , m_shuffle_active(false)
    , m_repeat_mode(0)
    , m_is_slid_to_side(false)
    , m_sliding_animation(false)
    , m_sliding_to_side(false)
    , m_pre_slide_x(0)
    , m_pre_slide_y(0)
    , m_slide_start_x(0)
    , m_slide_target_x(0)
    , m_slide_animation_step(0)
{
    m_last_click_pos.x = 0;
    m_last_click_pos.y = 0;
}

control_panel::~control_panel() {
    cleanup();
}

control_panel& control_panel::get_instance() {
    if (!s_instance) {
        s_instance = new control_panel();
    }
    return *s_instance;
}

void control_panel::initialize() {
    if (m_initialized) return;
    
    create_control_window();
    load_fonts();
    m_initialized = true;
}

void control_panel::cleanup() {
    if (m_visible) {
        hide_control_panel_immediate(); // Use immediate hide during cleanup
    }
    
    // Kill update timers
    if (m_control_window) {
        KillTimer(m_control_window, UPDATE_TIMER_ID);
        KillTimer(m_control_window, UPDATE_TIMER_ID + 1);
        KillTimer(m_control_window, TIMEOUT_TIMER_ID);
        KillTimer(m_control_window, ANIMATION_TIMER_ID);
        KillTimer(m_control_window, BUTTON_FADE_TIMER_ID);
        KillTimer(m_control_window, BUTTON_FADE_TIMER_ID + 1);
    }
    
    cleanup_cover_art();
    cleanup_fonts();
    
    if (m_control_window) {
        DestroyWindow(m_control_window);
        m_control_window = nullptr;
    }
    
    m_initialized = false;
}

void control_panel::show_control_panel(bool force_docked) {
    if (!m_initialized || m_visible) return;
    
    // Force docked state if requested (e.g., when opened from tray icon)
    if (force_docked) {
        m_is_undocked = false;
    }
    
    // Reset undocked overlay state
    m_undocked_overlay_visible = false;
    m_undocked_overlay_opacity = 0;
    
    // Sync shuffle/repeat state with foobar2000
    update_playback_order_state();
    
    // Position control panel
    position_control_panel();
    
    // Show window immediately with proper topmost behavior for docked mode
    ShowWindow(m_control_window, SW_SHOWNOACTIVATE);
    // Ensure topmost behavior for both docked and undocked modes
    SetWindowPos(m_control_window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    
    m_visible = true;
    
    // Enable mouse tracking to detect when cursor leaves window
    TRACKMOUSEEVENT tme = {0};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_control_window;
    TrackMouseEvent(&tme);
    
    // Schedule track info update for next timer tick (asynchronous), but not if we're trying to keep it docked
    if (!force_docked) {
        SetTimer(m_control_window, UPDATE_TIMER_ID + 2, 50, nullptr); // 50ms delayed update
    } else {
        // For forced docked mode, do immediate minimal update to avoid state changes
        if (m_control_window) {
            InvalidateRect(m_control_window, nullptr, TRUE);
        }
    }
    
    // Start update timer - only if not in artwork expanded mode
    if (!m_is_artwork_expanded) {
        // For force_docked (tray icon), use slower timer to avoid interference with timeout
        int timer_interval;
        if (force_docked) {
            timer_interval = 2000; // 2 second intervals for tray-opened panels
        } else {
            timer_interval = m_is_undocked ? 500 : 1000; // 500ms when undocked, 1000ms when docked
        }
        SetTimer(m_control_window, UPDATE_TIMER_ID, timer_interval, nullptr);
    }
    
    // Start timeout timer for docked panels (5 seconds auto-hide)
    // Force timeout timer when force_docked is true (from tray icon)
    if (force_docked || (!m_is_undocked && !m_is_artwork_expanded)) {
        SetTimer(m_control_window, TIMEOUT_TIMER_ID, 5000, nullptr);
    } else {
        // Kill any existing timeout timer if we're not setting one
        KillTimer(m_control_window, TIMEOUT_TIMER_ID);
    }
}

void control_panel::show_control_panel_simple() {
    if (!m_initialized || m_visible) return;

    // Force clean docked state - ignore any previous undocked state
    m_is_undocked = false;
    m_is_artwork_expanded = false;
    m_is_compact_mode = false; // Ensure compact mode is disabled in docked state
    m_has_saved_miniplayer_state = false; // Clear miniplayer memory when showing docked panel

    // Reset undocked overlay state
    m_undocked_overlay_visible = false;
    m_undocked_overlay_opacity = 0;
    
    // Update with current track info (but don't force cleanup for speed)
    update_track_info();
    
    // Position control panel
    position_control_panel();
    
    // Show window with topmost behavior (like original)
    ShowWindow(m_control_window, SW_SHOWNOACTIVATE);
    SetWindowPos(m_control_window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    
    m_visible = true;
    
    // Enable mouse tracking to detect when cursor leaves window
    TRACKMOUSEEVENT tme = {0};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_control_window;
    TrackMouseEvent(&tme);
    
    // Always start both timers (like original) - use timer ID from original version
    SetTimer(m_control_window, UPDATE_TIMER_ID, 1000, nullptr);
    SetTimer(m_control_window, TIMEOUT_TIMER_ID, 5000, nullptr); // 5 seconds auto-close
}

void control_panel::hide_control_panel() {
    if (!m_visible || m_animating) {
        OutputDebugStringA("hide_control_panel - early return (not visible or animating)\n");
        return;
    }
    
    // Start slide out animation (used for timeout)
    start_slide_out_animation();
}

void control_panel::hide_control_panel_immediate() {
    if (!m_visible) return;
    
    // Stop any ongoing animation
    if (m_animating) {
        m_animating = false;
        m_closing = false;
        KillTimer(m_control_window, ANIMATION_TIMER_ID);
    }
    
    // Stop other timers
    KillTimer(m_control_window, UPDATE_TIMER_ID);
    KillTimer(m_control_window, UPDATE_TIMER_ID + 1);
    KillTimer(m_control_window, TIMEOUT_TIMER_ID);
    KillTimer(m_control_window, SLIDE_TIMER_ID);
    
    // Reset slide-to-side state so panel reopens at original position
    if (m_is_slid_to_side && m_pre_slide_x != 0) {
        // Restore position before hiding
        RECT window_rect;
        GetWindowRect(m_control_window, &window_rect);
        int window_height = window_rect.bottom - window_rect.top;
        int window_width = window_rect.right - window_rect.left;
        SetWindowPos(m_control_window, nullptr, m_pre_slide_x, window_rect.top, 
                     window_width, window_height, SWP_NOACTIVATE | SWP_NOZORDER);
    }
    m_is_slid_to_side = false;
    m_sliding_animation = false;
    
    // Hide immediately without animation
    ShowWindow(m_control_window, SW_HIDE);
    m_visible = false;
}

void control_panel::toggle_control_panel() {
    if (m_visible) {
        hide_control_panel_immediate(); // Use immediate hide for manual toggle
    } else {
        // Reset to clean state for basic popup behavior
        m_is_undocked = false;
        m_is_artwork_expanded = false;
        m_is_compact_mode = false; // Reset compact mode when toggling via tray
        show_control_panel_simple(); // Use simple popup behavior like original
    }
}

void control_panel::hide_and_remember_miniplayer() {
    if (!m_control_window || !m_visible) return;

    // Only save state if in a non-docked mode (undocked, expanded, or compact)
    if (m_is_undocked || m_is_artwork_expanded || m_is_compact_mode) {
        RECT rect;
        GetWindowRect(m_control_window, &rect);
        m_saved_miniplayer_x = rect.left;
        m_saved_miniplayer_y = rect.top;
        m_saved_miniplayer_width = rect.right - rect.left;
        m_saved_miniplayer_height = rect.bottom - rect.top;
        m_saved_was_undocked = m_is_undocked;
        m_saved_was_expanded = m_is_artwork_expanded;
        m_saved_was_compact = m_is_compact_mode;
        m_has_saved_miniplayer_state = true;
    }

    // Hide immediately without animation
    KillTimer(m_control_window, UPDATE_TIMER_ID);
    KillTimer(m_control_window, ANIMATION_TIMER_ID);
    KillTimer(m_control_window, TIMEOUT_TIMER_ID);
    ShowWindow(m_control_window, SW_HIDE);
    m_visible = false;
}

void control_panel::show_miniplayer_at_saved_position() {
    if (!m_initialized) {
        initialize();
    }

    if (!m_control_window) {
        create_control_window();
    }

    // Update track info and load cover art
    update_track_info();

    // Restore the saved mode state
    m_is_undocked = m_saved_was_undocked;
    m_is_artwork_expanded = m_saved_was_expanded;
    m_is_compact_mode = m_saved_was_compact;

    // Use saved position and dimensions
    int x = m_saved_miniplayer_x;
    int y = m_saved_miniplayer_y;
    int width = m_saved_miniplayer_width;
    int height = m_saved_miniplayer_height;

    // Provide defaults if no valid saved state
    if (x < 0 || y < 0 || width <= 0 || height <= 0) {
        int screen_width = GetSystemMetrics(SM_CXSCREEN);
        int screen_height = GetSystemMetrics(SM_CYSCREEN);

        if (m_is_artwork_expanded) {
            width = m_saved_expanded_width > 0 ? m_saved_expanded_width : 400;
            height = m_saved_expanded_height > 0 ? m_saved_expanded_height : 400;
        } else if (m_is_compact_mode) {
            width = m_saved_compact_width > 0 ? m_saved_compact_width : 320;
            height = 75;
        } else {
            width = m_saved_undocked_width > 0 ? m_saved_undocked_width : 338;
            height = m_saved_undocked_height > 0 ? m_saved_undocked_height : 120;
        }

        x = (screen_width - width) / 2;
        y = (screen_height - height) / 2;
    }

    // Position and show the window
    SetWindowPos(m_control_window, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
    ShowWindow(m_control_window, SW_SHOWNOACTIVATE);
    m_visible = true;

    // Start update timer
    SetTimer(m_control_window, UPDATE_TIMER_ID, 500, nullptr);

    // Trigger repaint
    InvalidateRect(m_control_window, nullptr, TRUE);
}

void control_panel::show_undocked_miniplayer() {
    // If MiniPlayer is slid-to-side (peeking), slide it back out
    if (m_visible && m_is_slid_to_side) {
        slide_back_from_side();
        return;
    }
    
    // If MiniPlayer is fully visible, hide it and remember position
    if (m_visible) {
        hide_and_remember_miniplayer();
        return;
    }

    // Initialize if needed
    if (!m_initialized) {
        initialize();
    }

    if (!m_control_window) {
        create_control_window();
    }

    // Update track info and load cover art
    update_track_info();

    // Restore saved mode if available, otherwise default to Undocked
    if (m_has_saved_miniplayer_state) {
        m_is_undocked = m_saved_was_undocked;
        m_is_artwork_expanded = m_saved_was_expanded;
        m_is_compact_mode = m_saved_was_compact;
    } else {
        // Default to Undocked mode for first launch
        m_is_undocked = true;
        m_is_artwork_expanded = false;
        m_is_compact_mode = false;
    }

    // Set dimensions based on mode
    int width, height;
    if (m_is_artwork_expanded) {
        width = m_saved_expanded_width > 0 ? m_saved_expanded_width : 400;
        height = m_saved_expanded_height > 0 ? m_saved_expanded_height : 400;
    } else if (m_is_compact_mode) {
        width = m_saved_compact_width > 0 ? m_saved_compact_width : 320;
        height = 75;
    } else {
        // Undocked mode
        width = m_saved_undocked_width > 0 ? m_saved_undocked_width : 338;
        height = m_saved_undocked_height > 0 ? m_saved_undocked_height : 120;
    }

    // Use saved position if available, otherwise center on screen
    int x, y;
    if (m_has_saved_miniplayer_state && m_saved_miniplayer_x >= 0 && m_saved_miniplayer_y >= 0) {
        x = m_saved_miniplayer_x;
        y = m_saved_miniplayer_y;
    } else {
        int screen_width = GetSystemMetrics(SM_CXSCREEN);
        int screen_height = GetSystemMetrics(SM_CYSCREEN);
        x = (screen_width - width) / 2;
        y = (screen_height - height) / 2;
    }

    // Position and show the window
    SetWindowPos(m_control_window, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
    ShowWindow(m_control_window, SW_SHOWNOACTIVATE);
    m_visible = true;

    // Reset slide state
    m_is_slid_to_side = false;

    // Enable mouse tracking for button fade functionality
    TRACKMOUSEEVENT tme = {0};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_control_window;
    TrackMouseEvent(&tme);

    // Reset button opacity and visibility
    m_buttons_visible = true;
    m_button_opacity = 100;

    // Start update timer
    SetTimer(m_control_window, UPDATE_TIMER_ID, 500, nullptr);

    // Trigger repaint
    InvalidateRect(m_control_window, nullptr, TRUE);
}

void control_panel::update_track_info() {
    // Force cleanup of old artwork before loading new
    cleanup_cover_art();
    
    try {
        auto playback = playback_control::get();
        
        // Get current track
        metadb_handle_ptr track;
        if (playback->get_now_playing(track) && track.is_valid()) {
            // Check if this is a stream
            pfc::string8 path = track->get_path();
            bool is_stream = strstr(path.get_ptr(), "://") != nullptr;
            
            m_current_artist = "Unknown Artist";
            m_current_title = "Unknown Title";
            
            if (is_stream) {
                // For streaming sources, use titleformat to get what foobar2000 displays
                try {
                    static_api_ptr_t<titleformat_compiler> compiler;
                    service_ptr_t<titleformat_object> script;
                    
                    if (compiler->compile(script, "[%artist%]|[%title%]")) {
                        pfc::string8 formatted_title;
                        if (playback->playback_format_title(nullptr, formatted_title, script, nullptr, playback_control::display_level_all)) {
                            const char* separator = strstr(formatted_title.get_ptr(), "|");
                            if (separator && strlen(formatted_title.get_ptr()) > 1) {
                                pfc::string8 tf_artist(formatted_title.get_ptr(), separator - formatted_title.get_ptr());
                                pfc::string8 tf_title(separator + 1);
                                
                                if (!tf_artist.is_empty() && !tf_title.is_empty()) {
                                    m_current_artist = tf_artist.c_str();
                                    m_current_title = tf_title.c_str();
                                }
                            }
                        }
                    }
                } catch (...) {
                    // Fall through to basic metadata extraction
                }
            }
            
            // If titleformat didn't work or not a stream, try basic metadata
            if (m_current_artist == "Unknown Artist" || m_current_title == "Unknown Title") {
                file_info_impl info;
                if (track->get_info(info)) {
                    const char* artist_str = info.meta_get("ARTIST", 0);
                    const char* title_str = info.meta_get("TITLE", 0);
                    
                    if (artist_str && *artist_str) m_current_artist = artist_str;
                    if (title_str && *title_str) m_current_title = title_str;
                    
                    // For streams, try additional fallbacks
                    if (is_stream) {
                        if (m_current_title == "Unknown Title" && info.meta_exists("server")) {
                            m_current_title = info.meta_get("server", 0);
                        }
                        if (m_current_title == "Unknown Title" && info.meta_exists("SERVER")) {
                            m_current_title = info.meta_get("SERVER", 0);
                        }
                    }
                }
            }
            
            // Get track length
            m_track_length = track->get_length();
        } else {
            m_current_artist = "No track";
            m_current_title = "";
            m_track_length = 0.0;
        }
        
        // Get playback state and position
        m_is_playing = playback->is_playing();
        m_is_paused = playback->is_paused();
        m_current_time = playback->playback_get_position();
        
        // Load cover art - always reload when track info updates
        if (track.is_valid()) {
            load_cover_art();
            
            // Adjust window size for new artwork aspect ratio when in expanded mode
            if (m_is_artwork_expanded && m_control_window && m_original_art_width > 0 && m_original_art_height > 0) {
                RECT current_rect;
                GetWindowRect(m_control_window, &current_rect);
                int current_width = current_rect.right - current_rect.left;
                int current_height = current_rect.bottom - current_rect.top;
                
                float image_aspect = (float)m_original_art_width / (float)m_original_art_height;
                float window_aspect = (float)current_width / (float)current_height;
                
                // If aspect ratios differ significantly, resize the window to match the new image
                if (abs(image_aspect - window_aspect) > 0.05f) {
                    int new_width, new_height;
                    
                    // Keep the larger dimension, adjust the smaller one
                    if (image_aspect >= 1.0f) {
                        // Landscape or square - keep width, adjust height
                        new_width = current_width;
                        new_height = (int)((float)current_width / image_aspect);
                    } else {
                        // Portrait - keep height, adjust width
                        new_height = current_height;
                        new_width = (int)((float)current_height * image_aspect);
                    }
                    
                    // Ensure minimum size
                    if (new_width < 200) new_width = 200;
                    if (new_height < 200) new_height = 200;
                    
                    // Update saved dimensions
                    m_saved_expanded_width = new_width;
                    m_saved_expanded_height = new_height;
                    
                    // Resize window to match new aspect ratio
                    SetWindowPos(m_control_window, HWND_TOPMOST, 0, 0, new_width, new_height,
                        SWP_NOMOVE | SWP_NOACTIVATE);
                }
            }
        } else {
            // Clear artwork if no valid track
            cleanup_cover_art();
        }
        
    } catch (...) {
        m_current_artist = "Error";
        m_current_title = "";
        m_is_playing = false;
        m_is_paused = false;
        m_current_time = 0.0;
        m_track_length = 0.0;
    }
    
    // Refresh display
    // Always update if visible (including artwork expanded mode for track changes)
    if (m_control_window && (m_is_undocked || m_visible || m_is_artwork_expanded)) {
        InvalidateRect(m_control_window, nullptr, TRUE);
    }
}

void control_panel::create_control_window() {
    const char* class_name = "TrayControlsControlPanel";
    
    // Register window class (only once)
    static bool class_registered = false;
    if (!class_registered) {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = control_window_proc;
        wc.hInstance = g_hIns;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(32, 32, 32)); // Dark background
        wc.lpszClassName = L"TrayControlsControlPanel";
        
        ATOM class_atom = RegisterClassEx(&wc);
        if (class_atom != 0) {
            class_registered = true;
        }
    }
    
    // Create control window (initially hidden) - temporarily remove WS_EX_NOACTIVATE for testing
    m_control_window = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST, // Removed WS_EX_NOACTIVATE for testing
        L"TrayControlsControlPanel",
        L"Media Controls",
        WS_POPUP,
        0, 0, 338, 120, // Increased width by 15% (294 * 1.15 = 338)
        nullptr,
        nullptr,
        g_hIns,
        this
    );
    
    if (!m_control_window) {
        throw exception_win32(GetLastError());
    }
    
    // Apply window corner preference (rounded/square corners)
    apply_window_corner_preference();
}

void control_panel::position_control_panel() {
    if (!m_control_window) return;
    
    // Get screen dimensions
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    
    // Get taskbar info
    APPBARDATA abd = {};
    abd.cbSize = sizeof(APPBARDATA);
    SHAppBarMessage(ABM_GETTASKBARPOS, &abd);
    
    // Calculate position (bottom-right, above tray area)
    const int panel_width = 338;
    const int panel_height = 120;
    const int margin = 10;
    
    int x = screen_width - panel_width - margin;
    int y = screen_height - panel_height - margin;
    
    // Adjust for taskbar position
    if (abd.rc.bottom == screen_height && abd.rc.left == 0 && abd.rc.right == screen_width) {
        // Taskbar at bottom
        y = abd.rc.top - panel_height - margin;
    } else if (abd.rc.right == screen_width && abd.rc.top == 0 && abd.rc.bottom == screen_height) {
        // Taskbar at right
        x = abd.rc.left - panel_width - margin;
    } else if (abd.rc.top == 0 && abd.rc.left == 0 && abd.rc.right == screen_width) {
        // Taskbar at top
        y = abd.rc.bottom + margin;
    } else if (abd.rc.left == 0 && abd.rc.top == 0 && abd.rc.bottom == screen_height) {
        // Taskbar at left
        x = abd.rc.right + margin;
    }
    
    // Position the window - but don't override the Z-order set by show_control_panel
    SetWindowPos(m_control_window, nullptr, x, y, panel_width, panel_height, SWP_NOACTIVATE | SWP_NOZORDER);
}

void control_panel::load_cover_art() {
    cleanup_cover_art();
    
    try {
        auto playback = playback_control::get();
        metadb_handle_ptr track;
        
        if (playback->get_now_playing(track) && track.is_valid()) {
            auto api = album_art_manager_v2::get();
            if (!api.is_valid()) return;
            
            auto extractor = api->open(pfc::list_single_ref_t<metadb_handle_ptr>(track), 
                                       pfc::list_single_ref_t<GUID>(album_art_ids::cover_front), 
                                       fb2k::noAbort);
            
            if (extractor.is_valid()) {
                auto data = extractor->query(album_art_ids::cover_front, fb2k::noAbort);
                if (data.is_valid() && data->get_size() > 0) {
                    // Convert album art data to HBITMAP (all sizes)
                    m_cover_art_bitmap = convert_album_art_to_bitmap(data);
                    m_cover_art_bitmap_large = convert_album_art_to_bitmap_large(data);
                    m_cover_art_bitmap_original = convert_album_art_to_bitmap_original(data);
                }
            }
        }
    } catch (...) {
        // Ignore album art errors
    }
}

void control_panel::cleanup_cover_art() {
    if (m_cover_art_bitmap) {
        DeleteObject(m_cover_art_bitmap);
        m_cover_art_bitmap = nullptr;
    }
    if (m_cover_art_bitmap_large) {
        DeleteObject(m_cover_art_bitmap_large);
        m_cover_art_bitmap_large = nullptr;
    }
    if (m_cover_art_bitmap_original) {
        DeleteObject(m_cover_art_bitmap_original);
        m_cover_art_bitmap_original = nullptr;
    }
    m_original_art_width = 0;
    m_original_art_height = 0;
}

HBITMAP control_panel::convert_album_art_to_bitmap(album_art_data_ptr art_data) {
    if (!art_data.is_valid() || art_data->get_size() == 0) {
        return nullptr;
    }
    
    HBITMAP result = nullptr;
    
    // Initialize GDI+ if not already done
    static ULONG_PTR gdiplusToken = 0;
    static bool gdiplus_initialized = false;
    
    if (!gdiplus_initialized) {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) == Gdiplus::Ok) {
            gdiplus_initialized = true;
        } else {
            return nullptr;
        }
    }
    
    try {
        // Create IStream from memory buffer
        CComPtr<IStream> stream;
        stream.p = SHCreateMemStream(reinterpret_cast<const BYTE*>(art_data->get_ptr()), 
                                     static_cast<UINT>(art_data->get_size()));
        if (!stream) {
            return nullptr;
        }
        
        // Load image from stream using GDI+
        Gdiplus::Image image(stream);
        if (image.GetLastStatus() != Gdiplus::Ok) {
            return nullptr;
        }
        
        // Create a bitmap with the desired size (80x80 for the cover area)
        const int target_size = 80;
        Gdiplus::Bitmap bitmap(target_size, target_size, PixelFormat32bppARGB);
        if (bitmap.GetLastStatus() != Gdiplus::Ok) {
            return nullptr;
        }
        
        // Draw the image scaled to fit the target size
        Gdiplus::Graphics graphics(&bitmap);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        
        // Calculate scaling to maintain aspect ratio
        UINT img_width = image.GetWidth();
        UINT img_height = image.GetHeight();
        
        int draw_width = target_size;
        int draw_height = target_size;
        int offset_x = 0;
        int offset_y = 0;
        
        if (img_width != img_height) {
            if (img_width > img_height) {
                draw_height = (target_size * img_height) / img_width;
                offset_y = (target_size - draw_height) / 2;
            } else {
                draw_width = (target_size * img_width) / img_height;
                offset_x = (target_size - draw_width) / 2;
            }
        }
        
        // Clear background to dark gray to match panel
        graphics.Clear(Gdiplus::Color(255, 32, 32, 32));
        
        // Draw the scaled image
        graphics.DrawImage(&image, offset_x, offset_y, draw_width, draw_height);
        
        // Convert to HBITMAP
        if (bitmap.GetHBITMAP(Gdiplus::Color(32, 32, 32), &result) != Gdiplus::Ok) {
            result = nullptr;
        }
        
    } catch (...) {
        result = nullptr;
    }
    
    return result;
}

HBITMAP control_panel::convert_album_art_to_bitmap_large(album_art_data_ptr art_data) {
    if (!art_data.is_valid() || art_data->get_size() == 0) {
        return nullptr;
    }
    
    HBITMAP result = nullptr;
    
    // Initialize GDI+ if not already done
    static ULONG_PTR gdiplusToken = 0;
    static bool gdiplus_initialized = false;
    
    if (!gdiplus_initialized) {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) == Gdiplus::Ok) {
            gdiplus_initialized = true;
        } else {
            return nullptr;
        }
    }
    
    try {
        // Create IStream from memory buffer
        CComPtr<IStream> stream;
        stream.p = SHCreateMemStream(reinterpret_cast<const BYTE*>(art_data->get_ptr()), 
                                     static_cast<UINT>(art_data->get_size()));
        if (!stream) {
            return nullptr;
        }
        
        // Load image from stream using GDI+
        Gdiplus::Image image(stream);
        if (image.GetLastStatus() != Gdiplus::Ok) {
            return nullptr;
        }
        
        // Create a high-quality bitmap (300x300 for expanded view)
        const int target_size = 300;
        Gdiplus::Bitmap bitmap(target_size, target_size, PixelFormat32bppARGB);
        if (bitmap.GetLastStatus() != Gdiplus::Ok) {
            return nullptr;
        }
        
        // Draw the image with highest quality settings
        Gdiplus::Graphics graphics(&bitmap);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
        
        // Calculate scaling to maintain aspect ratio
        UINT img_width = image.GetWidth();
        UINT img_height = image.GetHeight();
        
        int draw_width = target_size;
        int draw_height = target_size;
        int offset_x = 0;
        int offset_y = 0;
        
        if (img_width != img_height) {
            if (img_width > img_height) {
                draw_height = (target_size * img_height) / img_width;
                offset_y = (target_size - draw_height) / 2;
            } else {
                draw_width = (target_size * img_width) / img_height;
                offset_x = (target_size - draw_width) / 2;
            }
        }
        
        // Clear background to dark gray to match panel
        graphics.Clear(Gdiplus::Color(255, 32, 32, 32));
        
        // Draw the scaled image with highest quality
        graphics.DrawImage(&image, offset_x, offset_y, draw_width, draw_height);
        
        // Convert to HBITMAP
        if (bitmap.GetHBITMAP(Gdiplus::Color(32, 32, 32), &result) != Gdiplus::Ok) {
            result = nullptr;
        }
        
    } catch (...) {
        result = nullptr;
    }
    
    return result;
}

HBITMAP control_panel::convert_album_art_to_bitmap_original(album_art_data_ptr art_data) {
    if (!art_data.is_valid() || art_data->get_size() == 0) {
        return nullptr;
    }
    
    HBITMAP result = nullptr;
    
    // Initialize GDI+ if not already done
    static ULONG_PTR gdiplusToken = 0;
    static bool gdiplus_initialized = false;
    
    if (!gdiplus_initialized) {
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) == Gdiplus::Ok) {
            gdiplus_initialized = true;
        } else {
            return nullptr;
        }
    }
    
    try {
        // Create IStream from memory buffer
        CComPtr<IStream> stream;
        stream.p = SHCreateMemStream(reinterpret_cast<const BYTE*>(art_data->get_ptr()), 
                                     static_cast<UINT>(art_data->get_size()));
        if (!stream) {
            return nullptr;
        }
        
        // Load image from stream using GDI+
        Gdiplus::Image image(stream);
        if (image.GetLastStatus() != Gdiplus::Ok) {
            return nullptr;
        }
        
        // Get original dimensions
        UINT img_width = image.GetWidth();
        UINT img_height = image.GetHeight();
        
        // Store original dimensions for aspect ratio calculations
        m_original_art_width = img_width;
        m_original_art_height = img_height;
        
        // Create bitmap at original resolution (no scaling)
        Gdiplus::Bitmap bitmap(img_width, img_height, PixelFormat32bppARGB);
        if (bitmap.GetLastStatus() != Gdiplus::Ok) {
            return nullptr;
        }
        
        // Draw the image at original resolution with highest quality settings
        Gdiplus::Graphics graphics(&bitmap);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
        
        // Draw at original size with no scaling
        graphics.DrawImage(&image, 0, 0, img_width, img_height);
        
        // Convert to HBITMAP
        if (bitmap.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &result) != Gdiplus::Ok) {
            result = nullptr;
        }
        
    } catch (...) {
        result = nullptr;
        m_original_art_width = 0;
        m_original_art_height = 0;
    }
    
    return result;
}


// Forward declaration for helper
void draw_hover_circle(HDC hdc, int x, int y, int size);

// Vector-drawn icon implementations - Material Design Style
void control_panel::draw_play_icon(HDC hdc, int x, int y, int size) {
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf); // For precision

    // Draw white circle background
    int radius = size / 2;
    Gdiplus::SolidBrush bg_brush(Gdiplus::Color(255, 255, 255, 255));
    graphics.FillEllipse(&bg_brush, x - radius, y - radius, size, size); // size is diameter

    // Draw black triangle icon inside
    int icon_height = size * 4 / 10;
    int half_icon = icon_height / 2;
    int icon_width = icon_height; 
    int center_offset_x = icon_width / 8;
    
    Gdiplus::Point triangle[3];
    triangle[0] = Gdiplus::Point(x - icon_width/2 + center_offset_x, y - half_icon);
    triangle[1] = Gdiplus::Point(x - icon_width/2 + center_offset_x, y + half_icon);
    triangle[2] = Gdiplus::Point(x + icon_width/2 + center_offset_x, y);
    
    Gdiplus::SolidBrush icon_brush(Gdiplus::Color(255, 0, 0, 0));
    graphics.FillPolygon(&icon_brush, triangle, 3);
}

void control_panel::draw_pause_icon(HDC hdc, int x, int y, int size) {
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    // Draw white circle background
    int radius = size / 2;
    Gdiplus::SolidBrush bg_brush(Gdiplus::Color(255, 255, 255, 255));
    graphics.FillEllipse(&bg_brush, x - radius, y - radius, size, size);

    // Draw black bars inside
    int icon_height = size * 4 / 10;
    int half_icon = icon_height / 2;
    int bar_width = icon_height / 3;
    int gap = icon_height / 3; 
    
    int offset = gap / 2;
    
    Gdiplus::SolidBrush icon_brush(Gdiplus::Color(255, 0, 0, 0));
    
    graphics.FillRectangle(&icon_brush, x - offset - bar_width, y - half_icon, bar_width, icon_height);
    graphics.FillRectangle(&icon_brush, x + offset, y - half_icon, bar_width, icon_height);
}

// Helper for drawing hover circles behind buttons
// Helper for drawing hover circles behind buttons
void draw_hover_circle(HDC hdc, int x, int y, int size) {
    int radius = size * 3 / 4; 
    // GDI+ hover circle
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, 60, 60, 60)); // Lighter than background
    graphics.FillEllipse(&brush, x - radius, y - radius, radius * 2, radius * 2);
}

void control_panel::draw_previous_icon(HDC hdc, int x, int y, int size) {
    // Hover highlight removed per user request
    // if (m_hovered_button == BTN_PREV) {
    //     draw_hover_circle(hdc, x, y, size);
    // }
    
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::SolidBrush brush(Gdiplus::Color(255, 200, 200, 200));

    int icon_h = size * 6 / 10; 
    int bar_width = 3; 
    int gap = 2; 
    
    int half_size = size / 2;
    
    // Bar on LEFT
    graphics.FillRectangle(&brush, x - icon_h/2, y - icon_h/2, bar_width, icon_h);
    
    // Triangle pointing left
    int tri_start_x = x - icon_h/2 + bar_width + gap;
    Gdiplus::Point triangle[3];
    triangle[0] = Gdiplus::Point(x + icon_h/2, y - icon_h/2);   // Top right
    triangle[1] = Gdiplus::Point(x + icon_h/2, y + icon_h/2);   // Bottom right
    triangle[2] = Gdiplus::Point(tri_start_x, y);               // Left point
    
    graphics.FillPolygon(&brush, triangle, 3);
}

void control_panel::draw_next_icon(HDC hdc, int x, int y, int size) {
    // Hover highlight removed per user request
    // if (m_hovered_button == BTN_NEXT) {
    //     draw_hover_circle(hdc, x, y, size);
    // }
    
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    Gdiplus::SolidBrush brush(Gdiplus::Color(255, 200, 200, 200));

    int icon_h = size * 6 / 10;
    int bar_width = 3;
    int gap = 2;
    
    // Triangle pointing right
    int tri_start_x = x - icon_h/2; 
    Gdiplus::Point triangle[3];
    triangle[0] = Gdiplus::Point(tri_start_x, y - icon_h/2);     // Top left
    triangle[1] = Gdiplus::Point(tri_start_x, y + icon_h/2);     // Bottom left
    triangle[2] = Gdiplus::Point(tri_start_x + icon_h - bar_width - gap, y); // Right point
    
    graphics.FillPolygon(&brush, triangle, 3);
    
    // Bar on RIGHT
    int bar_x = tri_start_x + icon_h - bar_width;
    graphics.FillRectangle(&brush, bar_x, y - icon_h/2, bar_width, icon_h);
}


void control_panel::draw_shuffle_icon(HDC hdc, int x, int y, int size) {
    // Hover highlight removed per user request
    // if (m_hovered_button == BTN_SHUFFLE) {
    //     draw_hover_circle(hdc, x, y, size);
    // }
    
    // Material Design Shuffle icon from SVG
    Gdiplus::Color color = m_shuffle_active ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 100, 100, 100);
    
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    
    float scale = size / 24.0f;
    float fx = (float)x - size / 2.0f;
    float fy = (float)y - size / 2.0f;
    
    // Apply translation and scale to map 24x24 SVG to icon size
    graphics.TranslateTransform(fx, fy);
    graphics.ScaleTransform(scale, scale);
    
    Gdiplus::SolidBrush brush(color);
    
    // SVG Path: M10.59 9.17L5.41 4 4 5.41l5.17 5.17 1.42-1.41z
    //           M14.5 4l2.04 2.04L4 18.59 5.41 20 17.96 7.46 20 9.5V4h-5.5z
    //           m.33 9.41l-1.41 1.41 3.13 3.13L14.5 20H20v-5.5l-2.04 2.04-3.13-3.13z
    
    Gdiplus::GraphicsPath path;
    
    // First shape: diagonal cross section (top-left area)
    path.StartFigure();
    path.AddLine(10.59f, 9.17f, 5.41f, 4.0f);
    path.AddLine(5.41f, 4.0f, 4.0f, 5.41f);
    path.AddLine(4.0f, 5.41f, 9.17f, 10.58f);
    path.CloseFigure();
    
    // Second shape: main X with top-right arrowhead
    path.StartFigure();
    path.AddLine(14.5f, 4.0f, 16.54f, 6.04f);
    path.AddLine(16.54f, 6.04f, 4.0f, 18.59f);
    path.AddLine(4.0f, 18.59f, 5.41f, 20.0f);
    path.AddLine(5.41f, 20.0f, 17.96f, 7.46f);
    path.AddLine(17.96f, 7.46f, 20.0f, 9.5f);
    path.AddLine(20.0f, 9.5f, 20.0f, 4.0f);
    path.AddLine(20.0f, 4.0f, 14.5f, 4.0f);
    path.CloseFigure();
    
    // Third shape: bottom-right arrowhead
    path.StartFigure();
    path.AddLine(14.83f, 13.41f, 13.42f, 14.82f);
    path.AddLine(13.42f, 14.82f, 16.55f, 17.95f);
    path.AddLine(16.55f, 17.95f, 14.5f, 20.0f);
    path.AddLine(14.5f, 20.0f, 20.0f, 20.0f);
    path.AddLine(20.0f, 20.0f, 20.0f, 14.5f);
    path.AddLine(20.0f, 14.5f, 17.96f, 16.54f);
    path.AddLine(17.96f, 16.54f, 14.83f, 13.41f);
    path.CloseFigure();
    
    graphics.FillPath(&brush, &path);
    
    graphics.ResetTransform();
}

void control_panel::draw_repeat_icon(HDC hdc, int x, int y, int size) {
    // Hover highlight removed per user request
    // if (m_hovered_button == BTN_REPEAT) {
    //     draw_hover_circle(hdc, x, y, size);
    // }
    
    bool is_active = (m_repeat_mode > 0);
    Gdiplus::Color color = is_active ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 100, 100, 100);
    
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    
    float scale = size / 24.0f;
    float fx = (float)x - size / 2.0f;
    float fy = (float)y - size / 2.0f;
    
    // Apply translation and scale to map 24x24 SVG to icon size
    graphics.TranslateTransform(fx, fy);
    graphics.ScaleTransform(scale, scale);
    
    Gdiplus::SolidBrush brush(color);
    
    // SVG Path: M7 7h10v3l4-4-4-4v3H5v6h2V7zm10 10H7v-3l-4 4 4 4v-3h12v-6h-2v4z
    // This is a rectangular repeat icon with arrows on right and left
    
    Gdiplus::GraphicsPath path;
    
    // First shape: Top arrow (pointing right)
    // M7 7 h10 v3 l4-4 -4-4 v3 H5 v6 h2 V7 z
    path.StartFigure();
    path.AddLine(7.0f, 7.0f, 17.0f, 7.0f);   // h10
    path.AddLine(17.0f, 7.0f, 17.0f, 10.0f); // v3
    path.AddLine(17.0f, 10.0f, 21.0f, 6.0f); // l4-4 (arrow tip)
    path.AddLine(21.0f, 6.0f, 17.0f, 2.0f);  // -4-4
    path.AddLine(17.0f, 2.0f, 17.0f, 5.0f);  // v3
    path.AddLine(17.0f, 5.0f, 5.0f, 5.0f);   // H5
    path.AddLine(5.0f, 5.0f, 5.0f, 11.0f);   // v6
    path.AddLine(5.0f, 11.0f, 7.0f, 11.0f);  // h2
    path.AddLine(7.0f, 11.0f, 7.0f, 7.0f);   // V7
    path.CloseFigure();
    
    // Second shape: Bottom arrow (pointing left)
    // m10 10 (relative from last point = 17,17) -> actually starts at 17,17
    // H7 v-3 l-4 4 4 4 v-3 h12 v-6 h-2 v4 z
    path.StartFigure();
    path.AddLine(17.0f, 17.0f, 7.0f, 17.0f);   // H7
    path.AddLine(7.0f, 17.0f, 7.0f, 14.0f);    // v-3
    path.AddLine(7.0f, 14.0f, 3.0f, 18.0f);    // l-4 4 (arrow tip)
    path.AddLine(3.0f, 18.0f, 7.0f, 22.0f);    // 4 4
    path.AddLine(7.0f, 22.0f, 7.0f, 19.0f);    // v-3
    path.AddLine(7.0f, 19.0f, 19.0f, 19.0f);   // h12
    path.AddLine(19.0f, 19.0f, 19.0f, 13.0f);  // v-6
    path.AddLine(19.0f, 13.0f, 17.0f, 13.0f);  // h-2
    path.AddLine(17.0f, 13.0f, 17.0f, 17.0f);  // v4
    path.CloseFigure();
    
    // If Track Repeat (Mode 2), add the "1" in the center from repeat_one SVG
    // SVG path segment: m-4-2V9h-1l-2 1v1h1.5v4H13z (relative to 17,17 = absolute 13,15)
    if (m_repeat_mode == 2) {
        path.StartFigure();
        path.AddLine(13.0f, 15.0f, 13.0f, 9.0f);   // V9
        path.AddLine(13.0f, 9.0f, 12.0f, 9.0f);    // h-1
        path.AddLine(12.0f, 9.0f, 10.0f, 10.0f);   // l-2 1
        path.AddLine(10.0f, 10.0f, 10.0f, 11.0f);  // v1
        path.AddLine(10.0f, 11.0f, 11.5f, 11.0f);  // h1.5
        path.AddLine(11.5f, 11.0f, 11.5f, 15.0f);  // v4
        path.AddLine(11.5f, 15.0f, 13.0f, 15.0f);  // H13
        path.CloseFigure();
    }
    
    graphics.FillPath(&brush, &path);
    
    graphics.ResetTransform();
}

void control_panel::draw_up_arrow(HDC hdc, int x, int y, int size) {
    // Material Design Arrow Drop Up
    int half_size = size / 2;
    
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    
    Gdiplus::Point triangle[3];
    triangle[0] = Gdiplus::Point(x, y - half_size/2);                 // Top point
    triangle[1] = Gdiplus::Point(x - half_size, y + half_size/2);     // Bottom left
    triangle[2] = Gdiplus::Point(x + half_size, y + half_size/2);     // Bottom right
    
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255, 255));
    // Optional outline
    Gdiplus::Pen pen(Gdiplus::Color(255, 255, 255, 255), 1.0f);
    
    graphics.FillPolygon(&brush, triangle, 3);
    graphics.DrawPolygon(&pen, triangle, 3);
}

void control_panel::draw_down_arrow(HDC hdc, int x, int y, int size) {
    // Material Design Arrow Drop Down
    int half_size = size / 2;
    
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    
    Gdiplus::Point triangle[3];
    triangle[0] = Gdiplus::Point(x, y + half_size/2);                 // Bottom point
    triangle[1] = Gdiplus::Point(x - half_size, y - half_size/2);     // Top left
    triangle[2] = Gdiplus::Point(x + half_size, y - half_size/2);     // Top right
    
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255, 255));
    Gdiplus::Pen pen(Gdiplus::Color(255, 255, 255, 255), 1.0f);
    
    graphics.FillPolygon(&brush, triangle, 3);
    graphics.DrawPolygon(&pen, triangle, 3);
}

void control_panel::draw_up_arrow_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    // Material Design Up Arrow with opacity
    int half_size = size / 2;
    
    POINT triangle[3];
    triangle[0] = {x, y - half_size/2};                 // Top point
    triangle[1] = {x - half_size, y + half_size/2};     // Bottom left
    triangle[2] = {x + half_size, y + half_size/2};     // Bottom right
    
    HBRUSH brush = CreateSolidBrush(RGB(opacity, opacity, opacity));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(opacity, opacity, opacity));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    
    Polygon(hdc, triangle, 3);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void control_panel::draw_down_arrow_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    // Material Design Down Arrow with opacity
    int half_size = size / 2;
    
    POINT triangle[3];
    triangle[0] = {x, y + half_size/2};                 // Bottom point
    triangle[1] = {x - half_size, y - half_size/2};     // Top left
    triangle[2] = {x + half_size, y - half_size/2};     // Top right
    
    HBRUSH brush = CreateSolidBrush(RGB(opacity, opacity, opacity));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(opacity, opacity, opacity));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    
    Polygon(hdc, triangle, 3);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void control_panel::draw_roll_dots(HDC hdc, int x, int y, int size) {
    // Draw six dots in two columns (3 rows x 2 columns)
    int dot_radius = size / 8;
    int spacing = size / 4;
    
    HBRUSH brush = CreateSolidBrush(RGB(180, 180, 180));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    
    // Draw 6 dots in 2x3 grid
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 2; col++) {
            int dot_x = x + (col * spacing) - spacing/2;
            int dot_y = y + (row * spacing) - spacing;
            
            Ellipse(hdc, 
                dot_x - dot_radius, dot_y - dot_radius,
                dot_x + dot_radius, dot_y + dot_radius);
        }
    }
    
    SelectObject(hdc, old_brush);
    DeleteObject(brush);
}

void control_panel::draw_volume_icon(HDC hdc, int x, int y, int size) {
    // Material Design Speaker Icon - Screenshot accurate
    
    // Normalize colors
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    
    // Speaker is drawn to the left of x, waves to the right
    // Center point x,y is roughly the gap between speaker and waves
    
    // 1. Speaker Body
    // Rect part
    int body_h = size * 3 / 8; // ~37% height
    int body_w = size / 4;
    int flare_w = size / 4;
    int speaker_tip_x = x - 2; // Tip of the speaker cone
    
    POINT speaker[6];
    speaker[0] = {speaker_tip_x - flare_w - body_w, y - body_h/2}; // Top-left rect
    speaker[1] = {speaker_tip_x - flare_w, y - body_h/2};          // Top-right rect
    speaker[2] = {speaker_tip_x, y - size/2 + 2};                  // Top flare tip
    speaker[3] = {speaker_tip_x, y + size/2 - 2};                  // Bottom flare tip
    speaker[4] = {speaker_tip_x - flare_w, y + body_h/2};          // Bottom-right rect
    speaker[5] = {speaker_tip_x - flare_w - body_w, y + body_h/2}; // Bottom-left rect
    
    Polygon(hdc, speaker, 6);
    
    // 2. Sound Waves
    HPEN wave_pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255)); // 2px thickness for visibility
    SelectObject(hdc, wave_pen);
    HBRUSH null_brush = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdc, null_brush); // Don't fill arcs
    
    int wave_center_x = speaker_tip_x; // Waves emanate from speaker tip
    
    // Inner wave
    int r1 = size / 3;
    // Bounding box for arc
    int r1_left = wave_center_x - r1;
    int r1_top = y - r1;
    int r1_right = wave_center_x + r1;
    int r1_bottom = y + r1;
    // Radial points for 45 degree start/end
    int offset1 = r1 * 7 / 10; // ~sin(45)
    Arc(hdc, r1_left, r1_top, r1_right, r1_bottom, 
        wave_center_x + offset1, y - offset1,  // Start: Top Right
        wave_center_x + offset1, y + offset1); // End: Bottom Right
        
    // Outer wave
    int r2 = size * 2 / 3;
    int r2_left = wave_center_x - r2;
    int r2_top = y - r2;
    int r2_right = wave_center_x + r2;
    int r2_bottom = y + r2;
    int offset2 = r2 * 7 / 10;
    Arc(hdc, r2_left, r2_top, r2_right, r2_bottom, 
        wave_center_x + offset2, y - offset2,
        wave_center_x + offset2, y + offset2);

    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(wave_pen);
    DeleteObject(brush);
}

void control_panel::draw_volume_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    // Material Design Volume Up with opacity - Screenshot accurate
    
    int color_value = 32 + ((255 - 32) * opacity) / 100;
    
    HBRUSH brush = CreateSolidBrush(RGB(color_value, color_value, color_value));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(color_value, color_value, color_value));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    
    // 1. Speaker Body
    int body_h = size * 3 / 8;
    int body_w = size / 4;
    int flare_w = size / 4;
    int speaker_tip_x = x - 2;
    
    POINT speaker[6];
    speaker[0] = {speaker_tip_x - flare_w - body_w, y - body_h/2}; 
    speaker[1] = {speaker_tip_x - flare_w, y - body_h/2};          
    speaker[2] = {speaker_tip_x, y - size/2 + 2};                  
    speaker[3] = {speaker_tip_x, y + size/2 - 2};                  
    speaker[4] = {speaker_tip_x - flare_w, y + body_h/2};          
    speaker[5] = {speaker_tip_x - flare_w - body_w, y + body_h/2}; 
    
    Polygon(hdc, speaker, 6);
    
    // 2. Sound Waves
    HPEN wave_pen = CreatePen(PS_SOLID, 2, RGB(color_value, color_value, color_value));
    SelectObject(hdc, wave_pen);
    HBRUSH null_brush = (HBRUSH)GetStockObject(NULL_BRUSH);
    SelectObject(hdc, null_brush);
    
    int wave_center_x = speaker_tip_x;

    // Inner wave
    int r1 = size / 3;
    int r1_left = wave_center_x - r1;
    int r1_top = y - r1;
    int r1_right = wave_center_x + r1;
    int r1_bottom = y + r1;
    int offset1 = r1 * 7 / 10;
    Arc(hdc, r1_left, r1_top, r1_right, r1_bottom, 
        wave_center_x + offset1, y - offset1,
        wave_center_x + offset1, y + offset1);
        
    // Outer wave
    int r2 = size * 2 / 3;
    int r2_left = wave_center_x - r2;
    int r2_top = y - r2;
    int r2_right = wave_center_x + r2;
    int r2_bottom = y + r2;
    int offset2 = r2 * 7 / 10;
    Arc(hdc, r2_left, r2_top, r2_right, r2_bottom, 
        wave_center_x + offset2, y - offset2,
        wave_center_x + offset2, y + offset2);

    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(wave_pen);
    DeleteObject(brush);
}

void control_panel::draw_close_icon(HDC hdc, int x, int y, int size) {
    // Material Design Close (X)
    int half_size = size / 2;
    int stroke_width = 2; // Thicker clearer stroke
    
    HPEN pen = CreatePen(PS_SOLID, stroke_width, RGB(255, 255, 255));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    
    MoveToEx(hdc, x - half_size, y - half_size, nullptr);
    LineTo(hdc, x + half_size, y + half_size);
    MoveToEx(hdc, x + half_size, y - half_size, nullptr);
    LineTo(hdc, x - half_size, y + half_size);
    DeleteObject(pen);
}

// Helper to draw collapse triangle
void control_panel::draw_collapse_triangle(HDC hdc, int x, int y, int size, int opacity) {
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    
    // Simulate opacity
    int alpha = (255 * opacity) / 100;
    // Ensure minimum visibility
    if (alpha < 50) alpha = 50; 
    
    Gdiplus::SolidBrush brush(Gdiplus::Color(alpha, 255, 255, 255));
    
    // Triangle pointing diagonally down-left from top-right corner
    // Or just a standard triangle pointing down or left?
    // "Triangle in the top right corner... pointing inwards" usually means:
    //  Right angle at the top-right corner? Or an equilateral pointing South-West?
    // Let's do a right-triangle in the corner.
    // P1: (x, y) - Corner
    // P2: (x - size, y) - Top edge
    // P3: (x, y + size) - Right edge
    // Actually, user said "small white triangle... to change to undocked".
    // A simple arrow-like triangle pointing "restore" might be better.
    // Let's stick to a simple small triangle pointing inwards (bottom-left).
    
    int half = size / 2;
    // Center x,y passed? Or top-right corner coordinates?
    // Let's assume (x,y) is the CENTER of the triangle area.
    
    Gdiplus::Point triangle[3];
    // Inward pointing (West-South)
    triangle[0] = Gdiplus::Point(x + half, y - half); // Top-Right
    triangle[1] = Gdiplus::Point(x - half, y - half); // Top-Left
    triangle[2] = Gdiplus::Point(x + half, y + half); // Bottom-Right
    // Wait, that's half a square.
    
    // Let's do an equilateral triangle pointing down-left?
    // Or just a standard "Restore" glyph?
    // User asked for "small white triangle".
    // I will draw a triangle pointing South-West.
    
    triangle[0] = Gdiplus::Point(x + half, y - half); // Top-Right
    triangle[1] = Gdiplus::Point(x - half, y - half); // Top-Left
    triangle[2] = Gdiplus::Point(x + half, y + half); // Bottom-Right
    // This forms a right triangle in the top-right corner.
    
    graphics.FillPolygon(&brush, triangle, 3);
}

// Helper to draw close icon
void control_panel::draw_close_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    
    // Simulate opacity
    int alpha = (255 * opacity) / 100;
    // Ensure minimum visibility
    if (alpha < 50) alpha = 50; 
    
    Gdiplus::Pen pen(Gdiplus::Color(alpha, 255, 255, 255), 2.0f);
    
    int half_size = size / 2;
    // Draw X
    graphics.DrawLine(&pen, x - half_size, y - half_size, x + half_size, y + half_size);
    graphics.DrawLine(&pen, x + half_size, y - half_size, x - half_size, y + half_size);
}

// Opacity-based icon drawing for button fade effect
void control_panel::draw_play_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    // Simulate opacity by scaling color from dark grey (0%) to white (100%)
    int bg_val = 32 + ((255 - 32) * opacity) / 100;
    
    // Draw circle
    int radius = size / 2;
    Gdiplus::SolidBrush bg_brush(Gdiplus::Color(255, bg_val, bg_val, bg_val));
    graphics.FillEllipse(&bg_brush, x - radius, y - radius, size, size);
    
    // Draw black icon
    int icon_height = size * 4 / 10;
    int half_icon = icon_height / 2;
    int icon_width = icon_height; 
    int center_offset_x = icon_width / 8;
    
    Gdiplus::Point triangle[3];
    triangle[0] = Gdiplus::Point(x - icon_width/2 + center_offset_x, y - half_icon);
    triangle[1] = Gdiplus::Point(x - icon_width/2 + center_offset_x, y + half_icon);
    triangle[2] = Gdiplus::Point(x + icon_width/2 + center_offset_x, y);
    
    Gdiplus::SolidBrush icon_brush(Gdiplus::Color(255, 0, 0, 0)); 
    graphics.FillPolygon(&icon_brush, triangle, 3);
}

void control_panel::draw_pause_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    
    // Background circle
    int bg_val = 32 + ((255 - 32) * opacity) / 100;
    int radius = size / 2;
    Gdiplus::SolidBrush bg_brush(Gdiplus::Color(255, bg_val, bg_val, bg_val));
    graphics.FillEllipse(&bg_brush, x - radius, y - radius, size, size);
    
    // Black bars
    int icon_height = size * 4 / 10;
    int half_icon = icon_height / 2;
    int bar_width = icon_height / 3;
    int gap = icon_height / 3;
    int offset = gap / 2;
    
    Gdiplus::SolidBrush icon_brush(Gdiplus::Color(255, 0, 0, 0));
    graphics.FillRectangle(&icon_brush, x - offset - bar_width, y - half_icon, bar_width, icon_height);
    graphics.FillRectangle(&icon_brush, x + offset, y - half_icon, bar_width, icon_height);
}

void control_panel::draw_previous_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    // Hover highlight removed per user request
    // if (m_hovered_button == BTN_PREV) {
    //     draw_hover_circle(hdc, x, y, size);
    // }
    
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    // Solid filled style (Triangle + Bar)
    int color_value = 32 + ((200 - 32) * opacity) / 100; 
    
    int half_size = size / 2;
    int icon_h = size * 6 / 10; 
    int bar_width = 3; 
    int gap = 2; 

    Gdiplus::SolidBrush brush(Gdiplus::Color(255, color_value, color_value, color_value));
    
    // Bar on LEFT
    graphics.FillRectangle(&brush, x - icon_h/2, y - icon_h/2, bar_width, icon_h);
    
    // Triangle pointing left
    int tri_start_x = x - icon_h/2 + bar_width + gap;
    Gdiplus::Point triangle[3];
    triangle[0] = Gdiplus::Point(x + icon_h/2, y - icon_h/2);   // Top right
    triangle[1] = Gdiplus::Point(x + icon_h/2, y + icon_h/2);   // Bottom right
    triangle[2] = Gdiplus::Point(tri_start_x, y);               // Left point
    
    graphics.FillPolygon(&brush, triangle, 3);
}

void control_panel::draw_next_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    // Hover highlight removed per user request
    // if (m_hovered_button == BTN_NEXT) {
    //     draw_hover_circle(hdc, x, y, size);
    // }
    
    Gdiplus::Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    // Solid filled style (Triangle + Bar)
    int color_value = 32 + ((200 - 32) * opacity) / 100;
    
    int icon_h = size * 6 / 10;
    int bar_width = 3;
    int gap = 2;
    
    Gdiplus::SolidBrush brush(Gdiplus::Color(255, color_value, color_value, color_value));
    
    // Triangle pointing right
    int tri_start_x = x - icon_h/2; 
    Gdiplus::Point triangle[3];
    triangle[0] = Gdiplus::Point(tri_start_x, y - icon_h/2);     // Top left
    triangle[1] = Gdiplus::Point(tri_start_x, y + icon_h/2);     // Bottom left
    triangle[2] = Gdiplus::Point(tri_start_x + icon_h - bar_width - gap, y); // Right point
    
    graphics.FillPolygon(&brush, triangle, 3);
    
    // Bar on RIGHT
    int bar_x = tri_start_x + icon_h - bar_width;
    graphics.FillRectangle(&brush, bar_x, y - icon_h/2, bar_width, icon_h);
}


// Font management methods
void control_panel::load_fonts() {
    cleanup_fonts();
    
    // Select fonts based on current display mode
    if (m_is_artwork_expanded) {
        // Expanded artwork mode
        if (get_expanded_use_custom_fonts()) {
            LOGFONT artist_lf = get_expanded_artist_font();
            LOGFONT track_lf = get_expanded_track_font();
            m_artist_font = CreateFontIndirect(&artist_lf);
            m_track_font = CreateFontIndirect(&track_lf);
        } else {
            LOGFONT artist_lf = get_default_font(true, 11);
            LOGFONT track_lf = get_default_font(false, 14);
            m_artist_font = CreateFontIndirect(&artist_lf);
            m_track_font = CreateFontIndirect(&track_lf);
        }
    } else if (m_is_compact_mode) {
        // Compact mode
        if (get_compact_use_custom_fonts()) {
            LOGFONT artist_lf = get_compact_artist_font();
            LOGFONT track_lf = get_compact_track_font();
            m_artist_font = CreateFontIndirect(&artist_lf);
            m_track_font = CreateFontIndirect(&track_lf);
        } else {
            LOGFONT artist_lf = get_default_font(true, 11);
            LOGFONT track_lf = get_default_font(false, 14);
            m_artist_font = CreateFontIndirect(&artist_lf);
            m_track_font = CreateFontIndirect(&track_lf);
        }
    } else if (m_is_undocked) {
        // Undocked mode
        if (get_undocked_use_custom_fonts()) {
            LOGFONT artist_lf = get_undocked_artist_font();
            LOGFONT track_lf = get_undocked_track_font();
            m_artist_font = CreateFontIndirect(&artist_lf);
            m_track_font = CreateFontIndirect(&track_lf);
        } else {
            LOGFONT artist_lf = get_default_font(true, 11);
            LOGFONT track_lf = get_default_font(false, 14);
            m_artist_font = CreateFontIndirect(&artist_lf);
            m_track_font = CreateFontIndirect(&track_lf);
        }
    } else {
        // Docked mode (default)
        if (get_cp_use_custom_fonts()) {
            LOGFONT artist_lf = get_cp_artist_font();
            LOGFONT track_lf = get_cp_track_font();
            m_artist_font = CreateFontIndirect(&artist_lf);
            m_track_font = CreateFontIndirect(&track_lf);
        } else {
            LOGFONT artist_lf = get_default_font(true, 13);
            LOGFONT track_lf = get_default_font(false, 16);
            m_artist_font = CreateFontIndirect(&artist_lf);
            m_track_font = CreateFontIndirect(&track_lf);
        }
    }
}

void control_panel::cleanup_fonts() {
    if (m_artist_font) {
        DeleteObject(m_artist_font);
        m_artist_font = nullptr;
    }
    if (m_track_font) {
        DeleteObject(m_track_font);
        m_track_font = nullptr;
    }
}

void control_panel::on_settings_changed() {
    // Reload fonts when settings change
    load_fonts();
    
    // Apply window corner preference
    apply_window_corner_preference();
    
    // Trigger repaint if visible
    if (m_visible && m_control_window) {
        InvalidateRect(m_control_window, nullptr, TRUE);
    }
}

void control_panel::apply_window_corner_preference() {
    if (!m_control_window) return;
    
    // DWMWA_WINDOW_CORNER_PREFERENCE = 33
    // DWMWCP_DONOTROUND = 1 (square corners)
    // DWMWCP_ROUND = 2 (rounded corners)
    // DWMWCP_ROUNDSMALL = 3 (small rounded corners)
    DWORD corner_pref = get_use_rounded_corners() ? 2 : 1;
    DwmSetWindowAttribute(m_control_window, 33, &corner_pref, sizeof(corner_pref));
}

void control_panel::set_undocked(bool undocked) {
    m_is_undocked = undocked;

    // Reset compact mode when switching undocked state
    if (undocked) {
        m_is_compact_mode = false; // Start in normal undocked mode, not compact
    } else {
        // Returning to docked mode - clear saved miniplayer state
        m_has_saved_miniplayer_state = false;
    }
    
    // Reload fonts for the new mode
    load_fonts();
    
    // When becoming undocked, schedule track info update (asynchronous)
    if (undocked && m_visible && m_control_window) {
        SetTimer(m_control_window, UPDATE_TIMER_ID + 2, 50, nullptr);
        
        // Enable mouse tracking when becoming undocked for button fade functionality
        TRACKMOUSEEVENT tme = {0};
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = m_control_window;
        TrackMouseEvent(&tme);
        
        // Reset button opacity and visibility for undocked mode
        m_buttons_visible = true;
        m_button_opacity = 100;
        KillTimer(m_control_window, BUTTON_FADE_TIMER_ID);
        KillTimer(m_control_window, BUTTON_FADE_TIMER_ID + 1);
    }
}

void control_panel::toggle_artwork_expanded() {
    if (!m_visible) return;
    
    // Save current window dimensions before switching modes
    RECT current_rect;
    GetWindowRect(m_control_window, &current_rect);
    int current_width = current_rect.right - current_rect.left;
    int current_height = current_rect.bottom - current_rect.top;
    
    if (m_is_artwork_expanded) {
        // Currently in expanded mode - save expanded dimensions and switch back to previous mode
        m_saved_expanded_width = current_width;
        m_saved_expanded_height = current_height;
        m_is_artwork_expanded = false;

        // Clear the saved miniplayer state since user manually changed modes
        m_has_saved_miniplayer_state = false;

        // Hide overlay when leaving expanded mode
        m_overlay_visible = false;
        m_overlay_opacity = 0;
        KillTimer(m_control_window, OVERLAY_TIMER_ID);
        KillTimer(m_control_window, FADE_TIMER_ID);
        
        // Restore previous mode (compact or normal undocked)
        m_is_compact_mode = m_was_compact_before_expanded;
        
        if (m_is_compact_mode) {
            // Return to compact mode
            int compact_height = 75;
            int compact_width = 320;
            SetWindowPos(m_control_window, HWND_TOPMOST, 0, 0, compact_width, compact_height, 
                SWP_NOMOVE | SWP_NOACTIVATE);
        } else {
            // Return to normal undocked mode
            SetWindowPos(m_control_window, HWND_TOPMOST, 0, 0, m_saved_undocked_width, m_saved_undocked_height, 
                SWP_NOMOVE | SWP_NOACTIVATE);
        }
        
        // Reload fonts for the mode we're returning to
        load_fonts();
        
        // Restart update timer for normal mini player functionality (use 500ms for responsive updates)
        SetTimer(m_control_window, UPDATE_TIMER_ID, 500, nullptr);
    } else {
        // Currently in undocked or compact mode - save state and switch to expanded mode
        m_was_compact_before_expanded = m_is_compact_mode; // Remember current mode
        
        if (m_is_compact_mode) {
            // Save compact mode dimensions (use saved normal dimensions for undocked restoration)
            m_saved_undocked_width = m_saved_normal_width;
            m_saved_undocked_height = m_saved_normal_height;
        } else {
            // Save normal undocked dimensions
            m_saved_undocked_width = current_width;
            m_saved_undocked_height = current_height;
        }
        
        m_is_compact_mode = false; // Disable compact mode when entering expanded mode
        m_is_artwork_expanded = true;
        
        // Load fonts for expanded mode
        load_fonts();
        
        // Stop timeout timer but keep update timer for track change detection
        KillTimer(m_control_window, TIMEOUT_TIMER_ID);
        
        // Restart update timer for track change detection (use 500ms for responsive updates)
        SetTimer(m_control_window, UPDATE_TIMER_ID, 500, nullptr);
        
        // Check if we have saved expanded dimensions, otherwise calculate based on artwork
        int window_width = m_saved_expanded_width;
        int window_height = m_saved_expanded_height;
        
        // If this is the first time entering expanded mode for this track, calculate initial size
        if (m_cover_art_bitmap_original && m_original_art_width > 0 && m_original_art_height > 0) {
            // Only recalculate if saved dimensions don't match artwork aspect ratio
            float image_aspect = (float)m_original_art_width / (float)m_original_art_height;
            float saved_aspect = (float)m_saved_expanded_width / (float)m_saved_expanded_height;
            
            // If aspect ratios are significantly different, recalculate for new artwork
            if (abs(image_aspect - saved_aspect) > 0.1f) {
                if (image_aspect >= 1.0f) {
                    // Landscape or square - set width to 400, calculate height
                    window_width = 400;
                    window_height = (int)(400.0f / image_aspect);
                } else {
                    // Portrait - set height to 400, calculate width
                    window_height = 400;
                    window_width = (int)(400.0f * image_aspect);
                }
                
                // Ensure minimum size
                if (window_width < 200) window_width = 200;
                if (window_height < 200) window_height = 200;
                
                // Update saved dimensions for next time
                m_saved_expanded_width = window_width;
                m_saved_expanded_height = window_height;
            }
        }
        
        // Resize window to saved or calculated expanded dimensions
        SetWindowPos(m_control_window, HWND_TOPMOST, 0, 0, window_width, window_height, 
            SWP_NOMOVE | SWP_NOACTIVATE);
        
        // Ensure we're undocked when in artwork mode
        if (!m_is_undocked) {
            set_undocked(true);
        }
    }
    
    // Trigger repaint
    InvalidateRect(m_control_window, nullptr, TRUE);
}

void control_panel::toggle_compact_mode() {
    if (!m_visible || !m_is_undocked || m_is_artwork_expanded) return;
    
    // Save current window dimensions before switching modes
    RECT current_rect;
    GetWindowRect(m_control_window, &current_rect);
    int current_width = current_rect.right - current_rect.left;
    int current_height = current_rect.bottom - current_rect.top;
    
    if (m_is_compact_mode) {
        // Currently in compact mode - switch back to normal undocked mode
        m_is_compact_mode = false;
        
        // Restore to saved normal dimensions
        SetWindowPos(m_control_window, HWND_TOPMOST, 0, 0, 
            m_saved_normal_width, m_saved_normal_height, 
            SWP_NOMOVE | SWP_NOACTIVATE);
    } else {
        // Currently in normal mode - switch to compact mode
        m_saved_normal_width = current_width;
        m_saved_normal_height = current_height;
        m_is_compact_mode = true;
        
        // Set compact dimensions (2cm height ≈ 75 pixels at 96 DPI, width reduced by 20%)
        int compact_height = 75;
        int compact_width = 320; // Reduced by 20% from 400px
        
        SetWindowPos(m_control_window, HWND_TOPMOST, 0, 0, 
            compact_width, compact_height, 
            SWP_NOMOVE | SWP_NOACTIVATE);
    }
    
    // Reload fonts for the new mode
    load_fonts();
    
    // Trigger repaint
    InvalidateRect(m_control_window, nullptr, TRUE);
}

void control_panel::handle_button_click(int button_id) {
    try {
        auto playback = playback_control::get();

        switch (button_id) {
        case BTN_PREV:
            playback->previous();
            // Delay update to allow track change to process
            SetTimer(m_control_window, UPDATE_TIMER_ID + 1, 100, nullptr);
            return; // Don't update immediately

        case BTN_PLAYPAUSE:
            if (m_is_playing) {
                if (m_is_paused) {
                    playback->pause(false); // Resume
                } else {
                    playback->pause(true); // Pause
                }
            } else {
                playback->play_or_unpause();
            }
            break;

        case BTN_NEXT:
            playback->next();
            // Delay update to allow track change to process
            SetTimer(m_control_window, UPDATE_TIMER_ID + 1, 100, nullptr);
            return; // Don't update immediately

        case BTN_VOLUME:
            {
                RECT rect;
                GetWindowRect(m_control_window, &rect);
                int window_x = rect.left;
                int window_y = rect.top;
                int width = rect.right - rect.left;
                int height = rect.bottom - rect.top;
                
                int btn_center_x = 0;
                int btn_center_y = 0;

                if (m_is_artwork_expanded) {
                    // Expanded mode overlay
                    const int overlay_height = 70;
                    int overlay_top = height - overlay_height;
                    // Formula from draw_control_overlay
                    int center_y = overlay_top + (overlay_height / 2) + (overlay_height * 28 / 100);
                    int center_x = width / 2;
                    int button_spacing = 60;
                    btn_center_x = center_x + button_spacing * 2;
                    btn_center_y = center_y + 12; 
                } else if (m_is_compact_mode) {
                     // Compact mode overlay
                    int margin = 5;
                    int art_size = height - (2 * margin);
                    int text_left = margin + art_size + margin;
                    int text_right = width - margin;
                    int text_area_width = text_right - text_left;
                    int button_size = 24;
                    int button_spacing = 8;
                    int total_buttons_width = (5 * button_size) + (4 * button_spacing);
                    
                    int buttons_start_x = text_left + (text_area_width - total_buttons_width) / 2;
                    int button_y = margin + (height - 2 * margin - button_size) / 2 + 5;
                    
                    int shuffle_x = buttons_start_x + button_size / 2;
                    int prev_x = shuffle_x + button_size + button_spacing;
                    int play_x = prev_x + button_size + button_spacing;
                    int next_x = play_x + button_size + button_spacing;
                    int repeat_x = next_x + button_size + button_spacing;
                    int volume_x = repeat_x; // Fallback since volume button isn't shown in compact mode
                    
                    btn_center_x = volume_x;
                    btn_center_y = button_y + button_size/2;
                } else {
                     // Normal/Docked logic
                     int button_y = height - 30;
                     int button_spacing = 50;
                     int center_x = width / 2;
                     btn_center_x = center_x + button_spacing * 2;
                     btn_center_y = button_y + 8;
                }
                
                volume_popup::get_instance().show_at(window_x + btn_center_x, window_y + btn_center_y);
            }
            return; // No track info update needed

        // BTN_CLOSE removed per user request
        // case BTN_CLOSE:
        //     m_has_saved_miniplayer_state = false;
        //     hide_control_panel_immediate();
        //     return;
            
        case BTN_SHUFFLE:
            {
                // Toggle shuffle mode using playlist_manager
                try {
                    auto playlist_api = playlist_manager::get();
                    t_size current_order = playlist_api->playback_order_get_active();
                    t_size order_count = playlist_api->playback_order_get_count();
                    
                    // Find shuffle orders (usually named "Shuffle" or contain "shuffle")
                    bool found_shuffle = false;
                    t_size shuffle_index = 0;
                    t_size default_index = 0; // Usually "Default" or first
                    
                    for (t_size i = 0; i < order_count; i++) {
                        const char* name = playlist_api->playback_order_get_name(i);
                        if (strstr(name, "Shuffle") || strstr(name, "shuffle") || strstr(name, "Random")) {
                            shuffle_index = i;
                            found_shuffle = true;
                            break;
                        }
                        if (strcmp(name, "Default") == 0 || i == 0) {
                            default_index = i;
                        }
                    }
                    
                    if (found_shuffle) {
                        // Toggle between shuffle and default
                        if (current_order == shuffle_index) {
                            playlist_api->playback_order_set_active(default_index);
                            m_shuffle_active = false;
                        } else {
                            playlist_api->playback_order_set_active(shuffle_index);
                            m_shuffle_active = true;
                        }
                    }
                } catch (...) {}
                
                InvalidateRect(m_control_window, nullptr, FALSE);
            }
            return;
            
        case BTN_REPEAT:
            {
                // Toggle repeat mode using playlist_manager
                // Cycle: Off (Default) -> Repeat (Playlist) -> Repeat (Track) -> Off
                try {
                    auto playlist_api = playlist_manager::get();
                    t_size order_count = playlist_api->playback_order_get_count();
                    
                    t_size default_index = 0;
                    t_size repeat_playlist_index = 0;
                    t_size repeat_track_index = 0;
                    
                    bool found_repeat_playlist = false;
                    bool found_repeat_track = false;
                    
                    for (t_size i = 0; i < order_count; i++) {
                        const char* name = playlist_api->playback_order_get_name(i);
                        if (strcmp(name, "Default") == 0) {
                            default_index = i;
                        } else if (strcmp(name, "Repeat (playlist)") == 0 || strcmp(name, "Repeat") == 0) {
                            repeat_playlist_index = i;
                            found_repeat_playlist = true;
                        } else if (strcmp(name, "Repeat (track)") == 0 || strcmp(name, "Repeat (one)") == 0 || strcmp(name, "Repeat (1)") == 0) {
                            repeat_track_index = i;
                            found_repeat_track = true;
                        }
                    }
                    
                    if (found_repeat_playlist) {
                        if (m_repeat_mode == 0) {
                            // Switch to Playlist Repeat
                            playlist_api->playback_order_set_active(repeat_playlist_index);
                            m_repeat_mode = 1;
                        } else if (m_repeat_mode == 1) {
                             // Switch to Track Repeat (if available), else Off
                            if (found_repeat_track) {
                                playlist_api->playback_order_set_active(repeat_track_index);
                                m_repeat_mode = 2;
                            } else {
                                playlist_api->playback_order_set_active(default_index);
                                m_repeat_mode = 0;
                            }
                        } else {
                            // Switch to Off
                            playlist_api->playback_order_set_active(default_index);
                            m_repeat_mode = 0;
                        }
                    }
                } catch (...) {}
                
                InvalidateRect(m_control_window, nullptr, FALSE);
            }
            return;
        }

        // Update display after action
        update_track_info();

    } catch (...) {
        // Ignore playback control errors
    }
}

void control_panel::update_playback_order_state() {
    try {
        auto playlist_api = playlist_manager::get();
        t_size current_order = playlist_api->playback_order_get_active();
        const char* order_name = playlist_api->playback_order_get_name(current_order);
        
        // Check if current order is a shuffle mode
        m_shuffle_active = (strstr(order_name, "Shuffle") != nullptr || 
                           strstr(order_name, "shuffle") != nullptr ||
                           strstr(order_name, "Random") != nullptr);
        
        // Check if current order is a repeat mode
        m_repeat_mode = 0; // Default off
        if (strcmp(order_name, "Repeat (track)") == 0 || strcmp(order_name, "Repeat (one)") == 0 || strcmp(order_name, "Repeat (1)") == 0) {
            m_repeat_mode = 2; // Track repeat
        } else if (strstr(order_name, "Repeat") != nullptr || strstr(order_name, "repeat") != nullptr) {
            m_repeat_mode = 1; // Playlist repeat (default fallback for any other repeat string)
        }
    } catch (...) {
        m_shuffle_active = false;
        m_repeat_mode = 0;
    }
}

void control_panel::handle_timer() {
    // Use simple behavior (like original) when in basic popup mode
    if (!m_is_undocked && !m_is_artwork_expanded) {
        // Original simple timer behavior - but also check for stream metadata changes
        try {
            auto playback = playback_control::get();
            m_current_time = playback->playback_get_position();
            m_is_playing = playback->is_playing();
            m_is_paused = playback->is_paused();
            
            // For streams in docked mode, also check for metadata changes
            metadb_handle_ptr current_track;
            if (playback->get_now_playing(current_track) && current_track.is_valid()) {
                pfc::string8 path = current_track->get_path();
                bool is_stream = strstr(path.get_ptr(), "://") != nullptr;
                
                if (is_stream) {
                    // Check if stream metadata has changed
                    static pfc::string8 last_docked_title, last_docked_artist;
                    pfc::string8 current_title, current_artist;
                    
                    // Use titleformat to get current stream metadata
                    try {
                        static_api_ptr_t<titleformat_compiler> compiler;
                        service_ptr_t<titleformat_object> script;
                        
                        if (compiler->compile(script, "[%artist%]|[%title%]")) {
                            pfc::string8 formatted_title;
                            if (playback->playback_format_title(nullptr, formatted_title, script, nullptr, playback_control::display_level_all)) {
                                const char* separator = strstr(formatted_title.get_ptr(), "|");
                                if (separator && strlen(formatted_title.get_ptr()) > 1) {
                                    pfc::string8 tf_artist(formatted_title.get_ptr(), separator - formatted_title.get_ptr());
                                    pfc::string8 tf_title(separator + 1);
                                    
                                    if (!tf_artist.is_empty() && !tf_title.is_empty()) {
                                        current_artist = tf_artist;
                                        current_title = tf_title;
                                    }
                                }
                            }
                        }
                    } catch (...) {
                        // Fall through to basic metadata
                    }
                    
                    // Check if metadata changed
                    if (current_title != last_docked_title || current_artist != last_docked_artist) {
                        last_docked_title = current_title;
                        last_docked_artist = current_artist;
                        update_track_info(); // Update track info for stream metadata change
                        return;
                    }
                }
            }
            
            // Refresh time display
            if (m_control_window) {
                InvalidateRect(m_control_window, nullptr, FALSE);
            }
        } catch (...) {
            // Ignore errors
        }
        return;
    }
    
    // Complex behavior for undocked/artwork modes
    bool skip_time_updates = m_is_artwork_expanded;
    
    // For undocked mini player or artwork expanded mode, check for track changes every timer tick
    if (m_is_undocked || m_is_artwork_expanded) {
        // Check if track has actually changed by comparing track handles
        static metadb_handle_ptr last_track;
        static pfc::string8 last_title, last_artist;
        
        try {
            auto playback = playback_control::get();
            metadb_handle_ptr current_track;
            if (playback->get_now_playing(current_track) && current_track.is_valid()) {
                // Compare track handle first
                bool track_changed = !last_track.is_valid() || current_track.get_ptr() != last_track.get_ptr();
                
                // Also compare metadata for streams that might not change track handle
                pfc::string8 current_title, current_artist;
                
                // Check if this is a stream
                pfc::string8 path = current_track->get_path();
                bool is_stream = strstr(path.get_ptr(), "://") != nullptr;
                
                if (is_stream) {
                    // For streams, use titleformat to get current metadata (same as update_track_info)
                    try {
                        static_api_ptr_t<titleformat_compiler> compiler;
                        service_ptr_t<titleformat_object> script;
                        
                        if (compiler->compile(script, "[%artist%]|[%title%]")) {
                            pfc::string8 formatted_title;
                            if (playback->playback_format_title(nullptr, formatted_title, script, nullptr, playback_control::display_level_all)) {
                                const char* separator = strstr(formatted_title.get_ptr(), "|");
                                if (separator && strlen(formatted_title.get_ptr()) > 1) {
                                    pfc::string8 tf_artist(formatted_title.get_ptr(), separator - formatted_title.get_ptr());
                                    pfc::string8 tf_title(separator + 1);
                                    
                                    if (!tf_artist.is_empty() && !tf_title.is_empty()) {
                                        current_artist = tf_artist;
                                        current_title = tf_title;
                                    }
                                }
                            }
                        }
                    } catch (...) {
                        // Fall through to basic metadata
                    }
                }
                
                // If titleformat didn't work or not a stream, use basic metadata
                if (current_title.is_empty() || current_artist.is_empty()) {
                    file_info_impl info;
                    if (current_track->get_info(info)) {
                        const char* title_str = info.meta_get("TITLE", 0);
                        const char* artist_str = info.meta_get("ARTIST", 0);
                        if (title_str && current_title.is_empty()) current_title = title_str;
                        if (artist_str && current_artist.is_empty()) current_artist = artist_str;
                    }
                }
                
                bool metadata_changed = (current_title != last_title) || (current_artist != last_artist);
                
                if (track_changed || metadata_changed) {
                    // Track or metadata changed - force full update
                    last_track = current_track;
                    last_title = current_title;
                    last_artist = current_artist;
                    update_track_info(); // Full update including artwork
                    return;
                }
            } else if (last_track.is_valid()) {
                // Playback stopped
                last_track.release();
                last_title = "";
                last_artist = "";
                update_track_info();
                return;
            }
        } catch (...) {
            // If we can't get track info, force update anyway
            update_track_info();
            return;
        }
    }
    
    // Otherwise just update current playback position (skip in artwork expanded mode)
    if (!skip_time_updates) {
        try {
            auto playback = playback_control::get();
            m_current_time = playback->playback_get_position();
            m_is_playing = playback->is_playing();
            m_is_paused = playback->is_paused();
            
            // Refresh time display
            if (m_control_window) {
                InvalidateRect(m_control_window, nullptr, FALSE);
            }
        } catch (...) {
            // Ignore errors
        }
    }
}

void control_panel::start_slide_out_animation() {
    if (m_animating || !m_visible) {
        return;
    }
    
    // Get current position as start position
    RECT window_rect;
    GetWindowRect(m_control_window, &window_rect);
    m_start_x = window_rect.left;
    m_start_y = window_rect.top;
    
    // Set final position (off-screen to the right)
    m_final_x = GetSystemMetrics(SM_CXSCREEN);
    m_final_y = m_start_y;
    
    // Stop other timers
    KillTimer(m_control_window, UPDATE_TIMER_ID);
    KillTimer(m_control_window, UPDATE_TIMER_ID + 1);
    KillTimer(m_control_window, TIMEOUT_TIMER_ID);
    
    m_animating = true;
    m_closing = true;
    m_animation_step = 0;
    
    // Start animation timer
    SetTimer(m_control_window, ANIMATION_TIMER_ID, ANIMATION_DURATION / ANIMATION_STEPS, nullptr);
}

void control_panel::update_animation() {
    if (!m_animating) {
        return;
    }
    
    m_animation_step++;
    
    if (m_animation_step >= ANIMATION_STEPS) {
        // Animation complete
        m_animating = false;
        KillTimer(m_control_window, ANIMATION_TIMER_ID);
        
        if (m_closing) {
            // Actually hide the window now
            ShowWindow(m_control_window, SW_HIDE);
            m_visible = false;
            m_closing = false;
        }
    } else {
        // Calculate current position using ease-out curve
        float progress = (float)m_animation_step / ANIMATION_STEPS;
        float eased_progress = 1.0f - (1.0f - progress) * (1.0f - progress); // Ease-out quadratic
        
        int current_x = m_start_x + (int)((m_final_x - m_start_x) * eased_progress);
        int current_y = m_start_y + (int)((m_final_y - m_start_y) * eased_progress);
        
        // Move window to current position
        SetWindowPos(m_control_window, HWND_TOPMOST, current_x, current_y, 0, 0, 
                     SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void control_panel::slide_to_side() {
    if (m_sliding_animation || m_is_slid_to_side || !m_control_window || !m_visible || get_disable_slide_to_side()) {
        return;
    }
    
    // Get current window position
    RECT window_rect;
    GetWindowRect(m_control_window, &window_rect);
    int window_width = window_rect.right - window_rect.left;
    
    // Save current position for slide-back
    m_pre_slide_x = window_rect.left;
    m_pre_slide_y = window_rect.top;
    
    // Calculate target position: right edge of screen with 70px peek visible
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    const int PEEK_AMOUNT = 70;
    
    m_slide_start_x = window_rect.left;
    m_slide_target_x = screen_width - PEEK_AMOUNT;
    
    // Start slide animation
    m_sliding_animation = true;
    m_sliding_to_side = true;
    m_slide_animation_step = 0;
    
    SetTimer(m_control_window, SLIDE_TIMER_ID, SLIDE_ANIMATION_DURATION / SLIDE_ANIMATION_STEPS, nullptr);
}

void control_panel::slide_back_from_side() {
    if (m_sliding_animation || !m_is_slid_to_side || !m_control_window || !m_visible) {
        return;
    }
    
    // Get current window position
    RECT window_rect;
    GetWindowRect(m_control_window, &window_rect);
    
    m_slide_start_x = window_rect.left;
    m_slide_target_x = m_pre_slide_x;
    
    // Start slide animation
    m_sliding_animation = true;
    m_sliding_to_side = false;
    m_slide_animation_step = 0;
    
    SetTimer(m_control_window, SLIDE_TIMER_ID, SLIDE_ANIMATION_DURATION / SLIDE_ANIMATION_STEPS, nullptr);
}

void control_panel::update_slide_animation() {
    if (!m_sliding_animation) {
        return;
    }
    
    m_slide_animation_step++;
    
    if (m_slide_animation_step >= SLIDE_ANIMATION_STEPS) {
        // Animation complete
        m_sliding_animation = false;
        KillTimer(m_control_window, SLIDE_TIMER_ID);
        
        if (m_sliding_to_side) {
            m_is_slid_to_side = true;
        } else {
            m_is_slid_to_side = false;
        }
        
        // Final position
        RECT window_rect;
        GetWindowRect(m_control_window, &window_rect);
        int window_height = window_rect.bottom - window_rect.top;
        int window_width = window_rect.right - window_rect.left;
        
        SetWindowPos(m_control_window, HWND_TOPMOST, m_slide_target_x, window_rect.top, 
                     window_width, window_height, SWP_NOACTIVATE);
    } else {
        // Calculate current position using ease-out curve
        float progress = (float)m_slide_animation_step / SLIDE_ANIMATION_STEPS;
        float eased_progress = 1.0f - (1.0f - progress) * (1.0f - progress); // Ease-out quadratic
        
        int current_x = m_slide_start_x + (int)((m_slide_target_x - m_slide_start_x) * eased_progress);
        
        // Move window to current position
        SetWindowPos(m_control_window, HWND_TOPMOST, current_x, m_pre_slide_y, 0, 0, 
                     SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

LRESULT CALLBACK control_panel::control_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    control_panel* panel = nullptr;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
        panel = reinterpret_cast<control_panel*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
    } else {
        panel = reinterpret_cast<control_panel*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (panel) {
        switch (msg) {
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                
                // Double-buffering to eliminate flickering
                RECT client_rect;
                GetClientRect(hwnd, &client_rect);
                int width = client_rect.right - client_rect.left;
                int height = client_rect.bottom - client_rect.top;
                
                // Create off-screen buffer
                HDC mem_dc = CreateCompatibleDC(hdc);
                HBITMAP mem_bitmap = CreateCompatibleBitmap(hdc, width, height);
                HBITMAP old_bitmap = (HBITMAP)SelectObject(mem_dc, mem_bitmap);
                
                // Paint to off-screen buffer
                panel->paint_control_panel(mem_dc);
                
                // Copy to screen
                BitBlt(hdc, 0, 0, width, height, mem_dc, 0, 0, SRCCOPY);
                
                // Cleanup
                SelectObject(mem_dc, old_bitmap);
                DeleteObject(mem_bitmap);
                DeleteDC(mem_dc);
                
                EndPaint(hwnd, &ps);
                return 0;
            }
            
        case WM_LBUTTONDOWN:
            // Handle compact mode click
            if (panel && panel->m_is_compact_mode) {
                POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                RECT client_rect;
                GetClientRect(hwnd, &client_rect);
                int window_width = client_rect.right - client_rect.left;
                int window_height = client_rect.bottom - client_rect.top;
                
                // Slide-to-side: Check for click on right edge (last 40px) or slide back if already slid
                const int SLIDE_CLICK_ZONE = 40;
                if (panel->m_is_slid_to_side) {
                    // If slid, any click brings it back
                    panel->slide_back_from_side();
                    return 0;
                } else if (pt.x >= window_width - SLIDE_CLICK_ZONE) {
                    // Right edge clicked - slide to side
                    panel->slide_to_side();
                    return 0;
                }
                
                // Check if click is on artwork in compact mode
                int margin = 5;
                int art_size = window_height - (2 * margin);
                int art_x = margin;
                int art_y = margin;
                
                if (pt.x >= art_x && pt.x < art_x + art_size && pt.y >= art_y && pt.y < art_y + art_size) {
                    // Click on artwork in compact mode - expand to artwork mode
                    if (panel && panel->m_visible && !panel->m_animating) {
                        panel->toggle_artwork_expanded();
                        return 0;
                    }
                }
                
                // Check if click is on progress bar in compact mode
                int text_left = margin + art_size + margin;
                int text_right = window_width - margin;
                int progress_bar_height = 5;
                int progress_bar_y = window_height - progress_bar_height - 2 - (int)(window_height * 0.1);
                int progress_bar_left = text_left;
                int progress_bar_width = text_right - text_left - 40; // Leave space for time display
                
                if (pt.x >= progress_bar_left && pt.x < progress_bar_left + progress_bar_width &&
                    pt.y >= progress_bar_y && pt.y < progress_bar_y + progress_bar_height) {
                    // Click on progress bar - seek to clicked position
                    if (panel->m_track_length > 0) {
                        double click_ratio = (double)(pt.x - progress_bar_left) / (double)progress_bar_width;
                        if (click_ratio < 0.0) click_ratio = 0.0;
                        if (click_ratio > 1.0) click_ratio = 1.0;
                        
                        double seek_time = click_ratio * panel->m_track_length;
                        
                        // Use foobar2000's playback control API to seek
                        static_api_ptr_t<playback_control> pc;
                        if (pc->is_playing()) {
                            pc->playback_seek(seek_time);
                        }
                    }
                    return 0;
                }
                
                // Check if click is on compact control overlay buttons
                if (panel->m_compact_controls_visible) {
                    // Calculate button positions (same as in draw_compact_control_overlay)
                    int text_left = margin + art_size + margin;
                    int text_right = window_width - margin;
                    int text_area_width = text_right - text_left;

                    int text_top = 0;
                    int text_bottom = window_height;
                    int text_area_height = text_bottom - text_top;

                    int button_size = 24; 
                    int play_button_size = 36;
                    int button_spacing = 10;
                    
                    int total_buttons_width = (4 * button_size) + play_button_size + (4 * button_spacing);
                    
                    // Must match the drawing offset (-15) for proper click detection
                    int buttons_start_x = text_left + (text_area_width - total_buttons_width) / 2 - 15;
                    
                    // Button Y center line - must match drawing calculation using overlay_bottom
                    int progress_bar_height = 5;
                    int overlay_bottom = window_height - progress_bar_height - 2 - (int)(window_height * 0.1) - 8;
                    int center_y_line = (overlay_bottom / 2) + 5;
                    int button_y = center_y_line;
                    
                    int shuffle_x = buttons_start_x + button_size / 2;
                    int prev_x = shuffle_x + button_size/2 + button_spacing + button_size/2;
                    int play_x = prev_x + button_size/2 + button_spacing + play_button_size/2;
                    int next_x = play_x + play_button_size/2 + button_spacing + button_size/2;
                    int repeat_x = next_x + button_size/2 + button_spacing + button_size/2;

                    // Check which button was clicked
                    // Use different radius for play vs others
                    int click_radius = button_size / 2 + 2; 
                    int play_click_radius = play_button_size / 2 + 2;

                    if (abs(pt.x - shuffle_x) <= click_radius && abs(pt.y - button_y) <= click_radius) {
                        panel->handle_button_click(BTN_SHUFFLE);
                        return 0;
                    } else if (abs(pt.x - prev_x) <= click_radius && abs(pt.y - button_y) <= click_radius) {
                        panel->handle_button_click(BTN_PREV);
                        return 0;
                    } else if (abs(pt.x - play_x) <= play_click_radius && abs(pt.y - button_y) <= play_click_radius) {
                        panel->handle_button_click(BTN_PLAYPAUSE);
                        return 0;
                    } else if (abs(pt.x - next_x) <= click_radius && abs(pt.y - button_y) <= click_radius) {
                        panel->handle_button_click(BTN_NEXT);
                        return 0;
                    } else if (abs(pt.x - repeat_x) <= click_radius && abs(pt.y - button_y) <= click_radius) {
                        panel->handle_button_click(BTN_REPEAT); 
                        return 0;
                    } else {
                        // Click in text area but not on buttons - initiate dragging
                        int text_left = margin + art_size + margin;
                        int text_right = window_width - margin;
                        if (pt.x >= text_left && pt.x < text_right) {
                            // Start dragging the window
                            ReleaseCapture();
                            SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, lparam);
                            return 0;
                        }
                    }
                }
                
                // Note: Clicks outside text area in compact mode fall through to default handling
                // Double-clicks are handled by WM_NCLBUTTONDBLCLK
                
                // Fall through to default handling
                return DefWindowProc(hwnd, msg, wparam, lparam);
            }
            
            // Handle normal undocked mode clicks (non-compact, non-expanded)
            // Collapse triangle in top-right corner and artwork clicks reach here via HTCLIENT
            if (panel && panel->m_is_undocked && !panel->m_is_compact_mode && !panel->m_is_artwork_expanded) {
                POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                RECT client_rect;
                GetClientRect(hwnd, &client_rect);
                int window_width = client_rect.right - client_rect.left;

                // Slide-to-side: Check for click on right edge (last 40px) or slide back if already slid
                // Exclude top-right corner (collapse triangle area)
                const int SLIDE_CLICK_ZONE = 40;
                if (panel->m_is_slid_to_side) {
                    // If slid, any click brings it back
                    panel->slide_back_from_side();
                    return 0;
                } else if (pt.x >= window_width - SLIDE_CLICK_ZONE && pt.y > 40) {
                    // Right edge clicked (below collapse triangle) - slide to side
                    panel->slide_to_side();
                    return 0;
                }

                // Check for collapse triangle click in top-right corner
                if (pt.x >= window_width - 40 && pt.y <= 40) {
                    // Switch from undocked to compact mode
                    panel->toggle_compact_mode();
                    return 0;
                }

                // Fall through to default handling for other clicks (artwork, etc.)
            }

            if (panel && panel->m_is_artwork_expanded) {
                // Handle clicks in expanded artwork mode
                POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                
                // Check if click is not on resize borders
                RECT client_rect;
                GetClientRect(hwnd, &client_rect);
                const int resize_border = 8;
                
                bool at_border = pt.x < resize_border || pt.x > client_rect.right - resize_border ||
                               pt.y < resize_border || pt.y > client_rect.bottom - resize_border;
                
                if (!at_border) {
                    // Top-left corner click removed per user request - only collapse triangle works now
                    
                    // Check for COLLAPSE triangle in top-right (new logic)
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    int window_width = client_rect.right - client_rect.left;
                    int window_height = client_rect.bottom - client_rect.top;
                    if (panel->m_overlay_visible && pt.x >= window_width - 40 && pt.y <= 40) {
                        panel->m_is_artwork_expanded = false;
                        
                        // Restore undocked size
                        SetWindowPos(hwnd, NULL, 0, 0, panel->m_saved_undocked_width, panel->m_saved_undocked_height, SWP_NOMOVE | SWP_NOZORDER);
                        InvalidateRect(hwnd, NULL, TRUE);
                        return 0;
                    }
                    
                    // Slide-to-side: Check for click on right edge or slide back if already slid
                    // Right edge zone is between resize_border and SLIDE_CLICK_ZONE from right edge
                    // Exclude top-right corner (collapse triangle) and bottom overlay
                    const int SLIDE_CLICK_ZONE = 40;
                    const int overlay_height = 70;
                    if (panel->m_is_slid_to_side) {
                        // If slid, any click brings it back
                        panel->slide_back_from_side();
                        return 0;
                    } else if (pt.x >= window_width - SLIDE_CLICK_ZONE - resize_border && 
                               pt.x < window_width - resize_border &&
                               pt.y > 40 && pt.y < window_height - overlay_height) {
                        // Right edge clicked (below collapse triangle, above control overlay) - slide to side
                        panel->slide_to_side();
                        return 0;
                    }

                    // Then check for control button clicks in bottom overlay area (overlay_height already defined above)
                    
                    if (panel->m_overlay_visible && pt.y >= window_height - overlay_height) {

                        // Calculate button positions (same as in draw_control_overlay)
                        int button_size = 24;
                        int button_spacing = 60;
                        int center_x = window_width / 2;
                        // Calculate center of the bottom overlay area, then lower for better visual balance
                        int overlay_top = window_height - overlay_height;
                        int center_y = overlay_top + (overlay_height / 2) + (overlay_height * 28 / 100); // Lower by 28%

                        // Check Previous button - use larger click area covering more of the overlay
                        int prev_x = center_x - button_spacing;
                        int click_area_size = button_size + 8; // Expand click area
                        // Use the same center_y for click detection as visual positioning
                        int click_center_y = center_y;
                        
                        // Check Shuffle button (left of Previous)
                        int shuffle_x = center_x - button_spacing * 2;
                        if (abs(pt.x - shuffle_x) <= click_area_size/2 && abs(pt.y - click_center_y) <= overlay_height/2) {
                            panel->handle_button_click(BTN_SHUFFLE);
                            return 0;
                        }
                        
                        if (abs(pt.x - prev_x) <= click_area_size/2 && abs(pt.y - click_center_y) <= overlay_height/2) {
                            panel->handle_button_click(BTN_PREV);
                            return 0;
                        }

                        // Check Play/Pause button
                        int play_x = center_x;
                        if (abs(pt.x - play_x) <= click_area_size/2 && abs(pt.y - click_center_y) <= overlay_height/2) {
                            panel->handle_button_click(BTN_PLAYPAUSE);
                            return 0;
                        }

                        // Check Next button
                        int next_x = center_x + button_spacing;
                        if (abs(pt.x - next_x) <= click_area_size/2 && abs(pt.y - click_center_y) <= overlay_height/2) {
                            panel->handle_button_click(BTN_NEXT);
                            return 0;
                        }
                        
                        // Check Repeat button (right of Next)
                        int repeat_x = center_x + button_spacing * 2;
                        if (abs(pt.x - repeat_x) <= click_area_size/2 && abs(pt.y - click_center_y) <= overlay_height/2) {
                            panel->handle_button_click(BTN_REPEAT);
                            return 0;
                        }
                    }
                    
                    // Double-click to toggle mode is disabled
                    // Previous behavior: double-click would toggle artwork expanded mode
                    
                    // Store this click for potential dragging, but don't start dragging yet
                    // Dragging will only start when mouse actually moves in WM_MOUSEMOVE
                    GetCursorPos(&panel->m_drag_start_pos);
                }
                return 0; // Don't fall through - handle only in expanded artwork mode
            }
            // Handle clicks in non-expanded modes
            {
                POINT pt = {LOWORD(lparam), HIWORD(lparam)};


                // Temporarily disabled timeout reset to test if continuous mouse events are interfering
                /*
                if (panel && panel->m_visible && panel->m_control_window && !panel->m_animating) {
                    KillTimer(panel->m_control_window, TIMEOUT_TIMER_ID);
                    SetTimer(panel->m_control_window, TIMEOUT_TIMER_ID, 5000, nullptr);
                }
                */

                // Close button removed per user request - no longer handling mouse hover for it
                
                if (panel->m_is_undocked && !panel->m_is_artwork_expanded && !panel->m_is_compact_mode) {
                    // Check for COLLAPSE triangle (Top-Right)
                    // Action: Switch to Compact Mode
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    int local_window_width = client_rect.right - client_rect.left;
                    
                    if (pt.x >= local_window_width - 30 && pt.y <= 30) {
                        panel->toggle_compact_mode();
                        return 0;
                    }
                }

                // Check which button was clicked - adaptive to window size
                RECT client_rect;
                GetClientRect(hwnd, &client_rect);
                int window_width = client_rect.right - client_rect.left;
                int window_height = client_rect.bottom - client_rect.top;

                int button_y = window_height - 30;
                
                // Spacing depends on mode (must match paint logic)
                // Undocked: 40, Docked: 60
                int button_spacing = (panel->m_is_undocked) ? 40 : 60;
                
                // Mirror paint logic: center in area right of artwork
                // Use same dynamic art_size calculation as paint function
                int art_size = (80 < (window_width - 30) ? 80 : (window_width - 30));
                art_size = (art_size < (window_height - 30) ? art_size : (window_height - 30));
                int button_area_left = 15 + art_size + 10;
                int button_area_width = window_width - button_area_left - 10;
                int center_x = button_area_left + button_area_width / 2;
                
                // Check if click is in button row (±20px around button_y for 40px tall area)
                if (pt.y >= button_y - 20 && pt.y <= button_y + 20) {
                    int click_radius = 15; // Default radius for smaller buttons
                    // Larger radius for play button
                    int play_click_radius = 20;
                    
                    // Check buttons presence logic
                    
                    if (abs(pt.x - center_x) <= play_click_radius) {
                        panel->handle_button_click(BTN_PLAYPAUSE);
                        return 0;
                    } 
                    else if (abs(pt.x - (center_x - button_spacing)) <= click_radius) {
                        panel->handle_button_click(BTN_PREV);
                        return 0;
                    }
                    else if (abs(pt.x - (center_x + button_spacing)) <= click_radius) {
                        panel->handle_button_click(BTN_NEXT);
                        return 0;
                    }
                    else if (panel->m_is_undocked) {
                         // Only check Shuffle/Repeat if undocked
                         if (abs(pt.x - (center_x - button_spacing * 2)) <= click_radius) {
                             panel->handle_button_click(BTN_SHUFFLE);
                             return 0;
                         }
                         else if (abs(pt.x - (center_x + button_spacing * 2)) <= click_radius) {
                             panel->handle_button_click(BTN_REPEAT);
                             return 0;
                         }
                    }
                }
                // Close button removed per user request - no longer needed
                // Check if click is in album art area
                if (panel->m_is_artwork_expanded) {
                    // In artwork expanded mode, single clicks do nothing (allows dragging)
                    return 0;
                }
                else {
                    // Check if click is in album art area
                    int art_size, art_x, art_y;
                    if (panel->m_is_compact_mode) {
                        // Compact mode artwork positioning
                        int margin = 5;
                        art_size = window_height - (2 * margin);
                        art_x = margin;
                        art_y = margin;
                    } else {
                        // Normal undocked mode artwork positioning
                        art_size = (80 < (window_width - 30) ? 80 : (window_width - 30));
                        art_size = (art_size < (window_height - 30) ? art_size : (window_height - 30));
                        art_x = 15;
                        art_y = 15;
                    }
                    
                    if (pt.x >= art_x && pt.x < art_x + art_size && pt.y >= art_y && pt.y < art_y + art_size) {
                        // Click on album art
                        if (panel && panel->m_visible && !panel->m_animating) {
                            if (!panel->m_is_undocked) {
                                // If docked, slide the panel away
                                panel->hide_control_panel();
                            } else {
                                // If undocked or compact, expand artwork
                                panel->toggle_artwork_expanded();
                            }
                        }
                    }
                }
                
                
                return 0;
            }
            break;
            
        case WM_LBUTTONUP:
            if (panel && panel->m_is_dragging && panel->m_is_artwork_expanded) {
                // Stop dragging
                panel->m_is_dragging = false;
                ReleaseCapture();
            }
            return 0;
            
        case WM_NCLBUTTONDOWN:
            // Double-click to toggle mode via NCLBUTTONDOWN is disabled
            // Let default processing handle dragging
            break;
            
        case WM_NCLBUTTONDBLCLK:
            // Double-click to toggle mode is disabled
            // Previous behavior: double-click on caption would switch between expanded/compact/normal modes
            return 0;
            
        case WM_LBUTTONDBLCLK:
            // Double-click to toggle mode is disabled
            // Previous behavior: double-click would switch between expanded/compact/normal modes
            return 0;
            
        case WM_MOUSEMOVE:
            if (panel && panel->m_is_artwork_expanded) {
                // Check if we should start dragging based on mouse movement
                if (panel->m_is_dragging) {
                    // Already dragging - continue moving window
                    POINT current_pos;
                    GetCursorPos(&current_pos);
                    
                    // Calculate the movement delta
                    int dx = current_pos.x - panel->m_drag_start_pos.x;
                    int dy = current_pos.y - panel->m_drag_start_pos.y;
                    
                    // Get current window position
                    RECT window_rect;
                    GetWindowRect(hwnd, &window_rect);
                    
                    // Move window
                    SetWindowPos(hwnd, nullptr, 
                                window_rect.left + dx, window_rect.top + dy, 
                                0, 0, SWP_NOSIZE | SWP_NOZORDER);
                    
                    // Update drag start position for next move
                    panel->m_drag_start_pos = current_pos;
                } else if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                    // Left button is held down - check if we should start dragging
                    POINT current_pos;
                    GetCursorPos(&current_pos);
                    
                    // Calculate distance from click position
                    int dx = abs(current_pos.x - panel->m_drag_start_pos.x);
                    int dy = abs(current_pos.y - panel->m_drag_start_pos.y);
                    
                    // Start dragging if moved more than 3 pixels (prevents accidental drags)
                    if (dx > 3 || dy > 3) {
                        panel->m_is_dragging = true;
                        SetCapture(hwnd);
                        panel->m_drag_start_pos = current_pos;
                    }
                } else {
                    // Show overlay on mouse movement in expanded artwork mode
                    if (!panel->m_overlay_visible) {
                        panel->m_overlay_visible = true;
                        panel->m_overlay_opacity = 100; // Full opacity immediately on mouse move
                        KillTimer(hwnd, FADE_TIMER_ID); // Stop any fade animation
                        // In artwork expanded mode, only invalidate overlay areas to prevent artwork flicker
                        InvalidateRect(hwnd, nullptr, FALSE);
                    } else {
                        // Reset to full opacity if already visible
                        panel->m_overlay_opacity = 100;
                        KillTimer(hwnd, FADE_TIMER_ID); // Stop any fade animation
                    }
                    
                    // Update last mouse move time and start/reset overlay timer
                    panel->m_last_mouse_move_time = GetTickCount();
                    SetTimer(hwnd, OVERLAY_TIMER_ID, 2000, nullptr); // Start fade after 2 seconds of no mouse movement
                    
                    // Track mouse leave events
                    TRACKMOUSEEVENT tme = {0};
                    tme.cbSize = sizeof(TRACKMOUSEEVENT);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = hwnd;
                    TrackMouseEvent(&tme);
                }
            } else if (panel && panel->m_is_undocked && !panel->m_is_artwork_expanded && (!panel->m_is_dragging || panel->m_is_compact_mode)) {
                // Handle mouse movement in undocked and compact modes for artwork overlay
                // For compact mode, continue handling even while dragging to keep controls visible
                POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                RECT client_rect;
                GetClientRect(hwnd, &client_rect);
                int window_width = client_rect.right - client_rect.left;
                int window_height = client_rect.bottom - client_rect.top;
                
                if (panel->m_is_compact_mode) {
                    // In compact mode, show controls whenever mouse is over panel (except artwork)
                    int margin = 5;
                    int art_size = window_height - (2 * margin);
                    int art_x = margin;
                    int art_y = margin;
                    
                    bool over_artwork = (pt.x >= art_x && pt.x <= art_x + art_size && 
                                       pt.y >= art_y && pt.y <= art_y + art_size);
                    
                    if (over_artwork) {
                        // Show artwork overlay (existing functionality)
                        bool state_changed = false;
                        if (!panel->m_undocked_overlay_visible) {
                            panel->m_undocked_overlay_visible = true;
                            panel->m_undocked_overlay_opacity = 100;
                            KillTimer(hwnd, FADE_TIMER_ID + 1);
                            KillTimer(hwnd, OVERLAY_TIMER_ID + 1);
                            state_changed = true;
                        }
                        // Hide control overlay if visible
                        if (panel->m_compact_controls_visible) {
                            panel->m_compact_controls_visible = false;
                            state_changed = true;
                        }
                        if (state_changed) {
                            InvalidateRect(hwnd, nullptr, FALSE);
                        }
                    } else {
                        // Mouse anywhere else in panel - show controls
                        bool state_changed = false;
                        if (!panel->m_compact_controls_visible) {
                            panel->m_compact_controls_visible = true;
                            panel->m_last_compact_mouse_time = GetTickCount();
                            state_changed = true;
                        }
                        // Hide artwork overlay if visible
                        if (panel->m_undocked_overlay_visible) {
                            panel->m_undocked_overlay_visible = false;
                            panel->m_undocked_overlay_opacity = 0;
                            KillTimer(hwnd, OVERLAY_TIMER_ID + 1);
                            KillTimer(hwnd, FADE_TIMER_ID + 1);
                            state_changed = true;
                        }
                        if (state_changed) {
                            InvalidateRect(hwnd, nullptr, FALSE);
                        }
                    }
                    
                    // Track mouse leave events for compact mode
                    TRACKMOUSEEVENT tme = {0};
                    tme.cbSize = sizeof(TRACKMOUSEEVENT);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = hwnd;
                    TrackMouseEvent(&tme);
                    
                } else {
                    // Normal undocked mode - existing artwork overlay logic
                    int art_size = (80 < (window_width - 30) ? 80 : (window_width - 30));
                    art_size = (art_size < (window_height - 30) ? art_size : (window_height - 30));
                    int art_x = 15;
                    int art_y = 15;
                    
                    if (pt.x >= art_x && pt.x <= art_x + art_size && 
                        pt.y >= art_y && pt.y <= art_y + art_size) {
                        // Mouse is over artwork - show overlay immediately
                        if (!panel->m_undocked_overlay_visible) {
                            panel->m_undocked_overlay_visible = true;
                            panel->m_undocked_overlay_opacity = 100;
                            KillTimer(hwnd, FADE_TIMER_ID + 1);
                            KillTimer(hwnd, OVERLAY_TIMER_ID + 1);
                            if (panel->m_is_artwork_expanded) {
                                InvalidateRect(hwnd, nullptr, FALSE);
                            } else {
                                InvalidateRect(hwnd, nullptr, FALSE);
                            }
                            
                            // Track mouse leave events for undocked mode
                            TRACKMOUSEEVENT tme = {0};
                            tme.cbSize = sizeof(TRACKMOUSEEVENT);
                            tme.dwFlags = TME_LEAVE;
                            tme.hwndTrack = hwnd;
                            TrackMouseEvent(&tme);
                        }
                    } else {
                        // Mouse left artwork area - hide immediately
                        if (panel->m_undocked_overlay_visible) {
                            panel->m_undocked_overlay_visible = false;
                            panel->m_undocked_overlay_opacity = 0;
                            KillTimer(hwnd, OVERLAY_TIMER_ID + 1);
                            KillTimer(hwnd, FADE_TIMER_ID + 1);
                            if (panel->m_is_artwork_expanded) {
                                InvalidateRect(hwnd, nullptr, FALSE);
                            } else {
                                InvalidateRect(hwnd, nullptr, FALSE);
                            }
                        }
                    }
                }
            }
            
            // Ensure buttons stay at full opacity in undocked mode (no fade behavior)
            if (panel && panel->m_is_undocked && !panel->m_is_artwork_expanded) {
                // Always keep buttons visible and at full opacity
                if (panel->m_button_opacity < 100) {
                    panel->m_button_opacity = 100;
                    panel->m_buttons_visible = true;
                    // Use FALSE to prevent flickering - no need to erase background
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                // Track mouse in window for collapse triangle visibility
                if (!panel->m_mouse_in_window) {
                    panel->m_mouse_in_window = true;
                    // Re-enable mouse tracking so WM_MOUSELEAVE fires when mouse leaves
                    TRACKMOUSEEVENT tme = {0};
                    tme.cbSize = sizeof(TRACKMOUSEEVENT);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = hwnd;
                    TrackMouseEvent(&tme);
                    // Use FALSE to prevent flickering
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }

            
            // Check which button is hovered for visual feedback
            if (panel) {
                int hovered_btn = 0;
                POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                
                // Docked mode logic
                if (!panel->m_is_undocked) {
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    int window_height = client_rect.bottom - client_rect.top;
                    int window_width = client_rect.right - client_rect.left;
                    int button_y = window_height - 30;
                    
                    // Docked mode uses button_spacing = 60
                    int button_spacing = 60;

                    // Mirror paint logic: center in area right of artwork
                    // Use dynamic art_size calculation
                    int art_size = (80 < (window_width - 30) ? 80 : (window_width - 30));
                    art_size = (art_size < (window_height - 30) ? art_size : (window_height - 30));
                    int button_area_left = 15 + art_size + 10;
                    int button_area_width = window_width - button_area_left - 10;
                    int center_x = button_area_left + button_area_width / 2;
                    
                    if (pt.y >= button_y - 20 && pt.y <= button_y + 20) {
                        int button_radius = 15;
                        int play_radius = 20;

                        // Docked mode only has 3 buttons: Prev, Play/Pause, Next
                        if (abs(pt.x - center_x) <= play_radius) hovered_btn = BTN_PLAYPAUSE;
                        else if (abs(pt.x - (center_x - button_spacing)) <= button_radius) hovered_btn = BTN_PREV;
                        else if (abs(pt.x - (center_x + button_spacing)) <= button_radius) hovered_btn = BTN_NEXT;
                    }
                } 
                // Expanded Artwork mode logic
                else if (panel->m_is_artwork_expanded && panel->m_overlay_visible) {
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    int window_height = client_rect.bottom - client_rect.top;
                    int window_width = client_rect.right - client_rect.left;
                    int overlay_height = 70;
                    
                    if (pt.y >= window_height - overlay_height) {
                        int button_spacing = 60;
                        int center_x = window_width / 2;
                        
                        int button_radius = 20; 
                        if (abs(pt.x - (center_x - button_spacing * 2)) <= button_radius) hovered_btn = BTN_SHUFFLE;
                        else if (abs(pt.x - (center_x - button_spacing)) <= button_radius) hovered_btn = BTN_PREV;
                        else if (abs(pt.x - center_x) <= button_radius) hovered_btn = BTN_PLAYPAUSE;
                        else if (abs(pt.x - (center_x + button_spacing)) <= button_radius) hovered_btn = BTN_NEXT;
                        else if (abs(pt.x - (center_x + button_spacing * 2)) <= button_radius) hovered_btn = BTN_REPEAT;
                    }
                }
                // Compact mode logic
                else if (panel->m_is_compact_mode && panel->m_compact_controls_visible) {
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    int window_height = client_rect.bottom - client_rect.top;
                    int window_width = client_rect.right - client_rect.left;
                    int margin = 5;
                    int art_size = window_height - (2 * margin);
                    int text_left = margin + art_size + margin;
                    int text_right = window_width - margin;
                    int text_area_width = text_right - text_left;
                    
                    int button_size = 24; 
                    int play_button_size = 36;
                    int button_spacing = 10;
                    
                    int total_buttons_width = (4 * button_size) + play_button_size + (4 * button_spacing);
                    
                    int buttons_start_x = text_left + (text_area_width - total_buttons_width) / 2;
                    
                    int center_y_line = margin + (window_height - 2 * margin) / 2 + 5;
                    int button_y = center_y_line; 
                    
                    int shuffle_x = buttons_start_x + button_size / 2;
                    int prev_x = shuffle_x + button_size/2 + button_spacing + button_size/2;
                    int play_x = prev_x + button_size/2 + button_spacing + play_button_size/2;
                    int next_x = play_x + play_button_size/2 + button_spacing + button_size/2;
                    int repeat_x = next_x + button_size/2 + button_spacing + button_size/2;
                    
                    int click_radius = button_size / 2 + 2;
                    int play_click_radius = play_button_size / 2 + 2;

                    if (abs(pt.x - shuffle_x) <= click_radius && abs(pt.y - button_y) <= click_radius) hovered_btn = BTN_SHUFFLE;
                    else if (abs(pt.x - prev_x) <= click_radius && abs(pt.y - button_y) <= click_radius) hovered_btn = BTN_PREV;
                    else if (abs(pt.x - play_x) <= play_click_radius && abs(pt.y - button_y) <= play_click_radius) hovered_btn = BTN_PLAYPAUSE;
                    else if (abs(pt.x - next_x) <= click_radius && abs(pt.y - button_y) <= click_radius) hovered_btn = BTN_NEXT;
                    else if (abs(pt.x - repeat_x) <= click_radius && abs(pt.y - button_y) <= click_radius) hovered_btn = BTN_REPEAT;
                }
                // Normal Undocked logic (Miniplayer)
                else if (panel->m_is_undocked && !panel->m_is_artwork_expanded && !panel->m_is_compact_mode) {
                     RECT client_rect;
                     GetClientRect(hwnd, &client_rect);
                     int window_width = client_rect.right - client_rect.left;
                     int window_height = client_rect.bottom - client_rect.top;
                     
                     int button_y = window_height - 30;
                     // Match paint function: button_spacing = 40 for undocked mode
                     int button_spacing = 40;
                     // Calculate art_size and center_x the same way as paint function
                     int art_size = (80 < (window_width - 30) ? 80 : (window_width - 30));
                     art_size = (art_size < (window_height - 30) ? art_size : (window_height - 30));
                     int button_area_left = 15 + art_size + 10;
                     int button_area_width = window_width - button_area_left - 10;
                     int center_x = button_area_left + button_area_width / 2;
                     
                     int click_radius = 15;
                     int play_click_radius = 20;
                     
                     if (abs(pt.y - button_y) <= 20) {
                         // Check all 5 buttons: Shuffle, Previous, Play/Pause, Next, Repeat
                         if (abs(pt.x - (center_x - button_spacing * 2)) <= click_radius) hovered_btn = BTN_SHUFFLE;
                         else if (abs(pt.x - (center_x - button_spacing)) <= click_radius) hovered_btn = BTN_PREV;
                         else if (abs(pt.x - center_x) <= play_click_radius) hovered_btn = BTN_PLAYPAUSE;
                         else if (abs(pt.x - (center_x + button_spacing)) <= click_radius) hovered_btn = BTN_NEXT;
                         else if (abs(pt.x - (center_x + button_spacing * 2)) <= click_radius) hovered_btn = BTN_REPEAT;
                     }
                }
                
                // Update hover state
                if (panel->m_hovered_button != hovered_btn) {
                    panel->m_hovered_button = hovered_btn;
                    // Fully redraw to ensure background circles are drawn/erased
                    // Use FALSE for bErase (3rd arg) to prevent flickering in double-buffered modes (like expanded artwork)
                    InvalidateRect(hwnd, nullptr, FALSE); 
                }
            }
            break;
            
        case WM_NCMOUSEMOVE:
            // Track mouse movement in non-client area (caption/draggable area)
            // This fires when WM_NCHITTEST returns HTCAPTION
            if (panel && panel->m_is_undocked && !panel->m_is_artwork_expanded && !panel->m_is_compact_mode) {
                // Mouse is in the window's draggable area - show collapse triangle
                if (!panel->m_mouse_in_window) {
                    panel->m_mouse_in_window = true;
                    // Set up mouse leave tracking for non-client area
                    TRACKMOUSEEVENT tme = {0};
                    tme.cbSize = sizeof(TRACKMOUSEEVENT);
                    tme.dwFlags = TME_LEAVE | TME_NONCLIENT;
                    tme.hwndTrack = hwnd;
                    TrackMouseEvent(&tme);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
            break;
            
        case WM_NCMOUSELEAVE:
            // WM_NCMOUSELEAVE fires when:
            // 1. Mouse moves from non-client (caption) to client (buttons) area - don't hide triangle
            // 2. Mouse leaves the window entirely - hide the triangle
            if (panel && panel->m_is_undocked && !panel->m_is_artwork_expanded && !panel->m_is_compact_mode) {
                // Check if mouse is actually outside our window
                POINT cursor_pos;
                GetCursorPos(&cursor_pos);
                HWND window_under_cursor = WindowFromPoint(cursor_pos);
                
                // Only hide if mouse is NOT over our window (or a child of it)
                if (window_under_cursor != hwnd && !IsChild(hwnd, window_under_cursor)) {
                    if (panel->m_mouse_in_window) {
                        panel->m_mouse_in_window = false;
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
            }
            break;
            
        case WM_MOUSELEAVE:
            // Hide overlay immediately when mouse leaves the window (no fade animation)
            if (panel && panel->m_is_artwork_expanded && panel->m_overlay_visible) {
                KillTimer(hwnd, OVERLAY_TIMER_ID);
                KillTimer(hwnd, FADE_TIMER_ID);
                panel->m_overlay_visible = false;
                panel->m_overlay_opacity = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            
            // Clear hover state on leave
            if (panel && panel->m_hovered_button != 0) {
                panel->m_hovered_button = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
                
            if (panel && !panel->m_is_artwork_expanded && (panel->m_undocked_overlay_visible || panel->m_compact_controls_visible)) {
                // Hide undocked artwork overlay and compact control overlay when mouse leaves the window
                bool needs_repaint = false;
                if (panel->m_undocked_overlay_visible) {
                    panel->m_undocked_overlay_visible = false;
                    panel->m_undocked_overlay_opacity = 0;
                    KillTimer(hwnd, OVERLAY_TIMER_ID + 1);
                    KillTimer(hwnd, FADE_TIMER_ID + 1);
                    needs_repaint = true;
                }
                if (panel->m_compact_controls_visible) {
                    panel->m_compact_controls_visible = false;
                    needs_repaint = true;
                }
                if (needs_repaint) {
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            }
            
            // Buttons no longer fade away - they stay visible in undocked mode
            // Removed automatic fade behavior per user request

            // Hide collapse triangle when mouse leaves the window
            if (panel && panel->m_is_undocked && !panel->m_is_artwork_expanded && panel->m_mouse_in_window) {
                panel->m_mouse_in_window = false;
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            break;
            
        case WM_NCHITTEST:
            // Handle hit testing based on current mode
            if (panel) {
                LRESULT hit = DefWindowProc(hwnd, msg, wparam, lparam);
                
                // In artwork expanded mode, enable dragging and resizing
                if (panel->m_is_artwork_expanded) {
                    // Convert screen coordinates to client coordinates
                    POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                    ScreenToClient(hwnd, &pt);
                    
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    const int resize_border = 8; // 8 pixel border for resizing
                    int window_width = client_rect.right - client_rect.left;
                    int window_height = client_rect.bottom - client_rect.top;
                    const int overlay_height = 70;
                    
                    // Check for resize areas (corners and edges)
                    bool at_left = pt.x < resize_border;
                    bool at_right = pt.x > client_rect.right - resize_border;
                    bool at_top = pt.y < resize_border;
                    bool at_bottom = pt.y > client_rect.bottom - resize_border;
                    
                    // Slide-to-side: Allow clicks on right edge for slide feature
                    // But preserve corners for resizing, and exclude top (collapse triangle) and bottom (controls)
                    const int SLIDE_CLICK_ZONE = 40;
                    if (pt.x >= window_width - SLIDE_CLICK_ZONE && 
                        pt.y > 40 && pt.y < window_height - overlay_height &&
                        !at_top && !at_bottom) {
                        return HTCLIENT; // Allow WM_LBUTTONDOWN for slide-to-side
                    }
                    
                    // Return appropriate resize handles
                    if (at_left && at_top) return HTTOPLEFT;
                    if (at_right && at_top) return HTTOPRIGHT;
                    if (at_left && at_bottom) return HTBOTTOMLEFT;
                    if (at_right && at_bottom) return HTBOTTOMRIGHT;
                    if (at_left) return HTLEFT;
                    if (at_right) return HTRIGHT;
                    if (at_top) return HTTOP;
                    if (at_bottom) return HTBOTTOM;
                    
                    // Center area - return HTCLIENT to allow mouse messages for overlay
                    return HTCLIENT;
                }
                
                // Handle compact mode - allow dragging, no resizing, special button handling
                if (panel->m_is_compact_mode) {
                    // Convert screen coordinates to client coordinates
                    POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                    ScreenToClient(hwnd, &pt);
                    
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    int window_width = client_rect.right - client_rect.left;
                    
                    // Check if over artwork area - return HTCLIENT for proper mouse handling
                    int window_height = client_rect.bottom - client_rect.top;
                    int margin = 5;
                    int art_size = window_height - (2 * margin);
                    int art_x = margin;
                    int art_y = margin;
                    
                    if (pt.x >= art_x && pt.x < art_x + art_size && pt.y >= art_y && pt.y < art_y + art_size) {
                        return HTCLIENT; // Allow mouse messages for artwork hover and clicks
                    }
                    
                    // Check if over progress bar area - return HTCLIENT for click handling
                    int text_left = margin + art_size + margin;
                    int text_right = window_width - margin;
                    int progress_bar_height = 5;
                    int progress_bar_y = window_height - progress_bar_height - 2 - (int)(window_height * 0.1);
                    int progress_bar_left = text_left;
                    int progress_bar_width = text_right - text_left - 40; // Leave space for time display
                    
                    if (pt.x >= progress_bar_left && pt.x < progress_bar_left + progress_bar_width &&
                        pt.y >= progress_bar_y && pt.y < progress_bar_y + progress_bar_height) {
                        return HTCLIENT; // Allow mouse messages for progress bar clicks
                    }
                    
                    // Check for horizontal resize areas FIRST (left and right edges only)
                    const int resize_border = 6;
                    bool at_left = pt.x < resize_border;
                    bool at_right = pt.x > window_width - resize_border;
                    
                    // Return resize handles for left edge only - right edge is for slide-to-side
                    if (at_left) return HTLEFT;
                    
                    // Slide-to-side: Allow clicks on right edge for slide feature
                    const int SLIDE_CLICK_ZONE = 40;
                    if (pt.x >= window_width - SLIDE_CLICK_ZONE) {
                        return HTCLIENT; // Allow WM_LBUTTONDOWN for slide-to-side
                    }
                    
                    // Check text area behavior - always return HTCLIENT for consistent mouse handling
                    int text_area_left = margin + art_size + margin;
                    int text_area_right = window_width - resize_border; // Exclude resize border
                    if (pt.x >= text_area_left && pt.x < text_area_right) {
                        // Always return HTCLIENT for text area to ensure consistent mouse movement processing
                        // This prevents flickering caused by HTCAPTION blocking WM_MOUSEMOVE messages
                        return HTCLIENT;
                    }
                    
                    // Allow dragging from anywhere else on the compact panel (unless disabled)
                    if (get_disable_miniplayer()) {
                        return HTCLIENT; // Prevent dragging when miniPlayer is disabled
                    }
                    return HTCAPTION;
                }
                
                // Handle undocked mini player - no resizing allowed, only dragging
                if (panel->m_is_undocked && !panel->m_is_artwork_expanded && !panel->m_is_compact_mode) {
                    // Undocked control panel is no longer resizable per user request
                    // Only allow dragging by returning HTCAPTION for the entire client area
                    
                    // Convert screen coordinates to client coordinates
                    POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                    ScreenToClient(hwnd, &pt);
                    
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    
                    // IF SLID TO SIDE: Force HTCLIENT to allow click to slide back
                    // This allows WM_LBUTTONDOWN to capture the click and call slide_back_from_side
                    if (panel->m_is_slid_to_side) {
                        return HTCLIENT;
                    }
                    
                    // Calculate adaptive button and artwork positions
                    int window_width = client_rect.right - client_rect.left;
                    int window_height = client_rect.bottom - client_rect.top;
                    
                    
                    int button_y = window_height - 30;
                    // Match paint function: button_spacing = 40 for undocked mode
                    int button_spacing = 40;
                    // Calculate art_size the same way as paint function
                    int art_size = (80 < (window_width - 30) ? 80 : (window_width - 30));
                    art_size = (art_size < (window_height - 30) ? art_size : (window_height - 30));
                    // Calculate center_x the same way as paint function
                    int button_area_left = 15 + art_size + 10; // x after artwork area
                    int button_area_width = window_width - button_area_left - 10;
                    int center_x = button_area_left + button_area_width / 2;
                    
                    // Check if click is in button areas - don't allow dragging here
                    // Use ±20px to match click detection in WM_LBUTTONDOWN
                    if (pt.y >= button_y - 20 && pt.y <= button_y + 20) { // Button row
                        // Check all 5 buttons: Shuffle, Previous, Play/Pause, Next, Repeat
                        int click_radius = 15; // Match click detection radius
                        int play_click_radius = 20; // Larger for center play button
                        
                        if (abs(pt.x - (center_x - button_spacing * 2)) <= click_radius ||  // Shuffle
                            abs(pt.x - (center_x - button_spacing)) <= click_radius ||      // Previous
                            abs(pt.x - center_x) <= play_click_radius ||                    // Play/Pause
                            abs(pt.x - (center_x + button_spacing)) <= click_radius ||      // Next
                            abs(pt.x - (center_x + button_spacing * 2)) <= click_radius) {  // Repeat
                            return HTCLIENT; // Normal button behavior
                        }
                    }
                    
                    // Check if click is in artwork area - don't allow dragging here
                    if (pt.x >= 15 && pt.x < 15 + art_size && pt.y >= 15 && pt.y < 15 + art_size) {
                        return HTCLIENT; // Normal click behavior for artwork
                    }

                    // Check if click is on collapse triangle in top-right corner
                    if (pt.x >= window_width - 40 && pt.y <= 40) {
                        return HTCLIENT; // Allow click handling for collapse triangle
                    }

                    // Slide-to-side: Allow clicks on right edge for slide feature (below collapse triangle)
                    const int SLIDE_CLICK_ZONE = 40;
                    if (pt.x >= window_width - SLIDE_CLICK_ZONE && pt.y > 40) {
                        return HTCLIENT; // Allow WM_LBUTTONDOWN for slide-to-side
                    }

                    // Check for horizontal resize areas only (left edge - right edge is for slide-to-side)
                    const int resize_border = 6;
                    bool at_left = pt.x < resize_border;
                    
                    // Only return resize handle for left edge if not in button or artwork areas
                    bool in_button_area = (pt.y >= button_y - 10 && pt.y <= button_y + 10);
                    bool in_artwork_area = (pt.x >= 15 && pt.x < 15 + art_size && pt.y >= 15 && pt.y < 15 + art_size);
                    
                    if (!in_button_area && !in_artwork_area) {
                        if (at_left) return HTLEFT;
                    }
                    
                    // Allow dragging from everywhere else (unless disabled)
                    if (get_disable_miniplayer()) {
                        return HTCLIENT; // Prevent dragging when miniPlayer is disabled
                    }
                    return HTCAPTION;
                }
                
                if (hit == HTCLIENT) {
                    // Convert screen coordinates to client coordinates
                    POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                    ScreenToClient(hwnd, &pt);
                    
                    // Calculate adaptive positions for docked mode - match paint function
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    int window_width = client_rect.right - client_rect.left;
                    int window_height = client_rect.bottom - client_rect.top;
                    int button_y = window_height - 30;
                    // Docked mode uses button_spacing = 60
                    int button_spacing = 60;
                    // Calculate art_size and center_x the same way as paint function
                    int art_size = (80 < (window_width - 30) ? 80 : (window_width - 30));
                    art_size = (art_size < (window_height - 30) ? art_size : (window_height - 30));
                    int button_area_left = 15 + art_size + 10;
                    int button_area_width = window_width - button_area_left - 10;
                    int center_x = button_area_left + button_area_width / 2;
                    
                    // Check if click is in button areas - don't allow dragging here
                    // Use ±20px to match click detection
                    int click_radius = 15;
                    int play_click_radius = 20;
                    if (pt.y >= button_y - 20 && pt.y <= button_y + 20) { // Button row
                        if (abs(pt.x - (center_x - button_spacing)) <= click_radius ||      // Previous
                            abs(pt.x - center_x) <= play_click_radius ||                    // Play/Pause
                            abs(pt.x - (center_x + button_spacing)) <= click_radius) {      // Next
                            return HTCLIENT; // Normal button behavior
                        }
                    }
                    
                    // Check if click is in artwork area - don't allow dragging here
                    if (pt.x >= 15 && pt.x < 15 + art_size && pt.y >= 15 && pt.y < 15 + art_size) {
                        return HTCLIENT; // Normal click behavior for artwork
                    }
                    
                    // For docked mode, allow dragging from everywhere else (except buttons and artwork, unless disabled)
                    if (get_disable_miniplayer()) {
                        return HTCLIENT; // Prevent dragging when miniPlayer is disabled
                    }
                    return HTCAPTION;
                }
                return hit;
            }
            break;
            
        case WM_ENTERSIZEMOVE:
            // User started dragging - if docked, switch to undocked mode
            if (panel && !panel->m_is_undocked) {
                panel->set_undocked(true);
                // Stop timeout timer to prevent auto-hide
                KillTimer(panel->m_control_window, TIMEOUT_TIMER_ID);
                // Keep topmost behavior even when undocked
                SetWindowPos(panel->m_control_window, HWND_TOPMOST, 0, 0, 0, 0, 
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                // Schedule track info update (asynchronous to avoid drag delay)
                SetTimer(panel->m_control_window, UPDATE_TIMER_ID + 2, 50, nullptr);
                // Trigger repaint to hide undock icon
                InvalidateRect(panel->m_control_window, nullptr, TRUE);
            }
            break;
            
        case WM_SIZE:
            // Handle window resizing
            if (panel) {
                if (panel->m_is_artwork_expanded) {
                    // Maintain aspect ratio in expanded artwork mode
                    if (panel->m_cover_art_bitmap_original && panel->m_original_art_width > 0 && panel->m_original_art_height > 0) {
                        int new_width = LOWORD(lparam);
                        int new_height = HIWORD(lparam);
                        
                        float image_aspect = (float)panel->m_original_art_width / (float)panel->m_original_art_height;
                        float window_aspect = (float)new_width / (float)new_height;
                        
                        // Adjust dimensions to maintain artwork aspect ratio
                        int corrected_width, corrected_height;
                        
                        if (window_aspect > image_aspect) {
                            // Window is too wide - adjust width to match aspect ratio
                            corrected_height = new_height;
                            corrected_width = (int)((float)new_height * image_aspect);
                        } else {
                            // Window is too tall - adjust height to match aspect ratio
                            corrected_width = new_width;
                            corrected_height = (int)((float)new_width / image_aspect);
                        }
                        
                        // Apply minimum size constraints
                        if (corrected_width < 200) corrected_width = 200;
                        if (corrected_height < 200) corrected_height = 200;
                        
                        // Only resize if different from current size to avoid recursion
                        if (corrected_width != new_width || corrected_height != new_height) {
                            SetWindowPos(hwnd, nullptr, 0, 0, corrected_width, corrected_height, 
                                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                            return 0; // Prevent further processing
                        }
                        
                        // Save the corrected dimensions for expanded mode
                        panel->m_saved_expanded_width = corrected_width;
                        panel->m_saved_expanded_height = corrected_height;
                    }
                    
                    // Repaint to adjust artwork display - use optimized invalidation to prevent flicker
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else if (panel->m_is_undocked) {
                    // Handle resize-based mode switching between normal undocked and compact modes
                    int new_width = LOWORD(lparam);
                    int new_height = HIWORD(lparam);
                    
                    const int compact_height = 75;
                    const int normal_min_width = 338; // Default undocked panel width
                    const int normal_height = 120; // Fixed undocked panel height
                    const int normal_max_width = 676; // Twice the default width (338 * 2)
                    const int compact_min_width = 320;
                    const int compact_max_width = 640; // Twice the default width (320 * 2)
                    
                    // Determine target mode based on height
                    bool should_be_compact = (new_height <= 90); // Allow some tolerance around compact height
                    
                    if (should_be_compact && !panel->m_is_compact_mode) {
                        // Switching to compact mode
                        panel->m_saved_normal_width = new_width >= normal_min_width ? new_width : normal_min_width;
                        panel->m_saved_normal_height = normal_height;
                        panel->m_is_compact_mode = true;
                        
                        // Use saved compact width or minimum width
                        new_width = panel->m_saved_compact_width >= compact_min_width ? panel->m_saved_compact_width : compact_min_width;
                        new_height = compact_height;
                        
                        SetWindowPos(hwnd, nullptr, 0, 0, new_width, new_height, 
                            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                        InvalidateRect(hwnd, nullptr, TRUE);
                        return 0;
                    } 
                    else if (!should_be_compact && panel->m_is_compact_mode) {
                        // Switching to normal undocked mode
                        panel->m_is_compact_mode = false;
                        
                        // Use saved normal width and fixed height
                        new_width = panel->m_saved_normal_width >= normal_min_width ? panel->m_saved_normal_width : panel->m_saved_undocked_width;
                        new_height = normal_height; // Always use fixed height
                        
                        SetWindowPos(hwnd, nullptr, 0, 0, new_width, new_height, 
                            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                        InvalidateRect(hwnd, nullptr, TRUE);
                        return 0;
                    }
                    else if (panel->m_is_compact_mode) {
                        // Already in compact mode - enforce constraints
                        bool needs_adjustment = false;
                        if (new_width < compact_min_width) {
                            new_width = compact_min_width;
                            needs_adjustment = true;
                        }
                        if (new_width > compact_max_width) {
                            new_width = compact_max_width;
                            needs_adjustment = true;
                        }
                        if (new_height != compact_height) {
                            new_height = compact_height;
                            needs_adjustment = true;
                        }
                        
                        // Save compact mode width when it's valid
                        if (!needs_adjustment || (needs_adjustment && new_width >= compact_min_width && new_width <= compact_max_width)) {
                            panel->m_saved_compact_width = new_width;
                        }
                        
                        if (needs_adjustment) {
                            SetWindowPos(hwnd, nullptr, 0, 0, new_width, new_height, 
                                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                            return 0;
                        }
                    }
                    else {
                        // Already in normal mode - enforce width constraints and fixed height
                        bool needs_adjustment = false;
                        if (new_width < normal_min_width) {
                            new_width = normal_min_width;
                            needs_adjustment = true;
                        }
                        if (new_width > normal_max_width) {
                            new_width = normal_max_width;
                            needs_adjustment = true;
                        }
                        if (new_height != normal_height) {
                            new_height = normal_height; // Force fixed height
                            needs_adjustment = true;
                        }
                        
                        // Save current normal width (height is fixed)
                        if (!needs_adjustment) {
                            panel->m_saved_normal_width = new_width;
                            panel->m_saved_normal_height = normal_height; // Always use fixed height
                        }
                        
                        if (needs_adjustment) {
                            SetWindowPos(hwnd, nullptr, 0, 0, new_width, new_height, 
                                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                            return 0;
                        }
                    }
                    
                    // Repaint to ensure proper display
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            }
            break;
            
        case WM_TIMER:
            if (wparam == UPDATE_TIMER_ID) {
                panel->handle_timer();
                return 0;
            } else if (wparam == ANIMATION_TIMER_ID) {
                // Animation timer - update animation
                if (panel) panel->update_animation();
                return 0;
            } else if (wparam == UPDATE_TIMER_ID + 3) {
                // Roll animation timer
                if (panel) panel->update_roll_animation();
                return 0;
            } else if (wparam == UPDATE_TIMER_ID + 1) {
                // Delayed update after track change
                KillTimer(hwnd, UPDATE_TIMER_ID + 1);
                panel->update_track_info();
                return 0;
            } else if (wparam == UPDATE_TIMER_ID + 2) {
                // Delayed update after showing panel (asynchronous)
                KillTimer(hwnd, UPDATE_TIMER_ID + 2);
                panel->cleanup_cover_art(); // Clear cached artwork
                panel->update_track_info(); // Full update
                return 0;
            } else if (wparam == TIMEOUT_TIMER_ID) {
                // Timeout reached - hide the control panel
                // But NOT in artwork expanded mode - user wants it to stay open
                if (panel && !panel->m_is_artwork_expanded) {
                    panel->hide_control_panel();
                }
                return 0;
            } else if (wparam == OVERLAY_TIMER_ID) {
                // Start fade animation after no mouse movement
                KillTimer(hwnd, OVERLAY_TIMER_ID);
                if (panel && panel->m_overlay_visible && panel->m_overlay_opacity > 0) {
                    panel->m_fade_start_time = GetTickCount();
                    SetTimer(hwnd, FADE_TIMER_ID, 50, nullptr); // 50ms intervals for smooth fade (20 fps)
                }
                return 0;
            } else if (wparam == FADE_TIMER_ID) {
                // Handle fade animation
                if (panel && panel->m_overlay_visible) {
                    DWORD elapsed = GetTickCount() - panel->m_fade_start_time;
                    const DWORD fade_duration = 1000; // 1 second fade
                    
                    if (elapsed >= fade_duration) {
                        // Fade complete - hide overlay
                        panel->m_overlay_visible = false;
                        panel->m_overlay_opacity = 0;
                        KillTimer(hwnd, FADE_TIMER_ID);
                        // In artwork expanded mode, only invalidate overlay areas to prevent artwork flicker
                        if (panel->m_is_artwork_expanded) {
                            InvalidateRect(hwnd, nullptr, FALSE);
                        } else {
                            InvalidateRect(hwnd, nullptr, TRUE);
                        }
                    } else {
                        // Calculate fade progress (100 to 0 over 1 second)
                        panel->m_overlay_opacity = 100 - (int)((elapsed * 100) / fade_duration);
                        // In artwork expanded mode, only invalidate overlay areas to prevent artwork flicker
                        if (panel->m_is_artwork_expanded) {
                            InvalidateRect(hwnd, nullptr, FALSE);
                        } else {
                            InvalidateRect(hwnd, nullptr, TRUE);
                        }
                    }
                }
                return 0;
            } else if (wparam == OVERLAY_TIMER_ID + 1) {
                // Start fade animation for undocked artwork overlay
                KillTimer(hwnd, OVERLAY_TIMER_ID + 1);
                if (panel && panel->m_undocked_overlay_visible && panel->m_undocked_overlay_opacity > 0) {
                    panel->m_undocked_fade_start_time = GetTickCount();
                    SetTimer(hwnd, FADE_TIMER_ID + 1, 50, nullptr); // 50ms intervals for smooth fade
                }
                return 0;
            } else if (wparam == FADE_TIMER_ID + 1) {
                // Handle undocked artwork overlay fade animation
                if (panel && panel->m_undocked_overlay_visible) {
                    DWORD elapsed = GetTickCount() - panel->m_undocked_fade_start_time;
                    const DWORD fade_duration = 1000; // 1 second fade
                    
                    if (elapsed >= fade_duration) {
                        // Fade complete - hide overlay
                        panel->m_undocked_overlay_visible = false;
                        panel->m_undocked_overlay_opacity = 0;
                        KillTimer(hwnd, FADE_TIMER_ID + 1);
                        // In artwork expanded mode, only invalidate overlay areas to prevent artwork flicker
                        if (panel->m_is_artwork_expanded) {
                            InvalidateRect(hwnd, nullptr, FALSE);
                        } else {
                            InvalidateRect(hwnd, nullptr, TRUE);
                        }
                    } else {
                        // Calculate fade progress (100 to 0 over 1 second)
                        panel->m_undocked_overlay_opacity = 100 - (int)((elapsed * 100) / fade_duration);
                        // In artwork expanded mode, only invalidate overlay areas to prevent artwork flicker
                        if (panel->m_is_artwork_expanded) {
                            InvalidateRect(hwnd, nullptr, FALSE);
                        } else {
                            InvalidateRect(hwnd, nullptr, TRUE);
                        }
                    }
                }
                return 0;
            } else if (wparam == BUTTON_FADE_TIMER_ID) {
                // Start button fade animation after 2 second delay
                if (panel && panel->m_is_undocked && !panel->m_is_artwork_expanded) {
                    KillTimer(hwnd, BUTTON_FADE_TIMER_ID);
                    panel->m_button_fade_start_time = GetTickCount();
                    SetTimer(hwnd, BUTTON_FADE_TIMER_ID + 1, 50, nullptr); // Start fade animation (20 FPS)
                }
                return 0;
            } else if (wparam == BUTTON_FADE_TIMER_ID + 1) {
                // Handle button fade animation
                if (panel && panel->m_is_undocked && !panel->m_is_artwork_expanded) {
                    DWORD elapsed = GetTickCount() - panel->m_button_fade_start_time;
                    const DWORD fade_duration = 1000; // 1 second fade duration
                    
                    if (elapsed >= fade_duration) {
                        panel->m_buttons_visible = false;
                        panel->m_button_opacity = 0;
                        KillTimer(hwnd, BUTTON_FADE_TIMER_ID + 1);
                    } else {
                        panel->m_button_opacity = 100 - (int)((elapsed * 100) / fade_duration);
                    }
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                return 0;
            } else if (wparam == SLIDE_TIMER_ID) {
                // Handle slide-to-side animation
                if (panel) panel->update_slide_animation();
                return 0;
            }
            break;
            
        case WM_KILLFOCUS:
        case WM_ACTIVATE:
            if (LOWORD(wparam) == WA_INACTIVE) {
                // TEMPORARILY DISABLED: Hide panel when it loses focus - testing timeout timer
                // if (panel && !panel->m_is_undocked) {
                //     panel->hide_control_panel_immediate();
                // }
                return 0;
            }
            break;
        }
    }
    
    return DefWindowProc(hwnd, msg, wparam, lparam);
}


void control_panel::paint_control_panel(HDC hdc) {
    if (!hdc) return;
    
    RECT client_rect;
    GetClientRect(m_control_window, &client_rect);
    
    // Handle artwork expanded mode
    if (m_is_artwork_expanded) {
        paint_artwork_expanded(hdc, client_rect);
        return;
    }
    
    // Handle compact mode
    if (m_is_compact_mode) {
        paint_compact_mode(hdc, client_rect);
        return;
    }
    
    // Fill background (no border)
    HBRUSH bg_brush = CreateSolidBrush(RGB(32, 32, 32));
    FillRect(hdc, &client_rect, bg_brush);
    DeleteObject(bg_brush);
    
    // Calculate album art area (left side) - adapt to window size
    int window_width = client_rect.right - client_rect.left;
    int window_height = client_rect.bottom - client_rect.top;
    int art_size = (80 < (window_width - 30) ? 80 : (window_width - 30));
    art_size = (art_size < (window_height - 30) ? art_size : (window_height - 30)); // Adaptive size with minimums
    RECT cover_rect = {15, 15, 15 + art_size, 15 + art_size};
    if (m_cover_art_bitmap) {
        // Draw actual cover art
        HDC cover_dc = CreateCompatibleDC(hdc);
        HBITMAP old_bitmap = (HBITMAP)SelectObject(cover_dc, m_cover_art_bitmap);
        
        // Draw the cover art bitmap scaled to fit the adaptive cover area
        StretchBlt(hdc, cover_rect.left, cover_rect.top, art_size, art_size, cover_dc, 0, 0, 80, 80, SRCCOPY);
        
        SelectObject(cover_dc, old_bitmap);
        DeleteDC(cover_dc);
    } else {
        // Draw placeholder
        HBRUSH cover_brush = CreateSolidBrush(RGB(60, 60, 60));
        FillRect(hdc, &cover_rect, cover_brush);
        DeleteObject(cover_brush);
        
        // Check if current track is a stream and show radio icon
        try {
            auto playback = playback_control::get();
            metadb_handle_ptr track;
            
            if (playback->get_now_playing(track) && track.is_valid()) {
                pfc::string8 path = track->get_path();
                bool is_stream = strstr(path.get_ptr(), "://") != nullptr;
                
                if (is_stream) {
                    // Load and draw radio icon for internet streams
                    // Try LoadImage first for better flexibility with icon sizes
                    HICON radio_icon = (HICON)LoadImage(g_hIns, MAKEINTRESOURCE(IDI_RADIO_ICON), IMAGE_ICON, art_size/2, art_size/2, LR_DEFAULTCOLOR);
                    
                    // If LoadImage fails, try LoadIcon as fallback
                    if (!radio_icon) {
                        radio_icon = LoadIcon(g_hIns, MAKEINTRESOURCE(IDI_RADIO_ICON));
                    }
                    
                    if (radio_icon) {
                        // Center the icon in the cover rect
                        int icon_size = art_size / 2; // Half the artwork size
                        int icon_x = cover_rect.left + (art_size - icon_size) / 2;
                        int icon_y = cover_rect.top + (art_size - icon_size) / 2;
                        
                        DrawIconEx(hdc, icon_x, icon_y, radio_icon, icon_size, icon_size, 0, nullptr, DI_NORMAL);
                        DestroyIcon(radio_icon); // Clean up the icon handle
                    } else {
                        // Fallback to text if icon can't be loaded
                        SetTextColor(hdc, RGB(200, 200, 200));
                        SetBkMode(hdc, TRANSPARENT);
                        int font_size = art_size / 3; // Scale font with artwork size
                        HFONT symbol_font = CreateFont(font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
                        HFONT old_symbol_font = (HFONT)SelectObject(hdc, symbol_font);
                        
                        DrawText(hdc, L"📻", -1, &cover_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                        
                        SelectObject(hdc, old_symbol_font);
                        DeleteObject(symbol_font);
                    }
                } else {
                    // Draw musical note symbol for local files
                    SetTextColor(hdc, RGB(200, 200, 200));
                    SetBkMode(hdc, TRANSPARENT);
                    int font_size = art_size / 3; // Scale font with artwork size
                    HFONT symbol_font = CreateFont(font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
                    HFONT old_symbol_font = (HFONT)SelectObject(hdc, symbol_font);
                    
                    DrawText(hdc, L"♪", -1, &cover_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    
                    SelectObject(hdc, old_symbol_font);
                    DeleteObject(symbol_font);
                }
            }
        } catch (...) {
            // Ignore errors - placeholder will remain plain gray
        }
        
    }
    
    // Draw track info (pass art_size for adaptive layout)
    draw_track_info(hdc, client_rect, art_size);

    // Draw time info
    draw_time_info(hdc, client_rect);

    // Draw undocked artwork overlay if hovering over artwork
    if (m_undocked_overlay_visible) {
        draw_undocked_artwork_overlay(hdc, window_width, window_height);
    }

    // Draw control buttons using custom vector graphics - adapt to window size
    int button_y = window_height - 30; // 30px from bottom
    
    // Spacing depends on mode: 
    // Undocked: 5 buttons -> Tighter spacing (40)
    // Docked: 3 buttons -> Wider spacing (60)
    int button_spacing = (m_is_undocked) ? 40 : 60;
    
    // Center buttons in the area to the right of artwork (artwork is ~95px including margin)
    int button_area_left = 15 + art_size + 10; // x after artwork area
    int button_area_width = window_width - button_area_left - 10; // Available width for buttons
    int center_x = button_area_left + button_area_width / 2;

    // Enable anti-aliasing for smoother drawing
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, nullptr);

    int icon_size = 24; // Size for shuffle, repeat, prev, next
    int play_icon_size = 38; // Larger size for the central play button (white circle)

    // Draw Previous button
    int prev_x = center_x - button_spacing;
    if (m_is_undocked && !m_is_artwork_expanded) {
        draw_previous_icon_with_opacity(hdc, prev_x, button_y, icon_size, m_button_opacity);
    } else {
        draw_previous_icon(hdc, prev_x, button_y, icon_size);
    }

    // Draw Play/Pause button (Larger)
    if (m_is_undocked && !m_is_artwork_expanded) {
        if (m_is_playing && !m_is_paused) {
            draw_pause_icon_with_opacity(hdc, center_x, button_y, play_icon_size, m_button_opacity);
        } else {
            draw_play_icon_with_opacity(hdc, center_x, button_y, play_icon_size, m_button_opacity);
        }
    } else {
        if (m_is_playing && !m_is_paused) {
            draw_pause_icon(hdc, center_x, button_y, play_icon_size);
        } else {
            draw_play_icon(hdc, center_x, button_y, play_icon_size);
        }
    }

    // Draw Next button
    int next_x = center_x + button_spacing;
    if (m_is_undocked && !m_is_artwork_expanded) {
        draw_next_icon_with_opacity(hdc, next_x, button_y, icon_size, m_button_opacity);
    } else {
        draw_next_icon(hdc, next_x, button_y, icon_size);
    }
    
    // Draw Shuffle and Repeat ONLY in undocked mode
    if (m_is_undocked) {
        // Draw Shuffle button (left of Previous)
        int shuffle_x = center_x - button_spacing * 2;
        draw_shuffle_icon(hdc, shuffle_x, button_y, icon_size);
        
        // Draw Repeat button (right of Next)
        int repeat_x = center_x + button_spacing * 2;
        draw_repeat_icon(hdc, repeat_x, button_y, icon_size);
    }

    // Draw close button in top-left corner
    if (m_is_undocked && !m_is_artwork_expanded) {
        // Draw close button only on hover (cursor-triggered overlay)
        if (m_mouse_over_close_button) {
            int close_x = 15;
            int close_y = 15;
            // Draw without opacity (full visibility) if hovered
            draw_close_icon_with_opacity(hdc, close_x, close_y, 12, 100); 
        }
        
        // Draw "Collapse" triangle in top-right (Undocked -> Compact mode toggle)
        // Only visible when mouse is over the undocked panel (excluding artwork area)
        // Check actual cursor position rather than relying on event-based flag
        POINT cursor_pos;
        GetCursorPos(&cursor_pos);
        RECT window_rect;
        GetWindowRect(m_control_window, &window_rect);
        
        // Check if cursor is within window bounds
        bool cursor_in_window = (cursor_pos.x >= window_rect.left && cursor_pos.x < window_rect.right &&
                                 cursor_pos.y >= window_rect.top && cursor_pos.y < window_rect.bottom);
        
        // Also exclude artwork area
        if (cursor_in_window) {
            POINT client_pt = cursor_pos;
            ScreenToClient(m_control_window, &client_pt);
            // Check if over artwork
            int art_size = (80 < (window_width - 30) ? 80 : (window_width - 30));
            art_size = (art_size < (window_height - 30) ? art_size : (window_height - 30));
            if (client_pt.x >= 15 && client_pt.x < 15 + art_size && 
                client_pt.y >= 15 && client_pt.y < 15 + art_size) {
                cursor_in_window = false; // Don't show collapse triangle when over artwork
            }
        }
        
        if (cursor_in_window) {
            int collapse_size = 12;
            int collapse_x = window_width - 15;
            int collapse_y = 15;
            draw_collapse_triangle(hdc, collapse_x, collapse_y, collapse_size, 100);
        }
    }
    
    
}

void control_panel::paint_artwork_expanded(HDC hdc, const RECT& client_rect) {
    // Calculate artwork display area
    int window_width = client_rect.right - client_rect.left;
    int window_height = client_rect.bottom - client_rect.top;
    
    // Use double buffering to prevent flicker during overlay animations
    HDC buffer_dc = CreateCompatibleDC(hdc);
    HBITMAP buffer_bitmap = CreateCompatibleBitmap(hdc, window_width, window_height);
    HBITMAP old_buffer_bitmap = (HBITMAP)SelectObject(buffer_dc, buffer_bitmap);
    
    if (m_cover_art_bitmap_original && m_original_art_width > 0 && m_original_art_height > 0) {
        // Use original resolution bitmap for expanded view
        HDC cover_dc = CreateCompatibleDC(buffer_dc);
        HBITMAP old_bitmap = (HBITMAP)SelectObject(cover_dc, m_cover_art_bitmap_original);
        
        // Set high-quality stretching mode
        SetStretchBltMode(buffer_dc, HALFTONE);
        SetBrushOrgEx(buffer_dc, 0, 0, nullptr);
        
        // Since window maintains aspect ratio, artwork should fill entire window
        // Draw the artwork to fill the entire client area (no black bars)
        StretchBlt(buffer_dc, 0, 0, window_width, window_height,
                   cover_dc, 0, 0, m_original_art_width, m_original_art_height, SRCCOPY);
        
        SelectObject(cover_dc, old_bitmap);
        DeleteDC(cover_dc);
        
        // Draw track info overlay if hovering
        if (m_overlay_visible) {
            draw_track_info_overlay(buffer_dc, window_width, window_height);
            draw_control_overlay(buffer_dc, window_width, window_height);
        }
        
    } else {
        // Draw placeholder for no artwork
        RECT artwork_rect = {0, 0, window_width, window_height};
        HBRUSH placeholder_brush = CreateSolidBrush(RGB(60, 60, 60));
        FillRect(buffer_dc, &artwork_rect, placeholder_brush);
        DeleteObject(placeholder_brush);
        
        // Check if current track is a stream and show radio icon
        try {
            auto playback = playback_control::get();
            metadb_handle_ptr track;
            
            if (playback->get_now_playing(track) && track.is_valid()) {
                pfc::string8 path = track->get_path();
                bool is_stream = strstr(path.get_ptr(), "://") != nullptr;
                
                if (is_stream) {
                    // Load and draw radio icon for internet streams (use larger size for expanded view)
                    int icon_size = (window_width < window_height ? window_width : window_height) / 4; // Quarter of smallest dimension
                    HICON radio_icon = (HICON)LoadImage(g_hIns, MAKEINTRESOURCE(IDI_RADIO_ICON), IMAGE_ICON, icon_size, icon_size, LR_DEFAULTCOLOR);
                    
                    if (!radio_icon) {
                        radio_icon = LoadIcon(g_hIns, MAKEINTRESOURCE(IDI_RADIO_ICON));
                    }
                    
                    if (radio_icon) {
                        int icon_x = (window_width - icon_size) / 2;
                        int icon_y = (window_height - icon_size) / 2;
                        
                        DrawIconEx(buffer_dc, icon_x, icon_y, radio_icon, icon_size, icon_size, 0, nullptr, DI_NORMAL);
                        DestroyIcon(radio_icon);
                    } else {
                        // Fallback to text if icon can't be loaded
                        SetTextColor(buffer_dc, RGB(200, 200, 200));
                        SetBkMode(buffer_dc, TRANSPARENT);
                        int font_size = (window_width < window_height ? window_width : window_height) / 8; // Larger font for expanded view
                        HFONT symbol_font = CreateFont(font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
                        HFONT old_symbol_font = (HFONT)SelectObject(buffer_dc, symbol_font);
                        
                        DrawText(buffer_dc, L"📻", -1, &artwork_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                        
                        SelectObject(buffer_dc, old_symbol_font);
                        DeleteObject(symbol_font);
                    }
                } else {
                    // Draw musical note symbol for local files
                    SetTextColor(buffer_dc, RGB(200, 200, 200));
                    SetBkMode(buffer_dc, TRANSPARENT);
                    int font_size = (window_width < window_height ? window_width : window_height) / 8;
                    HFONT symbol_font = CreateFont(font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
                    HFONT old_symbol_font = (HFONT)SelectObject(buffer_dc, symbol_font);
                    
                    DrawText(buffer_dc, L"♪", -1, &artwork_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    
                    SelectObject(buffer_dc, old_symbol_font);
                    DeleteObject(symbol_font);
                }
            }
        } catch (...) {
            // Ignore errors - placeholder will remain plain gray
        }
        
        // Draw overlay even when no artwork (controls should appear on hover)
        if (m_overlay_visible) {
            draw_track_info_overlay(buffer_dc, window_width, window_height);
            draw_control_overlay(buffer_dc, window_width, window_height);
        }
    }
    
    // Copy the complete buffered image to the screen in one operation
    BitBlt(hdc, 0, 0, window_width, window_height, buffer_dc, 0, 0, SRCCOPY);
    
    // Cleanup buffer
    SelectObject(buffer_dc, old_buffer_bitmap);
    DeleteObject(buffer_bitmap);
    DeleteDC(buffer_dc);
}

void control_panel::draw_track_info(HDC hdc, const RECT& client_rect, int art_size) {
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    
    // Calculate text area based on artwork size and window dimensions
    int text_left = 15 + art_size + 10; // 10px gap after artwork
    int text_right = client_rect.right - 70; // Leave space for time display
    
    // Determine font styles based on mode (docked vs undocked)
    bool is_docked = !m_is_undocked;
    
    if (is_docked) {
        // DOCKED MODE: Track title large and bold, Artist small and normal
        // Draw track title using larger, bold font
        HFONT title_font_to_use = m_track_font ? m_track_font : CreateFont(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                                                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                                           DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT old_font = (HFONT)SelectObject(hdc, title_font_to_use);
        
        RECT title_rect = {text_left, 20, text_right, 45};
        pfc::stringcvt::string_wide_from_utf8 wide_title(m_current_title.c_str());
        DrawText(hdc, wide_title.get_ptr(), -1, &title_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        
        // Draw artist using smaller, normal font
        HFONT artist_font_to_use = m_artist_font ? m_artist_font : CreateFont(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        
        
        SelectObject(hdc, artist_font_to_use);
        
        RECT artist_rect = {text_left, 50, text_right, 70};
        pfc::stringcvt::string_wide_from_utf8 wide_artist(m_current_artist.c_str());
        DrawText(hdc, wide_artist.get_ptr(), -1, &artist_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        
        // Cleanup fonts (only delete fallback fonts, not our member fonts)
        SelectObject(hdc, old_font);
        if (!m_track_font && title_font_to_use) {
            DeleteObject(title_font_to_use);
        }
        if (!m_artist_font && artist_font_to_use) {
            DeleteObject(artist_font_to_use);
        }
    } else {
        // UNDOCKED MODE: Keep original behavior (title bold 18, artist normal 14)
        // Draw track title using custom or default font
        HFONT font_to_use = m_track_font ? m_track_font : CreateFont(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                                     DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT old_font = (HFONT)SelectObject(hdc, font_to_use);
        
        RECT title_rect = {text_left, 20, text_right, 45};
        pfc::stringcvt::string_wide_from_utf8 wide_title(m_current_title.c_str());
        DrawText(hdc, wide_title.get_ptr(), -1, &title_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        
        // Draw artist using custom or default font
        HFONT artist_font_to_use = m_artist_font ? m_artist_font : CreateFont(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                                              DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        
        
        SelectObject(hdc, artist_font_to_use);
        
        RECT artist_rect = {text_left, 50, text_right, 70};
        pfc::stringcvt::string_wide_from_utf8 wide_artist(m_current_artist.c_str());
        DrawText(hdc, wide_artist.get_ptr(), -1, &artist_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        
        // Cleanup fonts (only delete fallback fonts, not our member fonts)
        SelectObject(hdc, old_font);
        if (!m_track_font && font_to_use) {
            DeleteObject(font_to_use);
        }
        if (!m_artist_font && artist_font_to_use) {
            DeleteObject(artist_font_to_use);
        }
    }
}

void control_panel::paint_compact_mode(HDC hdc, const RECT& rect) {
    if (!hdc) return;
    
    // Fill background with dark color
    HBRUSH bg_brush = CreateSolidBrush(RGB(32, 32, 32));
    FillRect(hdc, &rect, bg_brush);
    DeleteObject(bg_brush);
    
    int window_width = rect.right - rect.left;
    int window_height = rect.bottom - rect.top;
    
    // Draw compact layout: [Artwork] [Song Title] [Artist] [Toggle Button]
    int margin = 5;
    int art_size = window_height - (2 * margin); // Use almost full height for artwork
    
    // Draw artwork
    RECT art_rect = {margin, margin, margin + art_size, margin + art_size};
    
    if (m_cover_art_bitmap) {
        HDC mem_dc = CreateCompatibleDC(hdc);
        HBITMAP old_bitmap = (HBITMAP)SelectObject(mem_dc, m_cover_art_bitmap);
        
        // Get bitmap dimensions
        BITMAP bmp_info;
        GetObject(m_cover_art_bitmap, sizeof(BITMAP), &bmp_info);
        
        // Draw artwork with proper scaling
        SetStretchBltMode(hdc, HALFTONE);
        StretchBlt(hdc, art_rect.left, art_rect.top, 
                   art_rect.right - art_rect.left, art_rect.bottom - art_rect.top,
                   mem_dc, 0, 0, bmp_info.bmWidth, bmp_info.bmHeight, SRCCOPY);
        
        SelectObject(mem_dc, old_bitmap);
        DeleteDC(mem_dc);
    } else {
        // Draw placeholder
        HBRUSH cover_brush = CreateSolidBrush(RGB(60, 60, 60));
        FillRect(hdc, &art_rect, cover_brush);
        DeleteObject(cover_brush);
        
        // Check if current track is a stream and show radio icon
        try {
            auto playback = playback_control::get();
            metadb_handle_ptr track;
            
            if (playback->get_now_playing(track) && track.is_valid()) {
                pfc::string8 path = track->get_path();
                bool is_stream = strstr(path.get_ptr(), "://") != nullptr;
                
                if (is_stream) {
                    // Load and draw radio icon for internet streams
                    HICON radio_icon = (HICON)LoadImage(g_hIns, MAKEINTRESOURCE(IDI_RADIO_ICON), IMAGE_ICON, art_size/2, art_size/2, LR_DEFAULTCOLOR);
                    
                    if (!radio_icon) {
                        radio_icon = LoadIcon(g_hIns, MAKEINTRESOURCE(IDI_RADIO_ICON));
                    }
                    
                    if (radio_icon) {
                        int icon_size = art_size / 2;
                        int icon_x = art_rect.left + (art_size - icon_size) / 2;
                        int icon_y = art_rect.top + (art_size - icon_size) / 2;
                        
                        DrawIconEx(hdc, icon_x, icon_y, radio_icon, icon_size, icon_size, 0, nullptr, DI_NORMAL);
                        DestroyIcon(radio_icon);
                    } else {
                        // Fallback to text if icon can't be loaded
                        SetTextColor(hdc, RGB(200, 200, 200));
                        SetBkMode(hdc, TRANSPARENT);
                        int font_size = art_size / 3;
                        HFONT symbol_font = CreateFont(font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                       DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
                        HFONT old_symbol_font = (HFONT)SelectObject(hdc, symbol_font);
                        
                        DrawText(hdc, L"📻", -1, &art_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                        
                        SelectObject(hdc, old_symbol_font);
                        DeleteObject(symbol_font);
                    }
                } else {
                    // Draw musical note symbol for local files
                    SetTextColor(hdc, RGB(200, 200, 200));
                    SetBkMode(hdc, TRANSPARENT);
                    int font_size = art_size / 3;
                    HFONT symbol_font = CreateFont(font_size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
                    HFONT old_symbol_font = (HFONT)SelectObject(hdc, symbol_font);
                    
                    DrawText(hdc, L"♪", -1, &art_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    
                    SelectObject(hdc, old_symbol_font);
                    DeleteObject(symbol_font);
                }
            }
        } catch (...) {
            // Ignore errors - placeholder will remain plain gray
        }
    }
    
    // Draw artwork hover arrows if hovering over artwork (same as undocked mode)
    if (m_undocked_overlay_visible) {
        draw_undocked_artwork_overlay(hdc, window_width, window_height);
    }
    
    // Set text properties
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    
    // Calculate text area (shifted right slightly per user request)
    int text_left = margin + art_size + margin + 10;
    int text_right = window_width - margin; // No dots, use full width
    
    // Draw song title (use configured track font, fallback to default if not set)
    HFONT title_font = m_track_font;
    bool need_delete_title = false;
    if (!title_font) {
        title_font = CreateFont(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
        need_delete_title = true;
    }
    HFONT old_font = (HFONT)SelectObject(hdc, title_font);
    
    RECT title_rect = {text_left, margin - (int)(window_height * 0.10), text_right, window_height / 2 + margin - (int)(window_height * 0.10)};
    pfc::stringcvt::string_wide_from_utf8 wide_title(m_current_title.c_str());
    DrawText(hdc, wide_title.get_ptr(), -1, &title_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    
    // Draw artist name (use configured artist font, fallback to default if not set)
    HFONT artist_font = m_artist_font;
    bool need_delete_artist = false;
    if (!artist_font) {
        artist_font = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
        need_delete_artist = true;
    }
    SelectObject(hdc, artist_font);
    
    RECT artist_rect = {text_left, window_height / 2 + margin - (int)(window_height * 0.15), text_right, window_height - margin - (int)(window_height * 0.15)};
    pfc::stringcvt::string_wide_from_utf8 wide_artist(m_current_artist.c_str());
    SetTextColor(hdc, RGB(180, 180, 180)); // Slightly dimmer for artist
    DrawText(hdc, wide_artist.get_ptr(), -1, &artist_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    
    
    // Draw progress bar at bottom - moved up by 10%
    int progress_bar_height = 5;
    int progress_bar_y = window_height - progress_bar_height - 2 - (int)(window_height * 0.1); // Move up by 10%
    int progress_bar_left = text_left; // Start under artist name
    int progress_bar_width = text_right - text_left - 40; // Leave space for time display
    
    // Calculate progress
    double progress_ratio = (m_track_length > 0) ? (m_current_time / m_track_length) : 0.0;
    if (progress_ratio > 1.0) progress_ratio = 1.0;
    if (progress_ratio < 0.0) progress_ratio = 0.0;
    
    // Draw progress bar background
    RECT progress_bg_rect = {progress_bar_left, progress_bar_y, progress_bar_left + progress_bar_width, progress_bar_y + progress_bar_height};
    HBRUSH progress_bg_brush = CreateSolidBrush(RGB(80, 80, 80));
    FillRect(hdc, &progress_bg_rect, progress_bg_brush);
    DeleteObject(progress_bg_brush);
    
    // Draw progress bar fill
    int progress_fill_width = (int)(progress_bar_width * progress_ratio);
    if (progress_fill_width > 0) {
        RECT progress_fill_rect = {progress_bar_left, progress_bar_y, progress_bar_left + progress_fill_width, progress_bar_y + progress_bar_height};
        HBRUSH progress_fill_brush = CreateSolidBrush(RGB(100, 149, 237)); // Cornflower blue
        FillRect(hdc, &progress_fill_rect, progress_fill_brush);
        DeleteObject(progress_fill_brush);
    }
    
    // Draw elapsed time (count up, like Docked and Undocked modes)
    if (m_is_playing) {
        int elapsed_min = (int)(m_current_time / 60);
    int elapsed_sec = (int)m_current_time % 60;
    
    wchar_t time_str[16];
    swprintf_s(time_str, 16, L"%d:%02d", elapsed_min, elapsed_sec);
    
    // Use a slightly smaller font for the timer in compact mode
    HFONT time_font = CreateFont(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    bool need_delete_time_font = true;
    SelectObject(hdc, time_font);
    SetTextColor(hdc, RGB(255, 255, 255)); // White to match track title
    
        RECT time_rect = {progress_bar_left + progress_bar_width + 5, progress_bar_y - 10, window_width - margin, progress_bar_y + progress_bar_height + 6};
        DrawText(hdc, time_str, -1, &time_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        
        if (need_delete_time_font) {
            DeleteObject(time_font);
        }
    }
    
    // Draw compact control overlay if hovering over text area
    draw_compact_control_overlay(hdc, window_width, window_height);
    
    // Cleanup fonts
    SelectObject(hdc, old_font);
    if (need_delete_title) {
        DeleteObject(title_font);
    }
    if (need_delete_artist) {
        DeleteObject(artist_font);
    }
}

void control_panel::draw_time_info(HDC hdc, const RECT& client_rect) {
    if (!m_is_playing) return;

    SetTextColor(hdc, RGB(255, 255, 255)); // Same color as track title
    SetBkMode(hdc, TRANSPARENT);
    
    // Format current position time in 24hr format (MM:SS with leading zeros)
    int current_min = (int)(m_current_time / 60);
    int current_sec = (int)m_current_time % 60;
    
    pfc::string8 time_str;
    time_str << pfc::format_int(current_min, 2) << ":" << pfc::format_int(current_sec, 2);
    
    // Draw time with same font as track title (18pt, bold)
    HFONT time_font = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT old_font = (HFONT)SelectObject(hdc, time_font);
    
    RECT time_rect = {client_rect.right - 60, 20, client_rect.right - 10, 45};
    pfc::stringcvt::string_wide_from_utf8 wide_time(time_str.c_str());
    DrawText(hdc, wide_time.get_ptr(), -1, &time_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    
    SelectObject(hdc, old_font);
    DeleteObject(time_font);
}

void control_panel::draw_track_info_overlay(HDC hdc, int window_width, int window_height) {
    // Don't return early when no title/artist - overlay should still appear for controls
    
    // Create semi-transparent overlay background at the top using GDI+ alpha blending (glass effect)
    int overlay_height = 70; // Height of the overlay area
    
    if (m_overlay_opacity > 0) {
        // Use GDI+ for true alpha blending (glass effect)
        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        
        // Calculate alpha based on overlay opacity (0-100 -> 0-255 range, but make it semi-transparent)
        // Use around 60% max opacity for glass effect
        int alpha = (180 * m_overlay_opacity) / 100;
        
        Gdiplus::SolidBrush overlayBrush(Gdiplus::Color(alpha, 20, 20, 20));
        // Start at -1 to eliminate sub-pixel gap at top edge from anti-aliasing
        Gdiplus::RectF overlayRect(-1.0f, -1.0f, (float)window_width + 2.0f, (float)overlay_height + 1.0f);
        graphics.FillRectangle(&overlayBrush, overlayRect);
    }
    
    // Draw track info text on top of the overlay (only when overlay has opacity)
    if (m_overlay_opacity > 0) {
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
    
    // Track title (use configured track font, fallback to default if not set)
    HFONT title_font = m_track_font;
    bool need_delete_title = false;
    if (!title_font) {
        title_font = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        need_delete_title = true;
    }
    HFONT old_font = (HFONT)SelectObject(hdc, title_font);
    
    // Position title - balanced spacing between title and artist
    int title_y = (overlay_height / 2) - 18;
    RECT title_rect = {15, title_y, window_width - 15, title_y + 20};
    if (!m_current_title.is_empty()) {
        pfc::stringcvt::string_wide_from_utf8 wide_title(m_current_title.c_str());
        DrawText(hdc, wide_title.get_ptr(), -1, &title_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        DrawText(hdc, L"[No Track Title]", -1, &title_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    
    SelectObject(hdc, old_font);
    if (need_delete_title) {
        DeleteObject(title_font);
    }
    
    // Artist name (use configured artist font, fallback to default if not set)
    HFONT artist_font = m_artist_font;
    bool need_delete_artist = false;
    if (!artist_font) {
        artist_font = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        need_delete_artist = true;
    }
    old_font = (HFONT)SelectObject(hdc, artist_font);
    
    // Position artist - balanced spacing from title
    int artist_y = (overlay_height / 2) + 6;
    RECT artist_rect = {15, artist_y, window_width - 15, artist_y + 16};
    if (!m_current_artist.is_empty()) {
        pfc::stringcvt::string_wide_from_utf8 wide_artist(m_current_artist.c_str());
        DrawText(hdc, wide_artist.get_ptr(), -1, &artist_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        DrawText(hdc, L"[No Artist]", -1, &artist_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    
        SelectObject(hdc, old_font);
        if (need_delete_artist) {
            DeleteObject(artist_font);
        }
    } // End of should_draw_overlay condition
}

void control_panel::draw_control_overlay(HDC hdc, int window_width, int window_height) {
    // Create bottom overlay with glass effect using GDI+ alpha blending
    const int overlay_height = 70; // Height for control buttons
    
    if (m_overlay_opacity > 0) {
        // Use GDI+ for true alpha blending (glass effect)
        Gdiplus::Graphics graphics(hdc);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        
        // Calculate alpha based on overlay opacity (around 60% max opacity for glass effect)
        int alpha = (180 * m_overlay_opacity) / 100;
        
        Gdiplus::SolidBrush overlayBrush(Gdiplus::Color(alpha, 20, 20, 20));
        Gdiplus::RectF overlayRect(0.0f, (float)(window_height - overlay_height), (float)window_width, (float)overlay_height);
        graphics.FillRectangle(&overlayBrush, overlayRect);
        
        // Draw control buttons (Previous, Play/Pause, Next)
        int button_size = 24; // Size of each button except play
        int play_button_size = 38; // Larger play button
        int button_spacing = 60; // Space between button centers
        int center_x = window_width / 2;
        // Calculate center of the bottom overlay area
        int overlay_top = window_height - overlay_height;
        int center_y = overlay_top + (overlay_height / 2); // Perfectly centered vertically
        
        // Previous button
        int prev_x = center_x - button_spacing;
        int prev_y = center_y;
        if (m_is_undocked && !m_is_artwork_expanded) {
            draw_previous_icon_with_opacity(hdc, prev_x, prev_y, button_size, m_button_opacity);
        } else {
            draw_previous_icon(hdc, prev_x, prev_y, button_size);
        }
        
        // Play/Pause button
        int play_x = center_x;
        int play_y = center_y;
        if (m_is_undocked && !m_is_artwork_expanded) {
            if (m_is_playing && !m_is_paused) {
                draw_pause_icon_with_opacity(hdc, play_x, play_y, play_button_size, m_button_opacity);
            } else {
                draw_play_icon_with_opacity(hdc, play_x, play_y, play_button_size, m_button_opacity);
            }
        } else {
            if (m_is_playing && !m_is_paused) {
                draw_pause_icon(hdc, play_x, play_y, play_button_size);
            } else {
                draw_play_icon(hdc, play_x, play_y, play_button_size);
            }
        }
        
        // Next button
        int next_x = center_x + button_spacing;
        int next_y = center_y;
        if (m_is_undocked && !m_is_artwork_expanded) {
            draw_next_icon_with_opacity(hdc, next_x, next_y, button_size, m_button_opacity);
        } else {
            draw_next_icon(hdc, next_x, next_y, button_size);
        }
        
        // Shuffle button (left of Previous)
        int shuffle_x = center_x - button_spacing * 2;
        draw_shuffle_icon(hdc, shuffle_x, center_y, button_size);
        
        // Repeat button (right of Next)
        int repeat_x = center_x + button_spacing * 2;
        draw_repeat_icon(hdc, repeat_x, center_y, button_size);


        // Close button in top-left corner for expanded artwork mode
        // AND Collapse triangle in top-right corner
        int close_size = 16;
        int close_x = 10;
        int close_y = 10;
        
        // Draw collapse triangle (top-right)
        // Position it similarly to close button but on the right
        int collapse_size = 12;
        int collapse_x = window_width - 15;
        int collapse_y = 15;
        
        // Calculate dynamic opacity for these corner controls
        // Reuse m_button_opacity or m_overlay_opacity? 
        // Corner controls should probably show when mouse is over the window.
        // Let's use m_button_opacity to fade them in/out with standard controls.
        
        draw_collapse_triangle(hdc, collapse_x, collapse_y, collapse_size, m_button_opacity);
        
    }
}


void control_panel::draw_undocked_artwork_overlay(HDC hdc, int window_width, int window_height) {
    // Calculate artwork area (different for compact vs normal mode)
    int art_size, art_x, art_y;
    if (m_is_compact_mode) {
        // Compact mode artwork positioning
        int margin = 5;
        art_size = window_height - (2 * margin);
        art_x = margin;
        art_y = margin;
    } else {
        // Normal undocked mode artwork positioning
        art_size = (80 < (window_width - 30) ? 80 : (window_width - 30));
        art_size = (art_size < (window_height - 30) ? art_size : (window_height - 30));
        art_x = 15;
        art_y = 15;
    }
    
    // Draw vertical arrows with opacity (no background overlay)
    if (m_undocked_overlay_opacity > 0) {
        // Calculate arrow opacity based on overlay opacity
        int arrow_opacity = (255 * m_undocked_overlay_opacity) / 100;
        
        // Draw vertical arrows centered in the artwork area
        int center_x = art_x + art_size / 2;
        int arrow_size = 16;
        int arrow_spacing = 20;
        
        // Up arrow (closer to top)
        int up_arrow_y = art_y + art_size / 4;
        draw_up_arrow_with_opacity(hdc, center_x, up_arrow_y, arrow_size, arrow_opacity);
        
        // Down arrow (closer to bottom)
        int down_arrow_y = art_y + (3 * art_size) / 4;
        draw_down_arrow_with_opacity(hdc, center_x, down_arrow_y, arrow_size, arrow_opacity);
    }
}

void control_panel::draw_compact_control_overlay(HDC hdc, int window_width, int window_height) {
    if (!m_compact_controls_visible) {
        return;
    }

    // Calculate text area where controls will be drawn (same as text layout in paint_compact_mode)
    int margin = 5;
    int art_size = window_height - (2 * margin);
    int text_left = margin + art_size + margin;
    int text_right = window_width - margin;
    int text_area_width = text_right - text_left;

    // Use a simplified text area for the overlay
    int text_top = 0;
    int text_bottom = window_height;
    int text_area_height = text_bottom - text_top;
    int overlay_bottom = window_height - 18; // Leave minimal space for progress bar and time

    // Create control button layout in the text area (5 buttons: shuffle, prev, play, next, repeat)
    // Create control button layout in the text area (5 buttons: shuffle, prev, play, next, repeat)
    int button_size = 24; // Standard size matching other modes
    int play_button_size = 36; // Larger play button (a bit smaller than full overlay to fit text area)
    
    // Adjust spacing to account for larger center button
    int button_spacing = 10; 
    
    // Width calculation: 
    // Shuffle(24) + space(10) + Prev(24) + space(10) + Play(36) + space(10) + Next(24) + space(10) + Repeat(24)
    int total_buttons_width = (4 * button_size) + play_button_size + (4 * button_spacing);

    // Center the buttons horizontally in the text area (shifted left slightly per user request)
    int buttons_start_x = text_left + (text_area_width - total_buttons_width) / 2 - 15;

    // Position buttons slightly below center
    // Ideally center vertically based on the largest button (Play)
    int button_y = margin + (window_height - 2 * margin - play_button_size) / 2 + 5; 
    // So button_y should be the vertical center line.
    
    // Correct vertical centering: center within the overlay area (0 to overlay_bottom)
    // Add small offset (+5) to lower buttons slightly for better visual balance
    int center_y_line = (overlay_bottom / 2) + 5;

    // Calculate button CENTER positions
    // Start x is the left edge of the group.
    
    // Shuffle center: starts at buttons_start_x + size/2
    int shuffle_x = buttons_start_x + button_size / 2;
    
    // Prev center: shuffle_end + space + size/2 = shuffle_center + size/2 + space + size/2
    int prev_x = shuffle_x + button_size/2 + button_spacing + button_size/2;
    
    // Play center: prev_center + size/2 + space + play_size/2
    int play_x = prev_x + button_size/2 + button_spacing + play_button_size/2;
    
    // Next center: play_center + play_size/2 + space + size/2
    int next_x = play_x + play_button_size/2 + button_spacing + button_size/2;
    
    // Repeat center: next_center + size/2 + space + size/2
    int repeat_x = next_x + button_size/2 + button_spacing + button_size/2;

    // Create background overlay to completely hide the text (don't cover progress bar)
    RECT overlay_rect = {text_left, 0, text_right, overlay_bottom};
    HBRUSH overlay_brush = CreateSolidBrush(RGB(28, 28, 28)); // Solid background to completely hide text
    FillRect(hdc, &overlay_rect, overlay_brush);
    DeleteObject(overlay_brush);

    // Draw control buttons
    // Shuffle button
    draw_shuffle_icon(hdc, shuffle_x, center_y_line, button_size);
    
    // Previous button
    draw_previous_icon(hdc, prev_x, center_y_line, button_size);
    
    // Play/Pause button
    if (m_is_playing && !m_is_paused) {
        draw_pause_icon(hdc, play_x, center_y_line, play_button_size);
    } else {
        draw_play_icon(hdc, play_x, center_y_line, play_button_size);
    }
    // Next button
    draw_next_icon(hdc, next_x, center_y_line, button_size);
    
    // Repeat button
    draw_repeat_icon(hdc, repeat_x, center_y_line, button_size);
}

void control_panel::start_roll_animation(bool to_compact) {
    if (!m_visible || !m_is_undocked || m_is_artwork_expanded || m_is_rolling_animation) return;
    
    m_is_rolling_animation = true;
    m_rolling_to_compact = to_compact;
    m_roll_animation_step = 0;
    m_roll_animation_start_time = GetTickCount();
    
    // Set timer for animation updates
    SetTimer(m_control_window, UPDATE_TIMER_ID + 3, 16, nullptr); // ~60fps - use unique timer ID
}

void control_panel::update_roll_animation() {
    if (!m_is_rolling_animation) return;
    
    DWORD current_time = GetTickCount();
    DWORD elapsed = current_time - m_roll_animation_start_time;
    
    if (elapsed >= ROLL_ANIMATION_DURATION) {
        // Animation complete
        m_is_rolling_animation = false;
        KillTimer(m_control_window, UPDATE_TIMER_ID + 3);
        
        // Switch modes
        if (m_rolling_to_compact) {
            // Save current normal dimensions and switch to compact
            RECT current_rect;
            GetWindowRect(m_control_window, &current_rect);
            m_saved_normal_width = current_rect.right - current_rect.left;
            m_saved_normal_height = current_rect.bottom - current_rect.top;
            m_is_compact_mode = true;
            
            // Resize to compact dimensions
            int compact_width = m_saved_compact_width >= 320 ? m_saved_compact_width : 320;
            SetWindowPos(m_control_window, nullptr, 0, 0, compact_width, 75, 
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        } else {
            // Switch to normal undocked mode
            m_is_compact_mode = false;
            
            // Resize to normal dimensions
            int normal_width = m_saved_normal_width >= 300 ? m_saved_normal_width : 338;
            int normal_height = m_saved_normal_height >= 110 ? m_saved_normal_height : 120;
            SetWindowPos(m_control_window, nullptr, 0, 0, normal_width, normal_height, 
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        
        InvalidateRect(m_control_window, nullptr, TRUE);
        return;
    }
    
    // Calculate animation progress (0.0 to 1.0)
    float progress = (float)elapsed / (float)ROLL_ANIMATION_DURATION;
    if (progress > 1.0f) progress = 1.0f;
    
    // Apply easing function for smooth animation
    progress = progress * progress * (3.0f - 2.0f * progress); // Smoothstep
    
    // Get current and target dimensions
    RECT current_rect;
    GetWindowRect(m_control_window, &current_rect);
    int current_width = current_rect.right - current_rect.left;
    int current_height = current_rect.bottom - current_rect.top;
    
    int start_width, start_height, target_width, target_height;
    
    if (m_rolling_to_compact) {
        start_width = m_saved_normal_width >= 300 ? m_saved_normal_width : 338;
        start_height = m_saved_normal_height >= 110 ? m_saved_normal_height : 120;
        target_width = m_saved_compact_width >= 320 ? m_saved_compact_width : 320;
        target_height = 75;
    } else {
        start_width = m_saved_compact_width >= 320 ? m_saved_compact_width : 320;
        start_height = 75;
        target_width = m_saved_normal_width >= 300 ? m_saved_normal_width : 338;
        target_height = m_saved_normal_height >= 110 ? m_saved_normal_height : 120;
    }
    
    // Interpolate dimensions
    int new_width = start_width + (int)((target_width - start_width) * progress);
    int new_height = start_height + (int)((target_height - start_height) * progress);
    
    // Apply new dimensions
    SetWindowPos(m_control_window, nullptr, 0, 0, new_width, new_height, 
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    
    InvalidateRect(m_control_window, nullptr, TRUE);
}
