#include "stdafx.h"
#include "volume_popup.h"
#include "preferences.h"
#include <cmath>
#include <string>

// Convert dB to slider position (0.0 to 1.0) using position = 2^(dB/10).
// Matches Now Bar's ControlPanelCore volume curve.
static inline float db_to_slider(float db) {
    auto position = std::pow(2.0, static_cast<double>(db) / 10.0);
    if (position < 0.0) position = 0.0;
    if (position > 1.0) position = 1.0;
    return static_cast<float>(position);
}

// Convert slider position (0.0 to 1.0) to dB using dB = 10 * log2(position).
// Inverse of db_to_slider.
static inline float slider_to_db(float slider_pos) {
    if (slider_pos <= 0.0f) return -100.0f;
    auto volume = 10.0 * std::log2(static_cast<double>(slider_pos));
    if (volume < -100.0) volume = -100.0;
    if (volume > 0.0) volume = 0.0;
    return static_cast<float>(volume);
}

volume_popup* volume_popup::s_instance = nullptr;
extern HINSTANCE g_hIns;


volume_popup::volume_popup()
    : m_window(nullptr)
    , m_initialized(false)
    , m_visible(false)
    , m_is_dragging(false)
    , m_is_feedback_mode(false)
    , m_current_volume_db(-100.0f) // Start muted or unknown
    , m_hover_level(0)
{
}


volume_popup::~volume_popup() {
    cleanup();
}

volume_popup& volume_popup::get_instance() {
    if (!s_instance) {
        s_instance = new volume_popup();
    }
    return *s_instance;
}

void volume_popup::initialize() {
    if (m_initialized) return;
    register_class();
    create_window();
    m_initialized = true;
}

void volume_popup::cleanup() {
    if (m_window) {
        DestroyWindow(m_window);
        m_window = nullptr;
    }
    m_initialized = false;
}

void volume_popup::register_class() {
    static bool registered = false;
    if (registered) return;

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = g_hIns;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH); // Transparent background for custom shape
    wc.lpszClassName = L"TrayControlsVolumePopup";

    RegisterClassEx(&wc);
    registered = true;
}

void volume_popup::create_window() {
    m_window = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED, // Layered for transparency
        L"TrayControlsVolumePopup",
        L"Volume",
        WS_POPUP,
        0, 0, POPUP_WIDTH, POPUP_HEIGHT,
        nullptr, nullptr, g_hIns, this
    );
    // Initialize layered window with alpha
    SetLayeredWindowAttributes(m_window, 0, 255, LWA_ALPHA);
}

