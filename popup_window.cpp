#include "stdafx.h"
#include "popup_window.h"
#include "preferences.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// Static instance
popup_window* popup_window::s_instance = nullptr;

// External declaration from main.cpp
extern HINSTANCE g_hIns;

//=============================================================================
// popup_window - Singleton popup notification window
//=============================================================================

popup_window::popup_window() 
    : m_popup_window(nullptr)
    , m_cover_art_window(nullptr)
    , m_artist_label(nullptr)
    , m_title_label(nullptr)
    , m_initialized(false)
    , m_visible(false)
    , m_animating(false)
    , m_sliding_in(false)
    , m_animation_step(0)
    , m_final_x(0), m_final_y(0)
    , m_start_x(0), m_start_y(0)
    , m_cover_art_bitmap(nullptr) {
}

popup_window::~popup_window() {
    cleanup();
}

popup_window& popup_window::get_instance() {
    if (!s_instance) {
        s_instance = new popup_window();
    }
    return *s_instance;
}

void popup_window::initialize() {
    if (m_initialized) return;
    
    create_popup_window();
    m_initialized = true;
}

void popup_window::cleanup() {
    if (m_visible) {
        hide_popup();
    }
    
    // Kill any active timers
    if (m_popup_window) {
        KillTimer(m_popup_window, POPUP_TIMER_ID);
        KillTimer(m_popup_window, ANIMATION_TIMER_ID);
    }
    
    cleanup_cover_art();
    
    if (m_popup_window) {
        DestroyWindow(m_popup_window);
        m_popup_window = nullptr;
    }
    
    m_initialized = false;
    m_animating = false;
}

void popup_window::show_track_info(metadb_handle_ptr p_track) {
    if (!m_initialized || !get_show_popup_notification() || !p_track.is_valid()) {
        return;
    }
    
    // For track change detection, use path for local files and metadata for streams
    pfc::string8 current_path = p_track->get_path();
    bool is_stream = strstr(current_path.get_ptr(), "://") != nullptr;
    
    pfc::string8 track_identifier;
    if (is_stream) {
        // For streams, use artist|title as identifier since path doesn't change
        try {
            auto playback = playback_control::get();
            static_api_ptr_t<titleformat_compiler> compiler;
            service_ptr_t<titleformat_object> script;
            
            if (compiler->compile(script, "[%artist%]|[%title%]")) {
                pfc::string8 formatted_title;
                if (playback->playback_format_title(nullptr, formatted_title, script, nullptr, playback_control::display_level_all)) {
                    track_identifier = formatted_title;
                }
            }
        } catch (...) {
            // Fallback to path if titleformat fails
            track_identifier = current_path;
        }
    } else {
        // For local files, use path as identifier
        track_identifier = current_path;
    }
    
    // Check if this is the same track/metadata to prevent duplicate popups
    if (track_identifier == m_last_track_path && !track_identifier.is_empty()) {
        return; // Same track/metadata, don't show popup again
    }
    
    m_last_track_path = track_identifier;
    
    // Update track info and cover art
    update_track_info(p_track);
    load_cover_art(p_track);
    
    // Calculate final position
    position_popup();
    
    // Start slide-in animation
    start_slide_in_animation();
}

void popup_window::hide_popup() {
    if (!m_visible || m_animating) return;
    
    KillTimer(m_popup_window, POPUP_TIMER_ID);
    start_slide_out_animation();
}

void popup_window::refresh_track_info() {
    if (!m_initialized || !m_visible || !m_popup_window) return;
    
    // Force repaint to update displayed info with current metadata
    InvalidateRect(m_popup_window, nullptr, TRUE);
    UpdateWindow(m_popup_window);
}

void popup_window::on_settings_changed() {
    if (!get_show_popup_notification() && m_visible) {
        hide_popup();
    }
    
    // Update window corner preference
    if (m_popup_window) {
        DWORD corner_pref = get_use_rounded_corners() ? 2 : 1;
        DwmSetWindowAttribute(m_popup_window, 33, &corner_pref, sizeof(corner_pref));
    }
    
    // If popup is currently visible, update its position
    if (m_visible && !m_animating) {
        position_popup();
        SetWindowPos(m_popup_window, HWND_TOPMOST, m_final_x, m_final_y, 320, 80, SWP_NOACTIVATE);
    }
}

