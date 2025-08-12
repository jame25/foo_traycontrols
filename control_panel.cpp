#include "stdafx.h"
#include "control_panel.h"
#include "preferences.h"

// Timer constants
#define TIMEOUT_TIMER_ID 4002
#define ANIMATION_TIMER_ID 4003

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
    , m_cover_art_bitmap(nullptr)
    , m_artist_font(nullptr)
    , m_track_font(nullptr)
    , m_animating(false)
    , m_closing(false)
    , m_animation_step(0)
    , m_start_x(0)
    , m_start_y(0)
    , m_final_x(0)
    , m_final_y(0) {
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
    }
    
    cleanup_cover_art();
    cleanup_fonts();
    
    if (m_control_window) {
        DestroyWindow(m_control_window);
        m_control_window = nullptr;
    }
    
    m_initialized = false;
}

void control_panel::show_control_panel() {
    if (!m_initialized || m_visible) return;
    
    // Update with current track info
    update_track_info();
    
    // Position control panel
    position_control_panel();
    
    // Show window
    ShowWindow(m_control_window, SW_SHOWNOACTIVATE);
    SetWindowPos(m_control_window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    
    m_visible = true;
    
    // Start update timer
    SetTimer(m_control_window, UPDATE_TIMER_ID, 1000, update_timer_proc);
    
    // Start timeout timer (5 seconds)
    SetTimer(m_control_window, TIMEOUT_TIMER_ID, 5000, update_timer_proc);
}

void control_panel::hide_control_panel() {
    if (!m_visible || m_animating) return;
    
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
        show_control_panel();
    }
}

