#include "stdafx.h"
#include "popup_window.h"
#include "preferences.h"

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

void popup_window::on_settings_changed() {
    if (!get_show_popup_notification() && m_visible) {
        hide_popup();
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
    
    // Position at top-left corner
    int x = margin;
    int y = top_margin;
    
    // Adjust for taskbar position (only if taskbar is at top or left)
    if (abd.rc.top == 0 && abd.rc.left == 0 && abd.rc.right == screen_width) {
        // Taskbar is at top
        y = abd.rc.bottom + margin;
    } else if (abd.rc.left == 0 && abd.rc.top == 0 && abd.rc.bottom == screen_height) {
        // Taskbar is at left
        x = abd.rc.right + margin;
    }
    
    // Store final position for animation
    m_final_x = x;
    m_final_y = y;
}

void popup_window::update_track_info(metadb_handle_ptr p_track) {
    if (!p_track.is_valid()) return;
    
    // Store track path for comparison
    m_last_track_path = p_track->get_path();
    
    // Force repaint to update displayed info
    InvalidateRect(m_popup_window, nullptr, TRUE);
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
        
        // Add "♪" symbol as placeholder for cover art
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
    
    // Draw track info (right side)
    draw_track_info(hdc, client_rect);
}

void popup_window::draw_track_info(HDC hdc, const RECT& client_rect) {
    if (!hdc) return;
    
    // Get current track info
    pfc::string8 artist = "Unknown Artist";
    pfc::string8 title = "Unknown Title";
    
    try {
        auto playback = playback_control::get();
        if (playback->is_playing()) {
            metadb_handle_ptr track;
            if (playback->get_now_playing(track) && track.is_valid()) {
                file_info_impl info;
                if (track->get_info(info)) {
                    const char* artist_str = info.meta_get("ARTIST", 0);
                    const char* title_str = info.meta_get("TITLE", 0);
                    
                    if (artist_str && *artist_str) artist = artist_str;
                    if (title_str && *title_str) title = title_str;
                }
            }
        }
    } catch (...) {
        // Use default values on error
    }
    
    // Setup text drawing
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    
    // Draw artist (larger font)
    HFONT artist_font = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT old_font = (HFONT)SelectObject(hdc, artist_font);
    
    RECT artist_rect = {85, 15, client_rect.right - 10, 35};
    pfc::stringcvt::string_wide_from_utf8 wide_artist(artist.c_str());
    DrawText(hdc, wide_artist.get_ptr(), -1, &artist_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    
    // Draw title (smaller font)
    HFONT title_font = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, title_font);
    
    RECT title_rect = {85, 40, client_rect.right - 10, 60};
    pfc::stringcvt::string_wide_from_utf8 wide_title(title.c_str());
    DrawText(hdc, wide_title.get_ptr(), -1, &title_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    
    // Cleanup fonts
    SelectObject(hdc, old_font);
    DeleteObject(artist_font);
    DeleteObject(title_font);
}

void popup_window::start_slide_in_animation() {
    if (m_animating) return;
    
    // Calculate start position (off-screen to the left)
    m_start_x = -320; // Start off-screen to the left (negative width)
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
    
    // Get current position as start position
    RECT window_rect;
    GetWindowRect(m_popup_window, &window_rect);
    m_start_x = window_rect.left;
    m_start_y = window_rect.top;
    
    // Set final position (off-screen to the left)
    m_final_x = -320; // Exit off-screen to the left
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
            // Animation complete, start auto-hide timer
            SetTimer(m_popup_window, POPUP_TIMER_ID, POPUP_DISPLAY_TIME, hide_timer_proc);
        } else {
            // Slide-out complete, hide window
            ShowWindow(m_popup_window, SW_HIDE);
            m_visible = false;
        }
    }
}