void popup_window::create_popup_window() {
    const char* class_name = "TrayControlsPopupWindow";
    
    // Register window class (only once)
    static bool class_registered = false;
    if (!class_registered) {
        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = popup_window_proc;
        wc.hInstance = g_hIns;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(45, 45, 48)); // Dark background
        wc.lpszClassName = L"TrayControlsPopupWindow";
        
        ATOM class_atom = RegisterClassEx(&wc);
        if (class_atom != 0) {
            class_registered = true;
        }
    }
    
    // Create popup window (initially hidden)
    m_popup_window = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        L"TrayControlsPopupWindow",
        L"Track Info",
        WS_POPUP,
        0, 0, 320, 80, // Initial size, will be repositioned
        nullptr,
        nullptr,
        g_hIns,
        this
    );
    
    if (!m_popup_window) {
        throw exception_win32(GetLastError());
    }
    
    // Apply window corner preference (rounded/square corners)
    // DWMWA_WINDOW_CORNER_PREFERENCE = 33
    DWORD corner_pref = get_use_rounded_corners() ? 2 : 1;
    DwmSetWindowAttribute(m_popup_window, 33, &corner_pref, sizeof(corner_pref));
}

void popup_window::position_popup() {
    if (!m_popup_window) return;
    
    // Get screen dimensions
    int screen_width = GetSystemMetrics(SM_CXSCREEN);
    int screen_height = GetSystemMetrics(SM_CYSCREEN);
    
    // Get taskbar info to avoid overlapping
    APPBARDATA abd = {};
    abd.cbSize = sizeof(APPBARDATA);
    SHAppBarMessage(ABM_GETTASKBARPOS, &abd);
    
    // Calculate popup dimensions
    const int popup_width = 320;
    const int popup_height = 80;
    const int margin = 10;
    const int top_margin = 96; // About an inch down (96 pixels ≈ 1 inch at 96 DPI)
    
    // Position based on user preference
    int x;
    int y;
    int popup_position = get_popup_position();
    bool is_right_side = (popup_position >= 3); // 3, 4, 5 are right-side positions

    // Set X position based on left/right side
    if (is_right_side) {
        x = screen_width - popup_width - margin;
    } else {
        x = margin;
    }

    // Set Y position based on top/middle/bottom (indices 0,3=top, 1,4=middle, 2,5=bottom)
    int vertical_position = popup_position % 3;
    switch (vertical_position) {
    case 0: // Top
        y = top_margin;
        break;
    case 1: // Middle
        y = (screen_height - popup_height) / 2;
        break;
    case 2: // Bottom
        y = screen_height - popup_height - margin;
        break;
    default:
        y = top_margin;
        break;
    }
    
    // Adjust for taskbar position
    if (abd.rc.top == 0 && abd.rc.left == 0 && abd.rc.right == screen_width) {
        // Taskbar is at top
        if (vertical_position == 0) { // Only adjust top position
            y = abd.rc.bottom + margin;
        }
    } else if (abd.rc.left == 0 && abd.rc.top == 0 && abd.rc.bottom == screen_height) {
        // Taskbar is at left - only affects left-side popups
        if (!is_right_side) {
            x = abd.rc.right + margin;
        }
    } else if (abd.rc.top == screen_height - abd.rc.bottom && abd.rc.left == 0 && abd.rc.right == screen_width) {
        // Taskbar is at bottom
        if (vertical_position == 2) { // Only adjust bottom position
            y = abd.rc.top - popup_height - margin;
        }
    } else if (abd.rc.right == screen_width && abd.rc.top == 0 && abd.rc.bottom == screen_height && abd.rc.left > 0) {
        // Taskbar is at right - only affects right-side popups
        if (is_right_side) {
            x = abd.rc.left - popup_width - margin;
        }
    }
    
    // Store final position for animation
    m_final_x = x;
    m_final_y = y;
}

void popup_window::update_track_info(metadb_handle_ptr p_track) {
    if (!p_track.is_valid()) return;
    
    // Store track path for comparison
    m_last_track_path = p_track->get_path();
    
    // Store track handle for use during painting
    m_current_track = p_track;
    
    // Force repaint to update displayed info
    if (m_popup_window) {
        InvalidateRect(m_popup_window, nullptr, TRUE);
        UpdateWindow(m_popup_window);
    }
}