void volume_popup::show_at(int x, int y) {
    if (!m_initialized) initialize();

    m_is_feedback_mode = false;
    KillTimer(m_window, FEEDBACK_TIMER_ID);

    // Get current volume
    try {
        auto playback = playback_control::get();
        m_current_volume_db = playback->get_volume();
    } catch (...) {
        m_current_volume_db = -100.0f;
    }

    // Position window: x, y is the geometric center of the button that invoked this
    // We want to center the arrow tip on x, and have the popup above y
    
    int w = POPUP_WIDTH;
    int h = POPUP_HEIGHT;
    int win_x = x - (w / 2);
    int win_y = y - h - 5; // 5px padding above the button
    
    // Ensure on screen
    int screen_w = GetSystemMetrics(SM_CXSCREEN);

    if (win_x < 0) win_x = 0;
    if (win_x + w > screen_w) win_x = screen_w - w;
    if (win_y < 0) win_y = 0; 
    
    SetWindowPos(m_window, HWND_TOPMOST, win_x, win_y, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    m_visible = true;
    m_is_dragging = false;

    // Track mouse leave to auto-hide
    TRACKMOUSEEVENT tme = {0};
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_window;
    TrackMouseEvent(&tme);

    InvalidateRect(m_window, nullptr, TRUE);
}

void volume_popup::show_feedback() {
    if (!m_initialized) initialize();

    m_is_feedback_mode = true;

    // Get current volume
    try {
        auto playback = playback_control::get();
        m_current_volume_db = playback->get_volume();
    } catch (...) {
        m_current_volume_db = -100.0f;
    }

    int w = FEEDBACK_WIDTH;
    int h = FEEDBACK_HEIGHT;

    // Get cursor position
    POINT pt = {};
    GetCursorPos(&pt);

    int win_x = pt.x - (w / 2);
    int win_y = pt.y - h - 40; // Shifted upward for clearer visibility above taskbar

    // Ensure within primary monitor work area with breathing room above taskbar
    RECT work_area = {};
    SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0);

    if (win_x < work_area.left + 10) win_x = work_area.left + 10;
    if (win_x + w > work_area.right - 10) win_x = work_area.right - 10;
    if (win_y < work_area.top + 10) win_y = work_area.top + 10;
    if (win_y + h > work_area.bottom - 25) win_y = work_area.bottom - h - 25;


    SetWindowPos(m_window, HWND_TOPMOST, win_x, win_y, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    m_visible = true;
    m_is_dragging = false;

    // Reset auto-hide timer (1.5s = 1500ms)
    SetTimer(m_window, FEEDBACK_TIMER_ID, 1500, nullptr);

    InvalidateRect(m_window, nullptr, TRUE);
}

void volume_popup::hide() {
    if (m_visible && m_window) {
        KillTimer(m_window, FEEDBACK_TIMER_ID);
        ShowWindow(m_window, SW_HIDE);
        m_visible = false;
        m_is_dragging = false;
        m_is_feedback_mode = false;
        if (GetCapture() == m_window) ReleaseCapture();
    }
}


void volume_popup::update_volume_from_point(POINT pt) {
    RECT rc;
    GetClientRect(m_window, &rc);
    
    int track_left = SLIDER_MARGIN_X;
    int track_right = rc.right - SLIDER_MARGIN_X;
    int track_width = track_right - track_left;
    
    if (track_width <= 0) return;

    // Map x to volume (left is min, right is max)
    int x = pt.x;
    if (x < track_left) x = track_left;
    if (x > track_right) x = track_right;

    // 0.0 (left) to 1.0 (right)
    float ratio = (float)(x - track_left) / (float)track_width;
    
    float new_vol = slider_to_db(ratio);

    
    // Apply volume
    try {
        auto playback = playback_control::get();
        playback->set_volume(new_vol);
        m_current_volume_db = new_vol;
    } catch (...) {}
    
    InvalidateRect(m_window, nullptr, TRUE);
}

void volume_popup::paint(HDC hdc) {
    RECT rc;
    GetClientRect(m_window, &rc);
    
    // Allow for antialiasing if using GDI+ in future, but standard GDI here
    
    // 1. Draw Bubble Shape (Rounded Rect with Arrow)
    // Background color: White like screenshot
    HBRUSH bg_brush = CreateSolidBrush(RGB(245, 245, 245));
    HPEN bg_pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200)); // Light border
    
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, bg_brush);
    HPEN old_pen = (HPEN)SelectObject(hdc, bg_pen);
    
    int bubble_bottom = rc.bottom - ARROW_HEIGHT;
    
    // Draw rounded rect body
    RoundRect(hdc, 0, 0, rc.right, bubble_bottom, CORNER_RADIUS * 2, CORNER_RADIUS * 2);
    
    // Draw arrow at bottom center (triangle)
    int center_x = rc.right / 2;
    POINT arrow[3] = {
        {center_x - ARROW_WIDTH/2, bubble_bottom - 1}, // Top-left of arrow (overlap body slightly)
        {center_x + ARROW_WIDTH/2, bubble_bottom - 1}, // Top-right of arrow
        {center_x, rc.bottom} // Bottom tip
    };
    
    // To remove the border line between arrow and body, we need to do some polygon magic or fill first then outline.
    // Simpler: Draw filled first, then border.
    // Actually, Windows GDI RoundRect draws both.
    
    // Let's just draw the arrow on top with same brush to merge, then fix outline?
    // GDI is tricky for complex shapes with borders.
    // Alternative: Draw polygon for the whole shape.
    
    // Let's assume the user wants it looking "good enough" for GDI.
    // Draw filled arrow to cover the line
    HPEN null_pen = (HPEN)GetStockObject(NULL_PEN);
    SelectObject(hdc, null_pen);
    Polygon(hdc, arrow, 3);
    
    // Draw arrow outline (just the 'V' part)
    SelectObject(hdc, bg_pen);
    MoveToEx(hdc, center_x - ARROW_WIDTH/2, bubble_bottom, nullptr);
    LineTo(hdc, center_x, rc.bottom);
    LineTo(hdc, center_x + ARROW_WIDTH/2, bubble_bottom);
    
    // Clean up bubble drawing resources
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(bg_brush);
    DeleteObject(bg_pen);

    // 2. Slider Track
    int track_left = SLIDER_MARGIN_X;
    int track_right = rc.right - SLIDER_MARGIN_X;
    int track_y = bubble_bottom / 2; // Vertically centered in the bubble
    int track_height = 4;
    
    RECT track_rect = { track_left, track_y - track_height/2, track_right, track_y + track_height/2 };
    
    HBRUSH track_bg = CreateSolidBrush(RGB(200, 200, 200)); // Light gray track
    FillRect(hdc, &track_rect, track_bg);
    DeleteObject(track_bg);
    
    // 3. Filled Track (Left to current)
    float volume_percent = db_to_slider(m_current_volume_db);

    
    int track_width = track_right - track_left;
    int fill_width = (int)(track_width * volume_percent);
    
    RECT fill_rect = { track_left, track_rect.top, track_left + fill_width, track_rect.bottom };
    
    HBRUSH fill_brush = CreateSolidBrush(RGB(100, 100, 100)); // Darker gray fill
    FillRect(hdc, &fill_rect, fill_brush);
    DeleteObject(fill_brush);
    
    // 4. Thumb
    int thumb_x = track_left + fill_width;
    int thumb_r = THUMB_SIZE / 2;
    
    HBRUSH thumb_brush = CreateSolidBrush(RGB(255, 255, 255));
    HPEN thumb_pen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
    
    old_brush = (HBRUSH)SelectObject(hdc, thumb_brush);
    old_pen = (HPEN)SelectObject(hdc, thumb_pen);
    
    Ellipse(hdc, thumb_x - thumb_r, track_y - thumb_r, thumb_x + thumb_r, track_y + thumb_r);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(thumb_brush);
    DeleteObject(thumb_pen);
}

