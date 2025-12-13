#include "stdafx.h"
#include "volume_popup.h"
#include <cmath>

volume_popup* volume_popup::s_instance = nullptr;
extern HINSTANCE g_hIns;

volume_popup::volume_popup()
    : m_window(nullptr)
    , m_initialized(false)
    , m_visible(false)
    , m_is_dragging(false)
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
    // int screen_h = GetSystemMetrics(SM_CYSCREEN);

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

void volume_popup::hide() {
    if (m_visible && m_window) {
        ShowWindow(m_window, SW_HIDE);
        m_visible = false;
        m_is_dragging = false;
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
    
    float new_vol = -100.0f + (ratio * 100.0f);
    if (new_vol > 0.0f) new_vol = 0.0f;
    if (new_vol < -100.0f) new_vol = -100.0f;
    
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
    float volume_percent = (m_current_volume_db + 100.0f) / 100.0f;
    if (volume_percent < 0.0f) volume_percent = 0.0f;
    if (volume_percent > 1.0f) volume_percent = 1.0f;
    
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
                pThis->paint(hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }
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