void popup_window::load_cover_art(metadb_handle_ptr p_track) {
    if (!p_track.is_valid()) return;
    
    // Clean up previous cover art
    cleanup_cover_art();
    
    try {
        // Use foobar2000's album art API - this will automatically check for:
        // 1. Embedded album art in the music file
        // 2. External cover art files (cover.jpg, folder.jpg, etc.) in the same directory
        auto api = album_art_manager_v2::get();
        if (!api.is_valid()) return;
        
        // Extract album art (front cover)
        auto extractor = api->open(pfc::list_single_ref_t<metadb_handle_ptr>(p_track), 
                                   pfc::list_single_ref_t<GUID>(album_art_ids::cover_front), 
                                   fb2k::noAbort);
        
        if (extractor.is_valid()) {
            auto data = extractor->query(album_art_ids::cover_front, fb2k::noAbort);
            if (data.is_valid() && data->get_size() > 0) {
                // Convert album art data to HBITMAP
                m_cover_art_bitmap = convert_album_art_to_bitmap(data);
            }
        }
    } catch (...) {
        // Ignore album art errors - popup will work without cover art
    }
}

void popup_window::cleanup_cover_art() {
    if (m_cover_art_bitmap) {
        DeleteObject(m_cover_art_bitmap);
        m_cover_art_bitmap = nullptr;
    }
}

HBITMAP popup_window::convert_album_art_to_bitmap(album_art_data_ptr art_data) {
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
        
        // Create a bitmap with the desired size (60x60 for the popup cover area)
        const int target_size = 60;
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
        
        // Clear background to dark gray to match popup
        graphics.Clear(Gdiplus::Color(255, 40, 40, 40));
        
        // Draw the scaled image
        graphics.DrawImage(&image, offset_x, offset_y, draw_width, draw_height);
        
        // Convert to HBITMAP
        if (bitmap.GetHBITMAP(Gdiplus::Color(40, 40, 40), &result) != Gdiplus::Ok) {
            result = nullptr;
        }
        
    } catch (...) {
        result = nullptr;
    }
    
    return result;
}