// Draw volume icon matching Now Bar ControlPanelCore::draw_volume_icon
// Exactly 2 icon states: Mute (<=0.001f) and Speaker (>0.001f)
void volume_popup::draw_speaker_icon(Gdiplus::Graphics &g, int x, int y, int size, float volume_percent, const Gdiplus::Color &color) {
    float scale = static_cast<float>(size) / 24.0f;
    Gdiplus::SolidBrush brush(color);

    // Draw speaker body polygon (common to all) - 6 points from Material Design SVG
    Gdiplus::PointF speaker[6];
    speaker[0] = Gdiplus::PointF(x + 3.0f * scale, y + 8.0f * scale);
    speaker[1] = Gdiplus::PointF(x + 8.0f * scale, y + 8.0f * scale);
    speaker[2] = Gdiplus::PointF(x + 14.0f * scale, y + 2.0f * scale);
    speaker[3] = Gdiplus::PointF(x + 14.0f * scale, y + 22.0f * scale);
    speaker[4] = Gdiplus::PointF(x + 8.0f * scale, y + 16.0f * scale);
    speaker[5] = Gdiplus::PointF(x + 3.0f * scale, y + 16.0f * scale);
    g.FillPolygon(&brush, speaker, 6);

    if (volume_percent <= 0.001f) {
        // Mute icon state: draw Material Design 'X'
        Gdiplus::Pen pen(color, 2.0f * scale);
        g.DrawLine(&pen, x + 16.0f * scale, y + 8.0f * scale, x + 22.0f * scale, y + 16.0f * scale);
        g.DrawLine(&pen, x + 22.0f * scale, y + 8.0f * scale, x + 16.0f * scale, y + 16.0f * scale);
    } else {
        // Speaker icon state: draw double arc sound waves
        Gdiplus::Pen pen(color, 2.0f * scale);
        g.DrawArc(&pen, x + 14.0f * scale, y + 7.0f * scale, 6.0f * scale, 10.0f * scale, -60.0f, 120.0f);
        g.DrawArc(&pen, x + 14.0f * scale, y + 3.0f * scale, 10.0f * scale, 18.0f * scale, -50.0f, 100.0f);
    }
}


