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

// Add rounded rectangle with smooth arcs to GDI+ GraphicsPath
static void add_rounded_rect_to_path(Gdiplus::GraphicsPath& path, float x, float y, float width, float height, float radius) {
    float diameter = radius * 2.0f;
    if (diameter > width) diameter = width;
    if (diameter > height) diameter = height;
    if (diameter <= 0.0f) {
        path.AddRectangle(Gdiplus::RectF(x, y, width, height));
        return;
    }

    path.AddArc(x, y, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(x + width - diameter, y, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(x + width - diameter, y + height - diameter, diameter, diameter, 0.0f, 90.0f);
    path.AddArc(x, y + height - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();
}

// Helper function to get system DPI scale ratio (1.0 = 100% / 96 DPI, 2.0 = 200% / 192 DPI)
static float get_dpi_scale() {
    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
    return (dpi > 0) ? (static_cast<float>(dpi) / 96.0f) : 1.0f;
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
}

void volume_popup::show_at(int x, int y) {
    if (!m_initialized) initialize();

    m_is_feedback_mode = false;
    KillTimer(m_window, FEEDBACK_TIMER_ID);

    // Clear custom window region from feedback mode
    SetWindowRgn(m_window, nullptr, TRUE);

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

    float scale = get_dpi_scale();
    int w = (int)std::round(FEEDBACK_WIDTH * scale);
    int h = (int)std::round(FEEDBACK_HEIGHT * scale);

    // Clear custom window region so GDI+ anti-aliased edges are preserved
    SetWindowRgn(m_window, nullptr, TRUE);

    // Get cursor position
    POINT pt = {};
    GetCursorPos(&pt);

    int win_x = pt.x - (w / 2);
    int win_y = pt.y - h - (int)std::round(40.0f * scale); // Shifted upward for clearer visibility above taskbar

    // Ensure within primary monitor work area with breathing room above taskbar
    RECT work_area = {};
    SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0);

    int margin_side = (int)std::round(10.0f * scale);
    int margin_bottom = (int)std::round(25.0f * scale);

    if (win_x < work_area.left + margin_side) win_x = work_area.left + margin_side;
    if (win_x + w > work_area.right - margin_side) win_x = work_area.right - w - margin_side;
    if (win_y < work_area.top + margin_side) win_y = work_area.top + margin_side;
    if (win_y + h > work_area.bottom - margin_bottom) win_y = work_area.bottom - h - margin_bottom;

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

    // In feedback OSD mode, the slider track runs from FEEDBACK_TRACK_X to the text area.
    if (m_is_feedback_mode) {
        float scale = get_dpi_scale();
        int track_left = (int)std::round(FEEDBACK_TRACK_X * scale);
        int track_right = rc.right - (int)std::round((FEEDBACK_TEXT_W + 12.0f) * scale);
        int track_width = track_right - track_left;

        if (track_width <= 0) return;

        int x = pt.x;
        if (x < track_left) x = track_left;
        if (x > track_right) x = track_right;

        float ratio = (float)(x - track_left) / (float)track_width;
        float new_vol = slider_to_db(ratio);

        try {
            auto playback = playback_control::get();
            playback->set_volume(new_vol);
            m_current_volume_db = new_vol;
        } catch (...) {}

        InvalidateRect(m_window, nullptr, TRUE);
        return;
    }

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
    
    // Fill background with colorkey for transparency
    HBRUSH colorkey_brush = CreateSolidBrush(RGB(255, 0, 255));
    FillRect(hdc, &rc, colorkey_brush);
    DeleteObject(colorkey_brush);
    
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
    
    HBRUSH fill_brush = CreateSolidBrush(get_volume_osd_color()); // User-configurable accent color
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

    // Create 32-bit ARGB DIBSection for true per-pixel alpha blending
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pvBits = nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(hdcScreen);
    HBITMAP mem_bmp = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &pvBits, nullptr, 0);
    HBITMAP old_bmp = (HBITMAP)SelectObject(mem_dc, mem_bmp);

    // Initialize memory bits to 0 (100% transparent ARGB 0x00000000)
    if (pvBits) {
        memset(pvBits, 0, w * h * 4);
    }

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
    COLORREF track_fill_color = get_volume_osd_color(); // User-configurable accent color
    COLORREF text_color = is_dark ? RGB(240, 240, 245) : RGB(30, 30, 35);

    // Initialize GDI+ Graphics
    Gdiplus::Graphics g(mem_dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    float scale = get_dpi_scale();

    // 1. Draw Pill Card Background & Border with GDI+ Anti-Aliasing
    float stroke = 1.0f * scale;
    float pad = stroke * 0.5f;
    float radius = (get_use_rounded_corners() ? 12.0f : 6.0f) * scale;

    Gdiplus::GraphicsPath card_path;
    add_rounded_rect_to_path(card_path, pad, pad, (float)w - stroke, (float)h - stroke, radius);

    Gdiplus::SolidBrush bg_brush(Gdiplus::Color(255, GetRValue(bg_color), GetGValue(bg_color), GetBValue(bg_color)));
    Gdiplus::Pen border_pen(Gdiplus::Color(255, GetRValue(border_color), GetGValue(border_color), GetBValue(border_color)), stroke);

    g.FillPath(&bg_brush, &card_path);
    g.DrawPath(&border_pen, &card_path);

    // Volume ratio 0.0 to 1.0
    float vol_pct = db_to_slider(m_current_volume_db);
    int vol_int = (int)std::round(vol_pct * 100.0f);

    // 2. Draw Speaker Vector Icon
    int icon_size = (int)std::round(FEEDBACK_ICON_SIZE * scale);
    int icon_x = (int)std::round(FEEDBACK_ICON_X * scale);
    int icon_y = (h - icon_size) / 2;

    Gdiplus::Color gdiplus_icon_color(GetRValue(icon_color), GetGValue(icon_color), GetBValue(icon_color));
    draw_speaker_icon(g, icon_x, icon_y, icon_size, vol_pct, gdiplus_icon_color);

    // 4. Measure Numerical Volume Display dynamically first so track_w fits perfectly
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
    float font_size_pt = 10.5f * scale;
    Gdiplus::Font font(L"Segoe UI", font_size_pt, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush text_brush(Gdiplus::Color(255, GetRValue(text_color), GetGValue(text_color), GetBValue(text_color)));
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentFar);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

    std::wstring vol_str = std::to_wstring(vol_int);

    // Measure exact string width needed for volume percentage text
    Gdiplus::RectF bounds;
    g.MeasureString(vol_str.c_str(), (int)vol_str.length(), &font, Gdiplus::PointF(0.0f, 0.0f), &format, &bounds);

    float text_w = (std::max)(bounds.Width + (8.0f * scale), FEEDBACK_TEXT_W * scale);
    float right_margin = 12.0f * scale;

    // 3. Draw Slider Track Bar & Accent Fill
    float track_x = FEEDBACK_TRACK_X * scale;
    float track_w = (float)w - track_x - text_w - right_margin;
    float track_h = 6.0f * scale;
    float track_y = ((float)h - track_h) * 0.5f;

    if (track_w > 0.0f) {
        // Track background capsule
        Gdiplus::GraphicsPath track_path;
        add_rounded_rect_to_path(track_path, track_x, track_y, track_w, track_h, track_h * 0.5f);

        Gdiplus::SolidBrush track_bg_brush(Gdiplus::Color(255, GetRValue(track_bg_color), GetGValue(track_bg_color), GetBValue(track_bg_color)));
        g.FillPath(&track_bg_brush, &track_path);

        // Filled Orange Accent Bar capsule
        float fill_w = track_w * vol_pct;
        if (fill_w < track_h && vol_pct > 0.0f) fill_w = track_h;
        if (fill_w > track_w) fill_w = track_w;

        if (fill_w > 0.0f) {
            Gdiplus::GraphicsPath fill_path;
            add_rounded_rect_to_path(fill_path, track_x, track_y, fill_w, track_h, track_h * 0.5f);

            Gdiplus::SolidBrush track_fill_brush(Gdiplus::Color(255, GetRValue(track_fill_color), GetGValue(track_fill_color), GetBValue(track_fill_color)));
            g.FillPath(&track_fill_brush, &fill_path);
        }
    }

    // Draw text inside calculated bounding box
    Gdiplus::RectF text_rect((float)w - text_w - right_margin, 0.0f, text_w, (float)h);
    g.DrawString(vol_str.c_str(), (int)vol_str.length(), &font, text_rect, &format, &text_brush);

    // Update Layered Window with AC_SRC_ALPHA for hardware per-pixel alpha composition
    RECT win_rect;
    GetWindowRect(m_window, &win_rect);
    POINT ptDst = { win_rect.left, win_rect.top };
    SIZE size = { w, h };
    POINT ptSrc = { 0, 0 };

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(m_window, hdcScreen, &ptDst, &size, mem_dc, &ptSrc, 0, &blend, ULW_ALPHA);

    ReleaseDC(nullptr, hdcScreen);
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
                POINT pt = { (short)LOWORD(lparam), (short)HIWORD(lparam) };
                // Feedback OSD: clicking the speaker/mute icon toggles mute without starting a drag.
                if (pThis->m_is_feedback_mode && pt.x < FEEDBACK_TRACK_X) {
                    try {
                        auto playback = playback_control::get();
                        playback->volume_mute_toggle();
                        pThis->m_current_volume_db = playback->get_volume();
                    } catch (...) {}
                    InvalidateRect(hwnd, nullptr, TRUE);
                    return 0;
                }
                pThis->m_is_dragging = true;
                SetCapture(hwnd);
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
