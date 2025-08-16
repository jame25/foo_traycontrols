#include "stdafx.h"
#include "control_panel.h"
#include "preferences.h"

// Timer constants
#define TIMEOUT_TIMER_ID 9999  // Use unique timer ID to avoid conflicts
#define ANIMATION_TIMER_ID 4003
#define OVERLAY_TIMER_ID 4004
#define FADE_TIMER_ID 4005
#define BUTTON_FADE_TIMER_ID 9001

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
    , m_last_compact_mouse_time(0) {
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


// Vector-drawn icon implementations
void control_panel::draw_play_icon(HDC hdc, int x, int y, int size) {
    // Create a play triangle pointing right
    POINT triangle[3];
    int half_size = size / 2;
    triangle[0] = {x - half_size/2, y - half_size};     // Top left
    triangle[1] = {x - half_size/2, y + half_size};     // Bottom left  
    triangle[2] = {x + half_size, y};                   // Right point
    
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    
    Polygon(hdc, triangle, 3);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void control_panel::draw_pause_icon(HDC hdc, int x, int y, int size) {
    // Create two vertical rectangles
    int half_size = size / 2;
    int bar_width = size / 5;
    int gap = size / 6;
    
    RECT left_bar = {x - gap - bar_width, y - half_size, x - gap, y + half_size};
    RECT right_bar = {x + gap, y - half_size, x + gap + bar_width, y + half_size};
    
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &left_bar, brush);
    FillRect(hdc, &right_bar, brush);
    DeleteObject(brush);
}

void control_panel::draw_previous_icon(HDC hdc, int x, int y, int size) {
    // Draw vertical line + triangle pointing left
    int half_size = size / 2;
    int bar_width = 2;
    
    // Vertical line on the left
    RECT bar = {x - half_size, y - half_size, x - half_size + bar_width, y + half_size};
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hdc, &bar, brush);
    
    // Triangle pointing left
    POINT triangle[3];
    triangle[0] = {x + half_size/2, y - half_size/2};   // Top right
    triangle[1] = {x + half_size/2, y + half_size/2};   // Bottom right
    triangle[2] = {x - half_size/4, y};                 // Left point
    
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    
    Polygon(hdc, triangle, 3);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void control_panel::draw_next_icon(HDC hdc, int x, int y, int size) {
    // Draw triangle pointing right + vertical line
    int half_size = size / 2;
    int bar_width = 2;
    
    // Triangle pointing right
    POINT triangle[3];
    triangle[0] = {x - half_size/2, y - half_size/2};   // Top left
    triangle[1] = {x - half_size/2, y + half_size/2};   // Bottom left
    triangle[2] = {x + half_size/4, y};                 // Right point
    
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    
    Polygon(hdc, triangle, 3);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    
    // Vertical line on the right
    RECT bar = {x + half_size - bar_width, y - half_size, x + half_size, y + half_size};
    FillRect(hdc, &bar, brush);
    
    DeleteObject(pen);
    DeleteObject(brush);
}

void control_panel::draw_up_arrow(HDC hdc, int x, int y, int size) {
    // Draw triangle pointing up
    int half_size = size / 2;
    
    POINT triangle[3];
    triangle[0] = {x, y - half_size};                       // Top point
    triangle[1] = {x - half_size/2, y + half_size/4};       // Bottom left
    triangle[2] = {x + half_size/2, y + half_size/4};       // Bottom right
    
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    
    Polygon(hdc, triangle, 3);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void control_panel::draw_down_arrow(HDC hdc, int x, int y, int size) {
    // Draw triangle pointing down
    int half_size = size / 2;
    
    POINT triangle[3];
    triangle[0] = {x, y + half_size};                       // Bottom point
    triangle[1] = {x - half_size/2, y - half_size/4};       // Top left
    triangle[2] = {x + half_size/2, y - half_size/4};       // Top right
    
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    
    Polygon(hdc, triangle, 3);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void control_panel::draw_up_arrow_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    // Draw triangle pointing up with specified opacity (0-255)
    int half_size = size / 2;
    
    POINT triangle[3];
    triangle[0] = {x, y - half_size};                       // Top point
    triangle[1] = {x - half_size/2, y + half_size/4};       // Bottom left
    triangle[2] = {x + half_size/2, y + half_size/4};       // Bottom right
    
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
    // Draw triangle pointing down with specified opacity (0-255)
    int half_size = size / 2;
    
    POINT triangle[3];
    triangle[0] = {x, y + half_size};                       // Bottom point
    triangle[1] = {x - half_size/2, y - half_size/4};       // Top left
    triangle[2] = {x + half_size/2, y - half_size/4};       // Top right
    
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

// Opacity-based icon drawing for button fade effect
void control_panel::draw_play_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    // Create a play triangle pointing right with specified opacity (0-100)
    POINT triangle[3];
    int half_size = size / 2;
    triangle[0] = {x - half_size/2, y - half_size};     // Top left
    triangle[1] = {x - half_size/2, y + half_size};     // Bottom left  
    triangle[2] = {x + half_size, y};                   // Right point
    
    // Fade from white (255,255,255) to background (32,32,32)
    int color_value = 32 + ((255 - 32) * opacity) / 100;
    HBRUSH brush = CreateSolidBrush(RGB(color_value, color_value, color_value));
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(color_value, color_value, color_value));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    
    Polygon(hdc, triangle, 3);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void control_panel::draw_pause_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    // Create two vertical rectangles with specified opacity (0-100)
    int half_size = size / 2;
    int bar_width = size / 5;
    int gap = size / 6;
    
    // Fade from white (255,255,255) to background (32,32,32)
    int color_value = 32 + ((255 - 32) * opacity) / 100;
    HBRUSH brush = CreateSolidBrush(RGB(color_value, color_value, color_value));
    
    RECT left_bar = {x - gap/2 - bar_width, y - half_size, x - gap/2, y + half_size};
    RECT right_bar = {x + gap/2, y - half_size, x + gap/2 + bar_width, y + half_size};
    
    FillRect(hdc, &left_bar, brush);
    FillRect(hdc, &right_bar, brush);
    
    DeleteObject(brush);
}

void control_panel::draw_previous_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    // Draw vertical line + triangle pointing left with specified opacity (0-100)
    int half_size = size / 2;
    int bar_width = 2;
    
    // Fade from white (255,255,255) to background (32,32,32)
    int color_value = 32 + ((255 - 32) * opacity) / 100;
    HBRUSH brush = CreateSolidBrush(RGB(color_value, color_value, color_value));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(color_value, color_value, color_value));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    
    // Vertical line on the left
    RECT bar = {x - half_size, y - half_size, x - half_size + bar_width, y + half_size};
    FillRect(hdc, &bar, brush);
    
    // Triangle pointing left  
    POINT triangle[3];
    triangle[0] = {x - half_size/4, y - half_size/2};   // Top right
    triangle[1] = {x - half_size/4, y + half_size/2};   // Bottom right
    triangle[2] = {x + half_size/2, y};                 // Left point
    
    Polygon(hdc, triangle, 3);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void control_panel::draw_next_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity) {
    // Draw triangle pointing right + vertical line with specified opacity (0-100)
    int half_size = size / 2;
    int bar_width = 2;
    
    // Fade from white (255,255,255) to background (32,32,32)
    int color_value = 32 + ((255 - 32) * opacity) / 100;
    HBRUSH brush = CreateSolidBrush(RGB(color_value, color_value, color_value));
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(color_value, color_value, color_value));
    HPEN old_pen = (HPEN)SelectObject(hdc, pen);
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
    
    // Triangle pointing right
    POINT triangle[3];
    triangle[0] = {x - half_size/2, y - half_size/2};   // Top left
    triangle[1] = {x - half_size/2, y + half_size/2};   // Bottom left
    triangle[2] = {x + half_size/4, y};                 // Right point
    
    Polygon(hdc, triangle, 3);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    
    // Vertical line on the right
    RECT bar = {x + half_size - bar_width, y - half_size, x + half_size, y + half_size};
    FillRect(hdc, &bar, brush);
    
    DeleteObject(pen);
    DeleteObject(brush);
}