void volume_popup::paint_feedback(HDC hdc) {
    RECT rc;
    GetClientRect(m_window, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    if (w <= 0 || h <= 0) return;

    // Double buffer memory DC
    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP old_bmp = (HBITMAP)SelectObject(mem_dc, mem_bmp);

    // Dark mode check
    bool is_dark = false;
    int theme_mode = get_theme_mode(); // 0=Auto, 1=Dark, 2=Light
    if (theme_mode == 0) {
        try {
            fb2k::CCoreDarkModeHooks darkModeHooks;
            is_dark = (bool)darkModeHooks;
        } catch (...) {
            is_dark = true;
        }
    } else if (theme_mode == 1) {
        is_dark = true;
    } else {
        is_dark = false;
    }

    // Color definitions
    COLORREF bg_color = is_dark ? RGB(32, 32, 36) : RGB(252, 252, 253);
    COLORREF border_color = is_dark ? RGB(60, 60, 64) : RGB(218, 218, 222);
    COLORREF icon_color = is_dark ? RGB(220, 220, 225) : RGB(40, 40, 45);
    COLORREF track_bg_color = is_dark ? RGB(65, 65, 70) : RGB(190, 190, 195);
    COLORREF track_fill_color = is_dark ? RGB(0, 168, 181) : RGB(0, 150, 165); // Teal accent matching screenshot!
    COLORREF text_color = is_dark ? RGB(240, 240, 245) : RGB(30, 30, 35);

    // 1. Draw Pill Card Background
    HBRUSH bg_brush = CreateSolidBrush(bg_color);
    HPEN border_pen = CreatePen(PS_SOLID, 1, border_color);
    HBRUSH old_brush = (HBRUSH)SelectObject(mem_dc, bg_brush);
    HPEN old_pen = (HPEN)SelectObject(mem_dc, border_pen);

    int corner_radius = get_use_rounded_corners() ? 12 : 6;
    RoundRect(mem_dc, 0, 0, w, h, corner_radius * 2, corner_radius * 2);

    SelectObject(mem_dc, old_pen);
    SelectObject(mem_dc, old_brush);
    DeleteObject(bg_brush);
    DeleteObject(border_pen);

    // Volume ratio 0.0 to 1.0
    float vol_pct = db_to_slider(m_current_volume_db);
    int vol_int = (int)std::round(vol_pct * 100.0f);


    // 2. Draw Speaker Vector Icon (GDI+ smooth anti-aliased Material Design icon)
    int icon_size = 20;
    int icon_x = 14;
    int icon_y = (h - icon_size) / 2;

    Gdiplus::Graphics g(mem_dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Color gdiplus_icon_color(GetRValue(icon_color), GetGValue(icon_color), GetBValue(icon_color));
    draw_speaker_icon(g, icon_x, icon_y, icon_size, vol_pct, gdiplus_icon_color);


    // 3. Draw Slider Track
    int track_x = 44;
    int text_w = 38;
    int track_w = w - track_x - text_w - 12;
    int track_h = 6;
    int track_y = (h - track_h) / 2;

    if (track_w > 0) {
        RECT track_rc = { track_x, track_y, track_x + track_w, track_y + track_h };
        HBRUSH track_bg = CreateSolidBrush(track_bg_color);
        HPEN null_pen = (HPEN)GetStockObject(NULL_PEN);
        old_brush = (HBRUSH)SelectObject(mem_dc, track_bg);
        old_pen = (HPEN)SelectObject(mem_dc, null_pen);
        RoundRect(mem_dc, track_rc.left, track_rc.top, track_rc.right, track_rc.bottom, 6, 6);
        SelectObject(mem_dc, old_brush);
        DeleteObject(track_bg);

        // Draw Filled Accent Bar
        int fill_w = (int)(track_w * vol_pct);
        if (fill_w < 6 && vol_pct > 0.0f) fill_w = 6;
        if (fill_w > track_w) fill_w = track_w;

        if (fill_w > 0) {
            RECT fill_rc = { track_x, track_y, track_x + fill_w, track_y + track_h };
            HBRUSH fill_brush = CreateSolidBrush(track_fill_color);
            old_brush = (HBRUSH)SelectObject(mem_dc, fill_brush);
            RoundRect(mem_dc, fill_rc.left, fill_rc.top, fill_rc.right, fill_rc.bottom, 6, 6);
            SelectObject(mem_dc, old_brush);
            DeleteObject(fill_brush);
        }
        SelectObject(mem_dc, old_pen);
    }

    // 4. Draw Numerical Volume Display
    HFONT hFont = CreateFontW(-14, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT old_font = (HFONT)SelectObject(mem_dc, hFont);
    SetBkMode(mem_dc, TRANSPARENT);
    SetTextColor(mem_dc, text_color);

    std::wstring vol_str = std::to_wstring(vol_int);
    RECT text_rc = { w - text_w - 12, 0, w - 12, h };
    DrawTextW(mem_dc, vol_str.c_str(), (int)vol_str.length(), &text_rc, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);

    SelectObject(mem_dc, old_font);
    DeleteObject(hFont);

    // BitBlt to target DC
    BitBlt(hdc, 0, 0, w, h, mem_dc, 0, 0, SRCCOPY);

    SelectObject(mem_dc, old_bmp);
    DeleteObject(mem_bmp);
    DeleteDC(mem_dc);
}

LRESULT CALLBACK volume_popup::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    volume_popup* pThis = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = (CREATESTRUCT*)lparam;
        pThis = (volume_popup*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (volume_popup*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        switch (msg) {
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                if (pThis->m_is_feedback_mode) {
                    pThis->paint_feedback(hdc);
                } else {
                    pThis->paint(hdc);
                }
                EndPaint(hwnd, &ps);
                return 0;
            }
        case WM_TIMER:
            if (wparam == FEEDBACK_TIMER_ID) {
                pThis->hide();
                return 0;
            }
            break;

        case WM_LBUTTONDOWN:
            {
                pThis->m_is_dragging = true;
                SetCapture(hwnd);
                POINT pt = { (short)LOWORD(lparam), (short)HIWORD(lparam) };
                pThis->update_volume_from_point(pt);
                return 0;
            }
        case WM_MOUSEMOVE:
            {
                if (pThis->m_is_dragging) {
                    POINT pt = { (short)LOWORD(lparam), (short)HIWORD(lparam) };
                    pThis->update_volume_from_point(pt);
                }
                
                // Track mouse leave again if it was reset
                TRACKMOUSEEVENT tme = {0};
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);
                return 0;
            }
        case WM_LBUTTONUP:
            if (pThis->m_is_dragging) {
                pThis->m_is_dragging = false;
                ReleaseCapture();
            }
            return 0;
            
        case WM_MOUSELEAVE:
            // Check if we are dragging. If dragging, ignore mouse leave (cursor can go outside)
            if (!pThis->m_is_dragging) {
                // If not dragging, check if cursor is really outside (sometimes TME_LEAVE triggers weirdly)
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(hwnd, &pt);
                RECT rc;
                GetClientRect(hwnd, &rc);
                if (!PtInRect(&rc, pt)) {
                    pThis->hide();
                }
            }
            return 0;
            
        case WM_ACTIVATE:
            if (LOWORD(wparam) == WA_INACTIVE && !pThis->m_is_dragging) {
                pThis->hide();
            }
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}