LRESULT CALLBACK popup_window::popup_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    popup_window* popup = nullptr;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
        popup = reinterpret_cast<popup_window*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(popup));
    } else {
        popup = reinterpret_cast<popup_window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (popup) {
        switch (msg) {
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                popup->paint_popup(hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }
            
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
            // Hide popup on any click
            popup->hide_popup();
            return 0;
            
        case WM_TIMER:
            if (wparam == POPUP_TIMER_ID) {
                popup->hide_popup();
                return 0;
            } else if (wparam == ANIMATION_TIMER_ID) {
                popup->update_animation();
                return 0;
            }
            break;
        }
    }
    
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

VOID CALLBACK popup_window::hide_timer_proc(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time) {
    if (timer_id == POPUP_TIMER_ID && s_instance) {
        s_instance->hide_popup();
    }
}

VOID CALLBACK popup_window::animation_timer_proc(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time) {
    if (timer_id == ANIMATION_TIMER_ID && s_instance) {
        s_instance->update_animation();
    }
}

void popup_window::paint_popup(HDC hdc) {
    
    if (!hdc) return;
    
    RECT client_rect;
    GetClientRect(m_popup_window, &client_rect);
    
    // Set background
    HBRUSH bg_brush = CreateSolidBrush(RGB(45, 45, 48));
    FillRect(hdc, &client_rect, bg_brush);
    DeleteObject(bg_brush);
    
    // Draw border
    HPEN border_pen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
    HPEN old_pen = (HPEN)SelectObject(hdc, border_pen);
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    
    Rectangle(hdc, 0, 0, client_rect.right, client_rect.bottom);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(border_pen);
    
    // Draw cover art (left side)
    RECT cover_rect = {10, 10, 70, 70};
    
    if (m_cover_art_bitmap) {
        // Draw the loaded cover art
        HDC bitmap_dc = CreateCompatibleDC(hdc);
        HBITMAP old_bitmap = (HBITMAP)SelectObject(bitmap_dc, m_cover_art_bitmap);
        
        // Draw the bitmap to fit the cover rect (60x60)
        BitBlt(hdc, cover_rect.left, cover_rect.top, 60, 60, bitmap_dc, 0, 0, SRCCOPY);
        
        SelectObject(bitmap_dc, old_bitmap);
        DeleteDC(bitmap_dc);
    } else {
        // Draw placeholder if no cover art
        HBRUSH cover_brush = CreateSolidBrush(RGB(80, 80, 80));
        FillRect(hdc, &cover_rect, cover_brush);
        DeleteObject(cover_brush);
        
        // Determine icon based on whether this is a stream or local file
        bool is_stream = false;
        if (m_current_track.is_valid()) {
            pfc::string8 path = m_current_track->get_path();
            is_stream = strstr(path.get_ptr(), "://") != nullptr;
        }
        
        if (is_stream) {
            // Load and draw radio icon for internet streams
            // Try LoadImage first for better flexibility with icon sizes
            HICON radio_icon = (HICON)LoadImage(g_hIns, MAKEINTRESOURCE(IDI_RADIO_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
            
            // If LoadImage fails, try LoadIcon as fallback
            if (!radio_icon) {
                radio_icon = LoadIcon(g_hIns, MAKEINTRESOURCE(IDI_RADIO_ICON));
            }
            
            if (radio_icon) {
                // Center the icon in the cover rect
                int icon_size = 32; // Standard small icon size
                int icon_x = cover_rect.left + (60 - icon_size) / 2;
                int icon_y = cover_rect.top + (60 - icon_size) / 2;
                
                DrawIconEx(hdc, icon_x, icon_y, radio_icon, icon_size, icon_size, 0, nullptr, DI_NORMAL);
                DestroyIcon(radio_icon); // Clean up the icon handle
            } else {
                // Fallback to text if icon can't be loaded
                SetTextColor(hdc, RGB(200, 200, 200));
                SetBkMode(hdc, TRANSPARENT);
                HFONT symbol_font = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
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
            HFONT symbol_font = CreateFont(24, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                           DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
            HFONT old_symbol_font = (HFONT)SelectObject(hdc, symbol_font);
            
            DrawText(hdc, L"♪", -1, &cover_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            SelectObject(hdc, old_symbol_font);
            DeleteObject(symbol_font);
        }
    }
    
    // Draw track info (right side)
    draw_track_info(hdc, client_rect);
}

void popup_window::draw_track_info(HDC hdc, const RECT& client_rect) {
    
    if (!hdc) return;
    
    // Get track info from stored track or current playing track
    pfc::string8 artist = "Unknown Artist";
    pfc::string8 title = "Unknown Title";
    
    try {
        metadb_handle_ptr track = m_current_track;
        
        // If no stored track, get current playing track
        if (!track.is_valid()) {
            auto playback = playback_control::get();
            if (playback->is_playing()) {
                playback->get_now_playing(track);
            }
        }
        
        if (track.is_valid()) {
            // Check if this is a stream
            pfc::string8 path = track->get_path();
            bool is_stream = strstr(path.get_ptr(), "://") != nullptr;
            
            
            if (is_stream) {
                // For streaming sources, use titleformat to get what foobar2000 displays
                try {
                    auto playback = playback_control::get();
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
                                    artist = tf_artist;
                                    title = tf_title;
                                }
                            }
                        }
                    }
                } catch (...) {
                    // Fall through to basic metadata extraction
                }
            }
            
            // If titleformat didn't work or not a stream, try basic metadata
            if ((artist == "Unknown Artist" || title == "Unknown Title")) {
                file_info_impl info;
                if (track->get_info(info)) {
                    const char* artist_str = info.meta_get("ARTIST", 0);
                    const char* title_str = info.meta_get("TITLE", 0);
                    
                    if (artist_str && *artist_str) artist = artist_str;
                    if (title_str && *title_str) title = title_str;
                    
                    // For streams, try additional fallbacks
                    if (is_stream) {
                        if (title == "Unknown Title" && info.meta_exists("server")) {
                            title = info.meta_get("server", 0);
                        }
                        if (title == "Unknown Title" && info.meta_exists("SERVER")) {
                            title = info.meta_get("SERVER", 0);
                        }
                    }
                }
            }
        }
    } catch (...) {
        // Use default values on error
    }
    
    // Setup text drawing
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    
    // Use custom fonts if available, otherwise fallback to defaults
    HFONT artist_font, title_font;
    
    if (get_use_custom_fonts()) {
        LOGFONT artist_lf = get_artist_font();
        LOGFONT title_lf = get_track_font();
        
        // Scale fonts down slightly for popup (popup is smaller than control panel)
        artist_lf.lfHeight = (artist_lf.lfHeight * 3) / 4;  // 75% of original size
        title_lf.lfHeight = (title_lf.lfHeight * 3) / 4;    // 75% of original size
        
        artist_font = CreateFontIndirect(&artist_lf);
        title_font = CreateFontIndirect(&title_lf);
    } else {
        // Default fonts - title should be larger and bold, artist regular weight
        title_font = CreateFont(21, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
        artist_font = CreateFont(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    }
    
    // Draw title first (top line)
    HFONT old_font = (HFONT)SelectObject(hdc, title_font);
    
    RECT title_rect = {85, 15, client_rect.right - 10, 35};
    pfc::stringcvt::string_wide_from_utf8 wide_title(title.c_str());
    DrawText(hdc, wide_title.get_ptr(), -1, &title_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    
    // Draw artist second (bottom line)
    SelectObject(hdc, artist_font);
    
    RECT artist_rect = {85, 40, client_rect.right - 10, 60};
    pfc::stringcvt::string_wide_from_utf8 wide_artist(artist.c_str());
    DrawText(hdc, wide_artist.get_ptr(), -1, &artist_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    
    // Cleanup fonts
    SelectObject(hdc, old_font);
    DeleteObject(artist_font);
    DeleteObject(title_font);
}

void popup_window::start_slide_in_animation() {
    if (m_animating) return;

    // Determine if sliding from right or left based on popup position
    int popup_position = get_popup_position();
    bool is_right_side = (popup_position >= 3);

    // Get screen width for right-side calculations
    int screen_width = GetSystemMetrics(SM_CXSCREEN);

    // Calculate start position (off-screen on the appropriate side)
    if (is_right_side) {
        m_start_x = screen_width; // Start off-screen to the right
    } else {
        m_start_x = -320; // Start off-screen to the left (negative width)
    }
    m_start_y = m_final_y;

    // Set initial position
    SetWindowPos(m_popup_window, HWND_TOPMOST, m_start_x, m_start_y, 320, 80, SWP_NOACTIVATE);

    // Show window and start animation
    ShowWindow(m_popup_window, SW_SHOWNOACTIVATE);

    m_animating = true;
    m_sliding_in = true;
    m_animation_step = 0;
    m_visible = true;

    // Start animation timer
    SetTimer(m_popup_window, ANIMATION_TIMER_ID, ANIMATION_DURATION / ANIMATION_STEPS, animation_timer_proc);
}

void popup_window::start_slide_out_animation() {
    if (m_animating) return;

    // Determine if sliding to right or left based on popup position
    int popup_position = get_popup_position();
    bool is_right_side = (popup_position >= 3);

    // Get screen width for right-side calculations
    int screen_width = GetSystemMetrics(SM_CXSCREEN);

    // Get current position as start position
    RECT window_rect;
    GetWindowRect(m_popup_window, &window_rect);
    m_start_x = window_rect.left;
    m_start_y = window_rect.top;

    // Set final position (off-screen on the appropriate side)
    if (is_right_side) {
        m_final_x = screen_width; // Exit off-screen to the right
    } else {
        m_final_x = -320; // Exit off-screen to the left
    }
    m_final_y = m_start_y;

    m_animating = true;
    m_sliding_in = false;
    m_animation_step = 0;

    // Start animation timer
    SetTimer(m_popup_window, ANIMATION_TIMER_ID, ANIMATION_DURATION / ANIMATION_STEPS, animation_timer_proc);
}

void popup_window::update_animation() {
    if (!m_animating) return;
    
    m_animation_step++;
    
    // Calculate current position using eased interpolation
    float progress = (float)m_animation_step / (float)ANIMATION_STEPS;
    
    // Apply easing (ease-out for smooth deceleration)
    progress = 1.0f - (1.0f - progress) * (1.0f - progress);
    
    int current_x = m_start_x + (int)((m_final_x - m_start_x) * progress);
    int current_y = m_start_y + (int)((m_final_y - m_start_y) * progress);
    
    // Update window position
    SetWindowPos(m_popup_window, HWND_TOPMOST, current_x, current_y, 320, 80, SWP_NOACTIVATE);
    
    // Check if animation is complete
    if (m_animation_step >= ANIMATION_STEPS) {
        KillTimer(m_popup_window, ANIMATION_TIMER_ID);
        m_animating = false;
        
        if (m_sliding_in) {
            // Animation complete, start auto-hide timer using configurable duration
            SetTimer(m_popup_window, POPUP_TIMER_ID, get_popup_duration(), hide_timer_proc);
        } else {
            // Slide-out complete, hide window
            ShowWindow(m_popup_window, SW_HIDE);
            m_visible = false;
        }
    }
}