// Font management methods
void control_panel::load_fonts() {
    cleanup_fonts();
    
    if (get_cp_use_custom_fonts()) {
        // Use custom Control Panel fonts
        LOGFONT artist_lf = get_cp_artist_font();
        LOGFONT track_lf = get_cp_track_font();
        
        m_artist_font = CreateFontIndirect(&artist_lf);
        m_track_font = CreateFontIndirect(&track_lf);
    } else {
        // Use default fonts with larger sizes to work around minimum size constraints
        LOGFONT artist_lf = get_default_font(true, 13);   // Regular, 13pt instead of 11pt
        LOGFONT track_lf = get_default_font(false, 16);   // Bold, 16pt instead of 14pt
        
        m_artist_font = CreateFontIndirect(&artist_lf);
        m_track_font = CreateFontIndirect(&track_lf);
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
    
    // Trigger repaint if visible
    if (m_visible && m_control_window) {
        InvalidateRect(m_control_window, nullptr, TRUE);
    }
}

void control_panel::set_undocked(bool undocked) {
    m_is_undocked = undocked;
    
    // Reset compact mode when switching undocked state
    if (undocked) {
        m_is_compact_mode = false; // Start in normal undocked mode, not compact
    }
    
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
            
        }
        
        // Update display after action
        update_track_info();
        
    } catch (...) {
        // Ignore playback control errors
    }
}