void control_panel::update_track_info() {
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
        
        // Load cover art
        if (track.is_valid()) {
            load_cover_art();
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
    if (m_control_window) {
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
        wc.style = CS_HREDRAW | CS_VREDRAW;
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
    
    // Create control window (initially hidden)
    m_control_window = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
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
    
    SetWindowPos(m_control_window, HWND_TOPMOST, x, y, panel_width, panel_height, SWP_NOACTIVATE);
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
                    // Convert album art data to HBITMAP
                    m_cover_art_bitmap = convert_album_art_to_bitmap(data);
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

// Font management methods
void control_panel::load_fonts() {
    cleanup_fonts();
    
    if (get_use_custom_fonts()) {
        // Use custom fonts
        LOGFONT artist_lf = get_artist_font();
        LOGFONT track_lf = get_track_font();
        
        m_artist_font = CreateFontIndirect(&artist_lf);
        m_track_font = CreateFontIndirect(&track_lf);
    } else {
        // Use default fonts
        LOGFONT artist_lf = get_default_font(true, 16);   // Bold, 16pt
        LOGFONT track_lf = get_default_font(false, 14);   // Regular, 14pt
        
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
    // Update current playback position
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

void control_panel::start_slide_out_animation() {
    if (m_animating || !m_visible) return;
    
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
    if (!m_animating) return;
    
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
            {
                POINT pt = {LOWORD(lparam), HIWORD(lparam)};
                
                // Reset timeout timer on user interaction (only if not animating)
                if (panel && panel->m_visible && panel->m_control_window && !panel->m_animating) {
                    KillTimer(panel->m_control_window, TIMEOUT_TIMER_ID);
                    SetTimer(panel->m_control_window, TIMEOUT_TIMER_ID, 5000, update_timer_proc);
                }
                
                // Check which button was clicked - updated for new positions and sizes (21x21 icons)
                if (pt.y >= 80 && pt.y <= 100) { // Button row (21px tall, centered at y=90)
                    if (pt.x >= 131 && pt.x < 151) panel->handle_button_click(BTN_PREV);        // Previous (141 ± 10)
                    else if (pt.x >= 181 && pt.x < 201) panel->handle_button_click(BTN_PLAYPAUSE); // Play/Pause (191 ± 10)
                    else if (pt.x >= 231 && pt.x < 251) panel->handle_button_click(BTN_NEXT);   // Next (241 ± 10)
                }
                // Check if click is in album art area (cover area is 15,15 to 95,95 - see paint_control_panel)
                else if (pt.x >= 15 && pt.x < 95 && pt.y >= 15 && pt.y < 95) {
                    // Click on album art - slide the panel away
                    if (panel && panel->m_visible && !panel->m_animating) {
                        panel->hide_control_panel();
                    }
                }
                
                return 0;
            }
            
        case WM_MOUSEMOVE:
            // Reset timeout timer on mouse movement over the panel (only if not animating)
            if (panel && panel->m_visible && panel->m_control_window && !panel->m_animating) {
                KillTimer(panel->m_control_window, TIMEOUT_TIMER_ID);
                SetTimer(panel->m_control_window, TIMEOUT_TIMER_ID, 5000, update_timer_proc);
            }
            break;
            
        case WM_TIMER:
            if (wparam == UPDATE_TIMER_ID) {
                panel->handle_timer();
                return 0;
            } else if (wparam == UPDATE_TIMER_ID + 1) {
                // Delayed update after track change
                KillTimer(hwnd, UPDATE_TIMER_ID + 1);
                panel->update_track_info();
                return 0;
            } else if (wparam == TIMEOUT_TIMER_ID) {
                // Timeout reached - hide the control panel
                if (panel) panel->hide_control_panel();
                return 0;
            } else if (wparam == ANIMATION_TIMER_ID) {
                // Animation timer - update animation
                if (panel) panel->update_animation();
                return 0;
            }
            break;
            
        case WM_KILLFOCUS:
        case WM_ACTIVATE:
            if (LOWORD(wparam) == WA_INACTIVE) {
                // Hide panel when it loses focus (immediate hide for manual interaction)
                panel->hide_control_panel_immediate();
                return 0;
            }
            break;
        }
    }
    
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

VOID CALLBACK control_panel::update_timer_proc(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time) {
    if (s_instance) {
        if (timer_id == UPDATE_TIMER_ID) {
            s_instance->handle_timer();
        } else if (timer_id == TIMEOUT_TIMER_ID) {
            // Timeout reached - hide the control panel
            s_instance->hide_control_panel();
        }
    }
}

void control_panel::paint_control_panel(HDC hdc) {
    if (!hdc) return;
    
    RECT client_rect;
    GetClientRect(m_control_window, &client_rect);
    
    // Fill background (no border)
    HBRUSH bg_brush = CreateSolidBrush(RGB(32, 32, 32));
    FillRect(hdc, &client_rect, bg_brush);
    DeleteObject(bg_brush);
    
    // Draw album art area (left side)
    RECT cover_rect = {15, 15, 95, 95};
    if (m_cover_art_bitmap) {
        // Draw actual cover art
        HDC cover_dc = CreateCompatibleDC(hdc);
        HBITMAP old_bitmap = (HBITMAP)SelectObject(cover_dc, m_cover_art_bitmap);
        
        // Draw the cover art bitmap (80x80) centered in the cover area
        BitBlt(hdc, cover_rect.left, cover_rect.top, 80, 80, cover_dc, 0, 0, SRCCOPY);
        
        SelectObject(cover_dc, old_bitmap);
        DeleteDC(cover_dc);
    } else {
        // Draw placeholder
        HBRUSH cover_brush = CreateSolidBrush(RGB(60, 60, 60));
        FillRect(hdc, &cover_rect, cover_brush);
        DeleteObject(cover_brush);
        
        // Determine symbol/icon based on whether this is a stream or local file
        bool is_stream = false;
        try {
            auto playback = playback_control::get();
            metadb_handle_ptr track;
            if (playback->get_now_playing(track) && track.is_valid()) {
                pfc::string8 path = track->get_path();
                is_stream = strstr(path.get_ptr(), "://") != nullptr;
            }
        } catch (...) {
            // Ignore errors, default to local file behavior
        }
        
        if (is_stream) {
            // Load and draw radio icon for internet streams
            HICON radio_icon = LoadIcon(g_hIns, MAKEINTRESOURCE(IDI_RADIO_ICON));
            if (radio_icon) {
                // Center the icon in the cover rect (80x80 area)
                int icon_size = 48; // Larger icon for control panel
                int icon_x = cover_rect.left + (80 - icon_size) / 2;
                int icon_y = cover_rect.top + (80 - icon_size) / 2;
                
                DrawIconEx(hdc, icon_x, icon_y, radio_icon, icon_size, icon_size, 0, nullptr, DI_NORMAL);
            } else {
                // Fallback to text if icon can't be loaded
                SetTextColor(hdc, RGB(150, 150, 150));
                SetBkMode(hdc, TRANSPARENT);
                HFONT symbol_font = CreateFont(40, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                               DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
                HFONT old_font = (HFONT)SelectObject(hdc, symbol_font);
                
                DrawText(hdc, L"📻", -1, &cover_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                
                SelectObject(hdc, old_font);
                DeleteObject(symbol_font);
            }
        } else {
            // Add music note symbol for local files
            SetTextColor(hdc, RGB(150, 150, 150));
            SetBkMode(hdc, TRANSPARENT);
            HFONT symbol_font = CreateFont(40, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                           DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Symbol");
            HFONT old_font = (HFONT)SelectObject(hdc, symbol_font);
            
            DrawText(hdc, L"♪", -1, &cover_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            SelectObject(hdc, old_font);
            DeleteObject(symbol_font);
        }
    }
    
    // Draw track info
    draw_track_info(hdc, client_rect);
    
    // Draw time info
    draw_time_info(hdc, client_rect);
    
    // Draw control buttons using custom vector graphics
    // Moved right by 15% of panel width (294 * 0.15 = 44px)
    struct {
        int x, y;
        int button_type; // 0=previous, 1=play/pause, 2=next
    } buttons[] = {
        {141, 90, 0},    // Previous
        {191, 90, 1},    // Play/Pause
        {241, 90, 2}     // Next
    };
    
    // Enable anti-aliasing for smoother drawing
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, nullptr);
    
    int icon_size = 16; // Size of the icons
    
    for (auto& btn : buttons) {
        switch (btn.button_type) {
        case 0: // Previous
            draw_previous_icon(hdc, btn.x, btn.y, icon_size);
            break;
        case 1: // Play/Pause
            if (m_is_playing && !m_is_paused) {
                draw_pause_icon(hdc, btn.x, btn.y, icon_size);
            } else {
                draw_play_icon(hdc, btn.x, btn.y, icon_size);
            }
            break;
        case 2: // Next
            draw_next_icon(hdc, btn.x, btn.y, icon_size);
            break;
        }
    }
}

void control_panel::draw_track_info(HDC hdc, const RECT& client_rect) {
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    
    // Draw track title using custom or default font
    HFONT font_to_use = m_track_font ? m_track_font : CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT old_font = (HFONT)SelectObject(hdc, font_to_use);
    
    RECT title_rect = {105, 20, client_rect.right - 70, 45};
    pfc::stringcvt::string_wide_from_utf8 wide_title(m_current_title.c_str());
    DrawText(hdc, wide_title.get_ptr(), -1, &title_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    
    // Draw artist using custom or default font
    HFONT artist_font_to_use = m_artist_font ? m_artist_font : CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                                          DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SelectObject(hdc, artist_font_to_use);
    
    RECT artist_rect = {105, 50, client_rect.right - 70, 70};
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