void control_panel::handle_timer() {
    // Use simple behavior (like original) when in basic popup mode
    if (!m_is_undocked && !m_is_artwork_expanded) {
        // Original simple timer behavior - just update time position
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
                file_info_impl info;
                if (current_track->get_info(info)) {
                    const char* title_str = info.meta_get("TITLE", 0);
                    const char* artist_str = info.meta_get("ARTIST", 0);
                    if (title_str) current_title = title_str;
                    if (artist_str) current_artist = artist_str;
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
                panel->paint_control_panel(hdc);
                
                
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
                    int button_spacing = 20;
                    int total_buttons_width = (3 * button_size) + (2 * button_spacing);
                    int buttons_start_x = text_left + (text_area_width - total_buttons_width) / 2;
                    int button_y = margin + (window_height - 2 * margin - button_size) / 2 + 5;
                    
                    int prev_x = buttons_start_x + button_size / 2;
                    int play_x = prev_x + button_size + button_spacing;
                    int next_x = play_x + button_size + button_spacing;
                    
                    // Check which button was clicked (use button_size/2 as click radius)
                    int click_radius = button_size / 2 + 2; // Slightly larger click area
                    
                    if (abs(pt.x - prev_x) <= click_radius && abs(pt.y - button_y) <= click_radius) {
                        panel->handle_button_click(BTN_PREV);
                        return 0;
                    } else if (abs(pt.x - play_x) <= click_radius && abs(pt.y - button_y) <= click_radius) {
                        panel->handle_button_click(BTN_PLAYPAUSE);
                        return 0;
                    } else if (abs(pt.x - next_x) <= click_radius && abs(pt.y - button_y) <= click_radius) {
                        panel->handle_button_click(BTN_NEXT);
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
            
            // Note: Normal undocked mode clicks are now handled as dragging via HTCAPTION
            // Double-clicks are handled by WM_NCLBUTTONDBLCLK
            // Only artwork and control button clicks reach here via HTCLIENT
            
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
                    // First check for control button clicks in bottom overlay area
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    int window_width = client_rect.right - client_rect.left;
                    int window_height = client_rect.bottom - client_rect.top;
                    const int overlay_height = 46; // Reduced by another 5%
                    
                    // Check if click is in bottom overlay area and overlay is visible
                    if (panel->m_overlay_visible && pt.y >= window_height - overlay_height) {
                        // Calculate button positions (same as in draw_control_overlay)
                        int button_size = 24;
                        int button_spacing = 60;
                        int center_x = window_width / 2;
                        // Calculate center of the bottom overlay area, then lower by 20%
                        int overlay_top = window_height - overlay_height;
                        int center_y = overlay_top + (overlay_height / 2) + (overlay_height / 5);
                        
                        // Check Previous button - use larger click area covering more of the overlay
                        int prev_x = center_x - button_spacing;
                        int click_area_size = button_size + 8; // Expand click area
                        // Use overlay center for click detection, not the lowered visual position
                        int click_center_y = overlay_top + (overlay_height / 2);
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
                        
                    }
                    
                    
                    // Handle double-click detection for toggling mode
                    DWORD current_time = GetTickCount();
                    DWORD double_click_time = GetDoubleClickTime();
                    int double_click_x = GetSystemMetrics(SM_CXDOUBLECLK);
                    int double_click_y = GetSystemMetrics(SM_CYDOUBLECLK);
                    
                    // Check if this is a double-click
                    if ((current_time - panel->m_last_click_time) <= double_click_time &&
                        abs(pt.x - panel->m_last_click_pos.x) <= double_click_x &&
                        abs(pt.y - panel->m_last_click_pos.y) <= double_click_y) {
                        // Double-click detected - toggle artwork expanded mode
                        panel->toggle_artwork_expanded();
                        return 0;
                    }
                    
                    // Store this click for next double-click detection
                    panel->m_last_click_time = current_time;
                    panel->m_last_click_pos = pt;
                    
                    // Store click position for potential dragging, but don't start dragging yet
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
                
                // Check which button was clicked - adaptive to window size
                RECT client_rect;
                GetClientRect(hwnd, &client_rect);
                int window_width = client_rect.right - client_rect.left;
                int window_height = client_rect.bottom - client_rect.top;
                
                
                int button_y = window_height - 30;
                int button_spacing = 50;
                int center_x = window_width / 2;
                
                // Check if click is in button row (±10px around button_y for 20px tall area)
                if (pt.y >= button_y - 10 && pt.y <= button_y + 10) {
                    if (pt.x >= center_x - button_spacing - 10 && pt.x < center_x - button_spacing + 10) {
                        panel->handle_button_click(BTN_PREV);        // Previous
                    } else if (pt.x >= center_x - 10 && pt.x < center_x + 10) {
                        panel->handle_button_click(BTN_PLAYPAUSE);   // Play/Pause
                    } else if (pt.x >= center_x + button_spacing - 10 && pt.x < center_x + button_spacing + 10) {
                        panel->handle_button_click(BTN_NEXT);        // Next
                    }
                }
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
            // Handle clicks on the non-client area (HTCAPTION area in expanded mode)
            if (panel && panel->m_is_artwork_expanded && wparam == HTCAPTION) {
                // Convert screen coordinates to client coordinates
                POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                ScreenToClient(hwnd, &pt);
                
                DWORD current_time = GetTickCount();
                DWORD double_click_time = GetDoubleClickTime();
                int double_click_x = GetSystemMetrics(SM_CXDOUBLECLK);
                int double_click_y = GetSystemMetrics(SM_CYDOUBLECLK);
                
                // Check if this is a double-click
                if ((current_time - panel->m_last_click_time) <= double_click_time &&
                    abs(pt.x - panel->m_last_click_pos.x) <= double_click_x &&
                    abs(pt.y - panel->m_last_click_pos.y) <= double_click_y) {
                    // Double-click detected - toggle artwork expanded mode
                    panel->toggle_artwork_expanded();
                    return 0;
                }
                
                // Store this click for next double-click detection
                panel->m_last_click_time = current_time;
                panel->m_last_click_pos = pt;
                
                // Let default processing handle dragging
                break;
            }
            break;
            
        case WM_NCLBUTTONDBLCLK:
            // Handle double-click on non-client area (HTCAPTION)
            if (panel && wparam == HTCAPTION) {
                if (panel->m_is_artwork_expanded) {
                    // In expanded mode - return to undocked
                    panel->toggle_artwork_expanded();
                    return 0;
                } else if (panel->m_is_compact_mode) {
                    // In compact mode - roll down to normal mode
                    panel->start_roll_animation(false); // false = roll to normal mode
                    return 0;
                } else if (panel->m_is_undocked && !panel->m_is_artwork_expanded && !panel->m_is_compact_mode) {
                    // In normal undocked mode - roll up to compact mode
                    panel->start_roll_animation(true); // true = roll to compact mode
                    return 0;
                }
            }
            break;
            
        case WM_LBUTTONDBLCLK:
            {
                POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                
                // Handle double-click in artwork expanded mode - anywhere on window returns to undocked
                if (panel && panel->m_is_artwork_expanded) {
                    panel->toggle_artwork_expanded();
                    return 0;
                }
                
                // Handle double-click in compact mode text area - return to normal undocked mode
                if (panel && panel->m_is_compact_mode) {
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    int window_width = client_rect.right - client_rect.left;
                    int window_height = client_rect.bottom - client_rect.top;
                    int margin = 5;
                    int art_size = window_height - (2 * margin);
                    int text_left = margin + art_size + margin;
                    int text_right = window_width - margin;
                    
                    // Check if double-click is in text area
                    if (pt.x >= text_left && pt.x < text_right) {
                        panel->start_roll_animation(false); // false = roll to normal mode
                        return 0;
                    }
                }
                
                return 0;
            }
            
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
                        InvalidateRect(hwnd, nullptr, TRUE);
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
                        // Only repaint if state actually changed
                        if (state_changed) {
                            InvalidateRect(hwnd, nullptr, TRUE);
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
                        // Only repaint if state actually changed
                        if (state_changed) {
                            InvalidateRect(hwnd, nullptr, TRUE);
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
                            InvalidateRect(hwnd, nullptr, TRUE);
                            
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
                            InvalidateRect(hwnd, nullptr, TRUE);
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
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
            }
            break;
            
        case WM_MOUSELEAVE:
            if (panel && panel->m_is_artwork_expanded && panel->m_overlay_visible) {
                // Start immediate fade when mouse leaves the window
                KillTimer(hwnd, OVERLAY_TIMER_ID);
                panel->m_fade_start_time = GetTickCount();
                SetTimer(hwnd, FADE_TIMER_ID, 50, nullptr); // Start fade immediately
            } else if (panel && !panel->m_is_artwork_expanded && (panel->m_undocked_overlay_visible || panel->m_compact_controls_visible)) {
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
                    
                    // Check for resize areas (corners and edges)
                    bool at_left = pt.x < resize_border;
                    bool at_right = pt.x > client_rect.right - resize_border;
                    bool at_top = pt.y < resize_border;
                    bool at_bottom = pt.y > client_rect.bottom - resize_border;
                    
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
                    
                    // Return horizontal resize handles only
                    if (at_left) return HTLEFT;
                    if (at_right) return HTRIGHT;
                    
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
                    
                    // Calculate adaptive button and artwork positions
                    int window_width = client_rect.right - client_rect.left;
                    int window_height = client_rect.bottom - client_rect.top;
                    
                    
                    int button_y = window_height - 30;
                    int button_spacing = 50;
                    // Normal undocked mode - allow dragging from anywhere except artwork and control buttons
                    int center_x = window_width / 2;
                    int art_size = (80 < (window_width - 30) ? 80 : (window_width - 30));
                    art_size = (art_size < (window_height - 30) ? art_size : (window_height - 30));
                    
                    // Check if click is in button areas - don't allow dragging here
                    if (pt.y >= button_y - 10 && pt.y <= button_y + 10) { // Button row
                        if ((pt.x >= center_x - button_spacing - 10 && pt.x < center_x - button_spacing + 10) ||    // Previous
                            (pt.x >= center_x - 10 && pt.x < center_x + 10) ||                                      // Play/Pause
                            (pt.x >= center_x + button_spacing - 10 && pt.x < center_x + button_spacing + 10)) {    // Next
                            return HTCLIENT; // Normal button behavior
                        }
                    }
                    
                    // Check if click is in artwork area - don't allow dragging here
                    if (pt.x >= 15 && pt.x < 15 + art_size && pt.y >= 15 && pt.y < 15 + art_size) {
                        return HTCLIENT; // Normal click behavior for artwork
                    }
                    
                    // Check for horizontal resize areas only (left and right edges)
                    const int resize_border = 6;
                    bool at_left = pt.x < resize_border;
                    bool at_right = pt.x > window_width - resize_border;
                    
                    // Only return resize handles if not in button or artwork areas
                    bool in_button_area = (pt.y >= button_y - 10 && pt.y <= button_y + 10);
                    bool in_artwork_area = (pt.x >= 15 && pt.x < 15 + art_size && pt.y >= 15 && pt.y < 15 + art_size);
                    
                    if (!in_button_area && !in_artwork_area) {
                        // Return horizontal resize handles only
                        if (at_left) return HTLEFT;
                        if (at_right) return HTRIGHT;
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
                    
                    // Calculate adaptive positions for docked mode
                    RECT client_rect;
                    GetClientRect(hwnd, &client_rect);
                    int window_width = client_rect.right - client_rect.left;
                    int window_height = client_rect.bottom - client_rect.top;
                    int button_y = window_height - 30;
                    int button_spacing = 50;
                    int center_x = window_width / 2;
                    int art_size = (80 < (window_width - 30) ? 80 : (window_width - 30));
                    art_size = (art_size < (window_height - 30) ? art_size : (window_height - 30));
                    
                    // Check if click is in button areas - don't allow dragging here
                    if (pt.y >= button_y - 10 && pt.y <= button_y + 10) { // Button row
                        if ((pt.x >= center_x - button_spacing - 10 && pt.x < center_x - button_spacing + 10) ||    // Previous
                            (pt.x >= center_x - 10 && pt.x < center_x + 10) ||                                      // Play/Pause
                            (pt.x >= center_x + button_spacing - 10 && pt.x < center_x + button_spacing + 10)) {    // Next
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
                    
                    // Repaint to adjust artwork display
                    InvalidateRect(hwnd, nullptr, TRUE);
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
                        InvalidateRect(hwnd, nullptr, TRUE);
                    } else {
                        // Calculate fade progress (100 to 0 over 1 second)
                        panel->m_overlay_opacity = 100 - (int)((elapsed * 100) / fade_duration);
                        InvalidateRect(hwnd, nullptr, TRUE);
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
                        InvalidateRect(hwnd, nullptr, TRUE);
                    } else {
                        // Calculate fade progress (100 to 0 over 1 second)
                        panel->m_undocked_overlay_opacity = 100 - (int)((elapsed * 100) / fade_duration);
                        InvalidateRect(hwnd, nullptr, TRUE);
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
    int button_spacing = 50;
    int center_x = window_width / 2;
    
    struct {
        int x, y;
        int button_type; // 0=previous, 1=play/pause, 2=next
    } buttons[] = {
        {center_x - button_spacing, button_y, 0},    // Previous
        {center_x, button_y, 1},                     // Play/Pause
        {center_x + button_spacing, button_y, 2}     // Next
    };
    
    // Enable anti-aliasing for smoother drawing
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, nullptr);
    
    int icon_size = 16; // Size of the icons
    
    for (auto& btn : buttons) {
        switch (btn.button_type) {
        case 0: // Previous
            if (m_is_undocked && !m_is_artwork_expanded) {
                draw_previous_icon_with_opacity(hdc, btn.x, btn.y, icon_size, m_button_opacity);
            } else {
                draw_previous_icon(hdc, btn.x, btn.y, icon_size);
            }
            break;
        case 1: // Play/Pause
            if (m_is_undocked && !m_is_artwork_expanded) {
                if (m_is_playing && !m_is_paused) {
                    draw_pause_icon_with_opacity(hdc, btn.x, btn.y, icon_size, m_button_opacity);
                } else {
                    draw_play_icon_with_opacity(hdc, btn.x, btn.y, icon_size, m_button_opacity);
                }
            } else {
                if (m_is_playing && !m_is_paused) {
                    draw_pause_icon(hdc, btn.x, btn.y, icon_size);
                } else {
                    draw_play_icon(hdc, btn.x, btn.y, icon_size);
                }
            }
            break;
        case 2: // Next
            if (m_is_undocked && !m_is_artwork_expanded) {
                draw_next_icon_with_opacity(hdc, btn.x, btn.y, icon_size, m_button_opacity);
            } else {
                draw_next_icon(hdc, btn.x, btn.y, icon_size);
            }
            break;
        }
    }
    
    
}

void control_panel::paint_artwork_expanded(HDC hdc, const RECT& client_rect) {
    // Calculate artwork display area
    int window_width = client_rect.right - client_rect.left;
    int window_height = client_rect.bottom - client_rect.top;
    
    if (m_cover_art_bitmap_original && m_original_art_width > 0 && m_original_art_height > 0) {
        // Use original resolution bitmap for expanded view
        HDC cover_dc = CreateCompatibleDC(hdc);
        HBITMAP old_bitmap = (HBITMAP)SelectObject(cover_dc, m_cover_art_bitmap_original);
        
        // Set high-quality stretching mode
        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, nullptr);
        
        // Since window maintains aspect ratio, artwork should fill entire window
        // Draw the artwork to fill the entire client area (no black bars)
        StretchBlt(hdc, 0, 0, window_width, window_height,
                   cover_dc, 0, 0, m_original_art_width, m_original_art_height, SRCCOPY);
        
        SelectObject(cover_dc, old_bitmap);
        DeleteDC(cover_dc);
        
        // Draw track info overlay if hovering
        if (m_overlay_visible) {
            draw_track_info_overlay(hdc, window_width, window_height);
            draw_control_overlay(hdc, window_width, window_height);
        }
        
    } else {
        // Draw placeholder for no artwork
        RECT artwork_rect = {0, 0, window_width, window_height};
        HBRUSH placeholder_brush = CreateSolidBrush(RGB(60, 60, 60));
        FillRect(hdc, &artwork_rect, placeholder_brush);
        DeleteObject(placeholder_brush);
        
    }
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
    if (m_cover_art_bitmap) {
        RECT art_rect = {margin, margin, margin + art_size, margin + art_size};
        
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
    }
    
    // Draw artwork hover arrows if hovering over artwork (same as undocked mode)
    if (m_undocked_overlay_visible) {
        draw_undocked_artwork_overlay(hdc, window_width, window_height);
    }
    
    // Set text properties
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    
    // Calculate text area  
    int text_left = margin + art_size + margin;
    int text_right = window_width - margin; // No dots, use full width
    
    // Draw song title (top half of remaining space) - reduced by 25%
    HFONT title_font = CreateFont(29, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT old_font = (HFONT)SelectObject(hdc, title_font);
    
    RECT title_rect = {text_left, margin - (int)(window_height * 0.10), text_right, window_height / 2 + margin - (int)(window_height * 0.10)};
    pfc::stringcvt::string_wide_from_utf8 wide_title(m_current_title.c_str());
    DrawText(hdc, wide_title.get_ptr(), -1, &title_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    
    // Draw artist name (bottom half of remaining space) - doubled in size
    HFONT artist_font = CreateFont(27, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
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
    
    // Draw time remaining
    double time_remaining = m_track_length - m_current_time;
    if (time_remaining < 0) time_remaining = 0;
    
    int remaining_min = (int)(time_remaining / 60);
    int remaining_sec = (int)time_remaining % 60;
    
    wchar_t time_remaining_str[16];
    swprintf_s(time_remaining_str, 16, L"-%d:%02d", remaining_min, remaining_sec);
    
    HFONT time_font = CreateFont(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, time_font);
    SetTextColor(hdc, RGB(160, 160, 160));
    
    RECT time_rect = {progress_bar_left + progress_bar_width + 5, progress_bar_y - 8, window_width - margin, progress_bar_y + progress_bar_height + 8};
    DrawText(hdc, time_remaining_str, -1, &time_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    
    // Draw compact control overlay if hovering over text area
    draw_compact_control_overlay(hdc, window_width, window_height);
    
    // Cleanup fonts
    SelectObject(hdc, old_font);
    DeleteObject(title_font);
    DeleteObject(artist_font);
    DeleteObject(time_font);
}

void control_panel::draw_time_info(HDC hdc, const RECT& client_rect) {
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
    if (m_current_title.is_empty() && m_current_artist.is_empty()) return;
    
    // Create semi-transparent overlay background at the top
    int overlay_height = 46; // Height of the overlay area
    RECT overlay_rect = {0, 0, window_width, overlay_height};
    
    // Create solid overlay with consistent grey color - simulate fading by skipping frames
    const int grey_color = 40; // Consistent grey color
    
    // Create a solid overlay with opacity-adjusted color to avoid flickering
    if (m_overlay_opacity > 0) {
        // Scale the grey color based on opacity (darker = more opaque)
        int opacity_adjusted_color = (grey_color * m_overlay_opacity) / 100;
        RECT full_overlay = {0, 0, window_width, overlay_height};
        HBRUSH overlay_brush = CreateSolidBrush(RGB(opacity_adjusted_color, opacity_adjusted_color, opacity_adjusted_color));
        FillRect(hdc, &full_overlay, overlay_brush);
        DeleteObject(overlay_brush);
    }
    
    // Draw track info text on top of the overlay (only when overlay has opacity)
    if (m_overlay_opacity > 0) {
        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, TRANSPARENT);
    
    // Track title (larger and bold, top line)
    HFONT title_font = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT old_font = (HFONT)SelectObject(hdc, title_font);
    
    // Position title with slightly more spacing (2% increase)
    int title_y = (overlay_height / 2) - 15;  // Center minus spacing for both texts
    RECT title_rect = {15, title_y, window_width - 15, title_y + 20};
    if (!m_current_title.is_empty()) {
        pfc::stringcvt::string_wide_from_utf8 wide_title(m_current_title.c_str());
        DrawText(hdc, wide_title.get_ptr(), -1, &title_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        DrawText(hdc, L"[No Track Title]", -1, &title_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    
    SelectObject(hdc, old_font);
    DeleteObject(title_font);
    
    // Artist name (regular, bottom line)  
    HFONT artist_font = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    old_font = (HFONT)SelectObject(hdc, artist_font);
    
    // Position artist with slightly more spacing from title (2% increase)
    int artist_y = (overlay_height / 2) + 3;  // Center plus slightly more spacing from title
    RECT artist_rect = {15, artist_y, window_width - 15, artist_y + 16};
    if (!m_current_artist.is_empty()) {
        pfc::stringcvt::string_wide_from_utf8 wide_artist(m_current_artist.c_str());
        DrawText(hdc, wide_artist.get_ptr(), -1, &artist_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        DrawText(hdc, L"[No Artist]", -1, &artist_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    
        SelectObject(hdc, old_font);
        DeleteObject(artist_font);
    } // End of should_draw_overlay condition
}

void control_panel::draw_control_overlay(HDC hdc, int window_width, int window_height) {
    // Create bottom overlay with same opacity behavior as top overlay
    const int overlay_height = 46; // Height for control buttons (reduced by another 5%)
    const int grey_color = 40; // Same color as top overlay
    
    // Create bottom overlay with opacity-adjusted color to avoid flickering
    if (m_overlay_opacity > 0) {
        // Scale the grey color based on opacity (darker = more opaque)
        int opacity_adjusted_color = (grey_color * m_overlay_opacity) / 100;
        RECT bottom_overlay = {0, window_height - overlay_height, window_width, window_height};
        HBRUSH overlay_brush = CreateSolidBrush(RGB(opacity_adjusted_color, opacity_adjusted_color, opacity_adjusted_color));
        FillRect(hdc, &bottom_overlay, overlay_brush);
        DeleteObject(overlay_brush);
        
        // Draw control buttons (Previous, Play/Pause, Next)
        int button_size = 24; // Size of each button
        int button_spacing = 60; // Space between button centers
        int center_x = window_width / 2;
        // Calculate center of the bottom overlay area, then lower by 20%
        int overlay_top = window_height - overlay_height;
        int center_y = overlay_top + (overlay_height / 2) + (overlay_height / 5);
        
        // Previous button
        int prev_x = center_x - button_spacing;
        int prev_y = center_y;
        if (m_is_undocked && !m_is_artwork_expanded) {
            draw_previous_icon_with_opacity(hdc, prev_x - button_size/2, prev_y - button_size/2, button_size, m_button_opacity);
        } else {
            draw_previous_icon(hdc, prev_x - button_size/2, prev_y - button_size/2, button_size);
        }
        
        // Play/Pause button
        int play_x = center_x;
        int play_y = center_y;
        if (m_is_undocked && !m_is_artwork_expanded) {
            if (m_is_playing && !m_is_paused) {
                draw_pause_icon_with_opacity(hdc, play_x - button_size/2, play_y - button_size/2, button_size, m_button_opacity);
            } else {
                draw_play_icon_with_opacity(hdc, play_x - button_size/2, play_y - button_size/2, button_size, m_button_opacity);
            }
        } else {
            if (m_is_playing && !m_is_paused) {
                draw_pause_icon(hdc, play_x - button_size/2, play_y - button_size/2, button_size);
            } else {
                draw_play_icon(hdc, play_x - button_size/2, play_y - button_size/2, button_size);
            }
        }
        
        // Next button
        int next_x = center_x + button_spacing;
        int next_y = center_y;
        if (m_is_undocked && !m_is_artwork_expanded) {
            draw_next_icon_with_opacity(hdc, next_x - button_size/2, next_y - button_size/2, button_size, m_button_opacity);
        } else {
            draw_next_icon(hdc, next_x - button_size/2, next_y - button_size/2, button_size);
        }
        
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
    
    // Create control button layout in the text area
    int button_size = 24; // Increased by 20% from 20 to 24
    int button_spacing = 20; // Much more spacing between buttons
    int total_buttons_width = (3 * button_size) + (2 * button_spacing);
    
    // Center the buttons horizontally in the text area
    int buttons_start_x = text_left + (text_area_width - total_buttons_width) / 2;
    
    // Position buttons slightly below center
    int button_y = margin + (window_height - 2 * margin - button_size) / 2 + 5;
    
    // Calculate button positions
    int prev_x = buttons_start_x + button_size / 2;
    int play_x = prev_x + button_size + button_spacing;
    int next_x = play_x + button_size + button_spacing;
    
    // Create background overlay to completely hide the text (don't cover progress bar)
    RECT overlay_rect = {text_left, 0, text_right, overlay_bottom};
    HBRUSH overlay_brush = CreateSolidBrush(RGB(28, 28, 28)); // Solid background to completely hide text
    FillRect(hdc, &overlay_rect, overlay_brush);
    DeleteObject(overlay_brush);
    
    // Draw control buttons
    // Previous button
    draw_previous_icon(hdc, prev_x - button_size/2, button_y, button_size);
    
    // Play/Pause button
    if (m_is_playing && !m_is_paused) {
        draw_pause_icon(hdc, play_x - button_size/2, button_y, button_size);
    } else {
        draw_play_icon(hdc, play_x - button_size/2, button_y, button_size);
    }
    
    // Next button
    draw_next_icon(hdc, next_x - button_size/2, button_y, button_size);
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
