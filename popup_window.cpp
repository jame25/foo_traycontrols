#include "stdafx.h"
#include "popup_window.h"
#include "preferences.h"
#include "artwork_bridge.h"
#include "control_panel.h"
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// Static instance
popup_window* popup_window::s_instance = nullptr;

// External declaration from main.cpp
extern HINSTANCE g_hIns;

// Helper function to get DPI-scaled font height from point size
// Returns negative value as required by CreateFont for character height
static int get_dpi_scaled_font_height(int point_size) {
    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    int height = -MulDiv(point_size, dpi, 72);  // Points to pixels: size * dpi / 72
    ReleaseDC(nullptr, hdc);
    return height;
}

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
    , m_cover_art_bitmap(nullptr)
    , m_artwork_from_bridge(false)
    , m_is_stream(false)
    , m_pending_track(nullptr)
    , m_artwork_wait_count(0) {
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
        KillTimer(m_popup_window, ARTWORK_POLL_TIMER_ID);
        KillTimer(m_popup_window, ARTWORK_WAIT_TIMER_ID);
    }
    
    m_pending_track = nullptr;
    m_artwork_wait_count = 0;
    cleanup_cover_art();
    
    if (m_popup_window) {
        DestroyWindow(m_popup_window);
        m_popup_window = nullptr;
    }
    
    m_initialized = false;
    m_animating = false;
}

static bool is_remote_stream_path(const char* path) {
    if (!path || path[0] == '\0') return false;

    // First check via foobar2000's filesystem service
    try {
        service_ptr_t<filesystem> fs;
        if (filesystem::g_get_interface(fs, path)) {
            return fs->is_remote(path);
        }
    } catch (...) {}

    // Case-insensitive checks for local, archive, and disc formats
    if (_strnicmp(path, "file://", 7) == 0 ||
        _strnicmp(path, "unpack://", 9) == 0 ||
        _strnicmp(path, "cdda://", 7) == 0 ||
        _strnicmp(path, "cue://", 6) == 0 ||
        _strnicmp(path, "zip://", 6) == 0 ||
        _strnicmp(path, "rar://", 6) == 0 ||
        _strnicmp(path, "7z://", 5) == 0 ||
        _strnicmp(path, "iso://", 6) == 0 ||
        _strnicmp(path, "subsong://", 10) == 0 ||
        _strnicmp(path, "alac://", 7) == 0 ||
        _strnicmp(path, "ape://", 6) == 0 ||
        _strnicmp(path, "tak://", 6) == 0) {
        return false;
    }

    // Standard remote streaming protocols
    if (_strnicmp(path, "http://", 7) == 0 ||
        _strnicmp(path, "https://", 8) == 0 ||
        _strnicmp(path, "mms://", 6) == 0 ||
        _strnicmp(path, "rtsp://", 7) == 0 ||
        _strnicmp(path, "rtmp://", 7) == 0 ||
        _strnicmp(path, "icy://", 6) == 0 ||
        _strnicmp(path, "icecast://", 10) == 0 ||
        _strnicmp(path, "hls://", 6) == 0) {
        return true;
    }

    // Any embedded file:// reference
    if (strstr(path, "file://") != nullptr || strstr(path, "FILE://") != nullptr) {
        return false;
    }

    return false;
}

void popup_window::show_track_info(metadb_handle_ptr p_track) {
    if (!m_initialized) {
        initialize();
    }
    if (!m_popup_window) {
        create_popup_window();
    }
    if (!m_initialized || !m_popup_window || !get_show_popup_notification() || !p_track.is_valid()) {
        return;
    }
    
    pfc::string8 track_identifier = p_track->get_path();
    bool is_stream = is_remote_stream_path(track_identifier.get_ptr());
    if (is_stream) {
        // For online radio streams, wait for dynamic metadata in update_stream_metadata() or metadb refresh
        m_pending_track = p_track;
        m_current_track = p_track;
        m_is_stream = true;
        m_last_track_path.clear();
        cleanup_cover_art();
        clear_pending_online_artwork();
        return;
    }
    
    m_is_stream = false;
    m_last_track_path = track_identifier;
    m_pending_track = p_track;
    m_current_track = p_track;
    m_artwork_wait_count = 0;
    
    if (m_popup_window) {
        KillTimer(m_popup_window, ARTWORK_WAIT_TIMER_ID);
        KillTimer(m_popup_window, ANIMATION_TIMER_ID);
        KillTimer(m_popup_window, POPUP_TIMER_ID);
    }
    bool was_fully_visible = (m_visible && !m_animating);
    m_animating = false;
    
    // Purge previous track's artwork and pending online searches to ensure old cover art is never displayed
    cleanup_cover_art();
    clear_pending_online_artwork();
    
    // 1. Update text metadata (Title & Artist)
    update_track_info(p_track);

    // 2. Load cover art for the new track (embedded/local)
    load_cover_art(p_track, false);
    
    // 3. Position and show popup immediately
    position_popup();
    if (was_fully_visible) {
        SetWindowPos(m_popup_window, HWND_TOPMOST, m_final_x, m_final_y, 320, 80, SWP_NOACTIVATE);
        ShowWindow(m_popup_window, SW_SHOWNOACTIVATE);
        InvalidateRect(m_popup_window, nullptr, TRUE);
        SetTimer(m_popup_window, POPUP_TIMER_ID, get_popup_duration(), hide_timer_proc);
    } else {
        start_slide_in_animation();
    }
}

static HBITMAP copy_hbitmap_surface(HBITMAP src_bmp) {
    if (!src_bmp) return nullptr;
    BITMAP bmp;
    if (!GetObject(src_bmp, sizeof(bmp), &bmp) || bmp.bmWidth <= 0 || bmp.bmHeight <= 0) return nullptr;

    HDC screen_dc = GetDC(nullptr);
    HDC src_dc = CreateCompatibleDC(screen_dc);
    HDC dst_dc = CreateCompatibleDC(screen_dc);
    HBITMAP dst_bmp = CreateCompatibleBitmap(screen_dc, bmp.bmWidth, bmp.bmHeight);

    HBITMAP old_src = (HBITMAP)SelectObject(src_dc, src_bmp);
    HBITMAP old_dst = (HBITMAP)SelectObject(dst_dc, dst_bmp);

    BitBlt(dst_dc, 0, 0, bmp.bmWidth, bmp.bmHeight, src_dc, 0, 0, SRCCOPY);

    SelectObject(src_dc, old_src);
    SelectObject(dst_dc, old_dst);
    DeleteDC(src_dc);
    DeleteDC(dst_dc);
    ReleaseDC(nullptr, screen_dc);

    return dst_bmp;
}

void popup_window::show_preview() {
    if (!m_initialized) {
        initialize();
    }
    if (!m_popup_window) {
        create_popup_window();
    }
    if (!m_popup_window) return;

    m_last_track_path.clear();

    if (m_popup_window) {
        KillTimer(m_popup_window, ARTWORK_WAIT_TIMER_ID);
    }
    cleanup_cover_art();

    metadb_handle_ptr track;
    try {
        auto playback = playback_control::get();
        if (playback->get_now_playing(track) && track.is_valid()) {
            m_pending_track = track;
            m_current_track = track;
            update_track_info(track);
            load_cover_art(track, true);

            position_popup();
            if (m_visible && !m_animating) {
                SetWindowPos(m_popup_window, HWND_TOPMOST, m_final_x, m_final_y, 320, 80, SWP_NOACTIVATE);
                InvalidateRect(m_popup_window, nullptr, TRUE);
                SetTimer(m_popup_window, POPUP_TIMER_ID, get_popup_duration(), hide_timer_proc);
            } else if (!m_visible) {
                start_slide_in_animation();
            }
            return;
        }
    } catch (...) {
    }

    // Nothing currently playing - display sample preview popup notification
    m_current_track = nullptr;
    m_pending_track = nullptr;

    position_popup();
    if (m_visible && !m_animating) {
        SetWindowPos(m_popup_window, HWND_TOPMOST, m_final_x, m_final_y, 320, 80, SWP_NOACTIVATE);
        InvalidateRect(m_popup_window, nullptr, TRUE);
        SetTimer(m_popup_window, POPUP_TIMER_ID, get_popup_duration(), hide_timer_proc);
    } else if (!m_visible) {
        start_slide_in_animation();
    }
}

void popup_window::on_artwork_wait_timer() {
    if (!m_initialized || !get_show_popup_notification() || !m_pending_track.is_valid()) {
        if (m_popup_window) KillTimer(m_popup_window, ARTWORK_WAIT_TIMER_ID);
        return;
    }
    
    m_artwork_wait_count++;
    
    // Check if artwork has arrived via foo_artwork callback
    if (has_pending_online_artwork_popup()) {
        HBITMAP bitmap = get_pending_online_artwork_popup();
        if (bitmap) {
            cleanup_cover_art();
            m_cover_art_bitmap = bitmap;
            m_artwork_from_bridge = false;
        }
    }
    
    // Check if foo_artwork is actively downloading artwork over the network
    bool is_downloading = is_online_artwork_loading();
    
    // 1. Artwork loaded successfully
    // 2. Online search finished (not downloading) AND waited at least 15 steps (750ms)
    // 3. Network timeout reached (3.5s)
    bool artwork_ready = (m_cover_art_bitmap != nullptr);
    bool search_finished = (!is_downloading && m_artwork_wait_count >= 15);
    bool network_timeout = (m_artwork_wait_count >= MAX_ARTWORK_WAIT_STEPS);
    
    if (artwork_ready || search_finished || network_timeout) {
        if (m_popup_window) {
            KillTimer(m_popup_window, ARTWORK_WAIT_TIMER_ID);
        }
        
        // Update track info and display popup
        update_track_info(m_pending_track);
        position_popup();
        
        if (m_visible && !m_animating) {
            SetWindowPos(m_popup_window, HWND_TOPMOST, m_final_x, m_final_y, 320, 80, SWP_NOACTIVATE);
            InvalidateRect(m_popup_window, nullptr, TRUE);
            SetTimer(m_popup_window, POPUP_TIMER_ID, get_popup_duration(), hide_timer_proc);
        } else if (!m_visible) {
            start_slide_in_animation();
        }
    }
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
}

void popup_window::on_settings_changed() {
    if (!get_show_popup_notification()) {
        if (m_popup_window) {
            KillTimer(m_popup_window, ARTWORK_WAIT_TIMER_ID);
        }
        if (m_visible) {
            hide_popup();
        }
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
    
    RECT work_area = {};
    if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0)) {
        work_area.left = 0;
        work_area.top = 0;
        work_area.right = GetSystemMetrics(SM_CXSCREEN);
        work_area.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    
    const int popup_width = 320;
    const int popup_height = 80;
    const int margin = 10;
    
    int popup_position = get_popup_position();
    if (popup_position < 0 || popup_position > 5) popup_position = 0;
    bool is_right_side = (popup_position >= 3);

    int x = is_right_side ? (work_area.right - popup_width - margin) : (work_area.left + margin);

    int vertical_position = popup_position % 3;
    int y;
    switch (vertical_position) {
    case 0: // Top
        y = work_area.top + margin;
        break;
    case 1: // Middle
        y = work_area.top + (work_area.bottom - work_area.top - popup_height) / 2;
        break;
    case 2: // Bottom
    default:
        y = work_area.bottom - popup_height - margin;
        break;
    }
    
    m_final_x = x;
    m_final_y = y;
}

static const char* safe_meta_get_popup(const file_info& info, const char* name) {
    t_size index = info.meta_find(name);
    if (index != pfc_infinite && info.meta_enum_value_count(index) > 0) {
        const char* val = info.meta_enum_value(index, 0);
        if (val && val[0] != '\0') return val;
    }
    return nullptr;
}

void popup_window::update_track_info(metadb_handle_ptr p_track) {
    if (!p_track.is_valid()) return;
    
    m_current_track = p_track;
    
    pfc::string8 path = p_track->get_path();
    m_is_stream = is_remote_stream_path(path.get_ptr());
    m_current_title = "";
    m_current_artist = "";
    format_display_lines_track(p_track, m_current_title, m_current_artist);

    if (!m_is_stream && (m_current_title.is_empty() || m_current_artist.is_empty())) {
        metadb_info_container::ptr info_container = p_track->get_info_ref();
        if (info_container.is_valid()) {
            const file_info& info = info_container->info();
            if (m_current_title.is_empty()) {
                const char* val = safe_meta_get_popup(info, "TITLE");
                if (!val) val = safe_meta_get_popup(info, "title");
                if (!val) val = pfc::string_filename_ext(p_track->get_path()).get_ptr();
                if (val) m_current_title = val;
            }
            if (m_current_artist.is_empty()) {
                const char* val = safe_meta_get_popup(info, "ARTIST");
                if (!val) val = safe_meta_get_popup(info, "artist");
                if (!val) val = safe_meta_get_popup(info, "ALBUMARTIST");
                if (!val) val = safe_meta_get_popup(info, "albumartist");
                if (!val) val = safe_meta_get_popup(info, "PERFORMER");
                if (!val) val = safe_meta_get_popup(info, "performer");
                if (val) m_current_artist = val;
            }
        }
    }

    // Fallback for streams if format returned empty
    if (m_is_stream && (m_current_title.is_empty() || m_current_artist.is_empty())) {
        file_info_impl info;
        if (p_track->get_info(info)) {
            if (m_current_title.is_empty()) {
                if (info.meta_exists("title")) {
                    m_current_title = info.meta_get("title", 0);
                } else if (info.meta_exists("TITLE")) {
                    m_current_title = info.meta_get("TITLE", 0);
                } else if (info.meta_exists("server")) {
                    m_current_title = info.meta_get("server", 0);
                } else if (info.meta_exists("SERVER")) {
                    m_current_title = info.meta_get("SERVER", 0);
                }
            }
            if (m_current_artist.is_empty()) {
                if (info.meta_exists("artist")) {
                    m_current_artist = info.meta_get("artist", 0);
                } else if (info.meta_exists("ARTIST")) {
                    m_current_artist = info.meta_get("ARTIST", 0);
                }
            }
        }
    }

    if (m_current_title.is_empty()) m_current_title = "Unknown Title";
    if (m_current_artist.is_empty()) m_current_artist = "Unknown Artist";
    
    if (m_is_stream) {
        // If stream has valid discovered track title (not raw URL or placeholder)
        bool has_valid_title = !m_current_title.is_empty() && 
                               m_current_title != "Unknown Title" && 
                               m_current_title.find_first("http://") != 0 && 
                               m_current_title.find_first("https://") != 0;
        
        if (has_valid_title) {
            pfc::string8 stream_id;
            if (!m_current_artist.is_empty() && m_current_artist != "Unknown Artist") {
                stream_id << m_current_artist << " - " << m_current_title;
            } else {
                stream_id = m_current_title;
            }

            if (stream_id != m_last_track_path) {
                m_last_track_path = stream_id;

                // Load cover art for the stream track
                load_cover_art(p_track, false);

                if (get_show_popup_notification()) {
                    if (m_popup_window) {
                        KillTimer(m_popup_window, ARTWORK_WAIT_TIMER_ID);
                        KillTimer(m_popup_window, ANIMATION_TIMER_ID);
                        KillTimer(m_popup_window, POPUP_TIMER_ID);
                    }
                    bool was_fully_visible = (m_visible && !m_animating);
                    m_animating = false;

                    position_popup();
                    if (was_fully_visible) {
                        SetWindowPos(m_popup_window, HWND_TOPMOST, m_final_x, m_final_y, 320, 80, SWP_NOACTIVATE);
                        ShowWindow(m_popup_window, SW_SHOWNOACTIVATE);
                        InvalidateRect(m_popup_window, nullptr, TRUE);
                        SetTimer(m_popup_window, POPUP_TIMER_ID, get_popup_duration(), hide_timer_proc);
                    } else {
                        start_slide_in_animation();
                    }
                    return;
                }
            }
        }
    } else {
        m_last_track_path = p_track->get_path();
    }

    // Force repaint to update displayed info
    if (m_popup_window) {
        InvalidateRect(m_popup_window, nullptr, TRUE);
    }
}

static bool is_inverted_stream_popup(const file_info& info, metadb_handle_ptr track = nullptr) {
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

void popup_window::update_stream_metadata(const file_info & p_info) {
    if (!m_initialized || !get_show_popup_notification()) return;

    try {
        metadb_handle_ptr track = m_pending_track.is_valid() ? m_pending_track : m_current_track;
        if (!track.is_valid()) {
            auto playback = playback_control::get();
            if (!playback->get_now_playing(track) || !track.is_valid()) {
                return;
            }
        }
        pfc::string8 path = track->get_path();
        if (!is_remote_stream_path(path.get_ptr())) {
            return;
        }

        pfc::string8 artist, title;

        const char* p_artist = safe_meta_get_popup(p_info, "ARTIST");
        const char* p_title = safe_meta_get_popup(p_info, "TITLE");

        const char* stream_title = safe_meta_get_popup(p_info, "STREAMTITLE");
        if (!stream_title) {
            stream_title = safe_meta_get_popup(p_info, "ICY_TITLE");
        }

        if (stream_title) {
            const char* tilde = strchr(stream_title, '~');
            if (tilde) {
                if (!p_artist) {
                    artist.set_string(stream_title, tilde - stream_title);
                }
                if (!p_title) {
                    const char* next_tilde = strchr(tilde + 1, '~');
                    if (next_tilde) {
                        title.set_string(tilde + 1, next_tilde - (tilde + 1));
                    } else {
                        title.set_string(tilde + 1);
                    }
                }
            } else {
                const char* dash = strstr(stream_title, " - ");
                if (!dash) dash = strstr(stream_title, " ˗ ");
                if (!dash) dash = strstr(stream_title, " – ");
                if (!dash) dash = strstr(stream_title, " / ");
                if (dash) {
                    if (!p_artist) {
                        artist.set_string(stream_title, dash - stream_title);
                    }
                    if (!p_title) {
                        size_t delim_len = (strstr(stream_title, " - ") == dash || strstr(stream_title, " / ") == dash) ? 3 : 4;
                        title.set_string(dash + delim_len);
                    }
                } else if (!p_title) {
                    title = stream_title;
                }
            }
        }

        if (p_artist && artist.is_empty()) artist = p_artist;
        if (p_title && title.is_empty()) title = p_title;

        if (artist.is_empty() && !title.is_empty()) {
            pfc::string8 temp = title;
            const char* tilde = strchr(temp.get_ptr(), '~');
            if (tilde) {
                artist.set_string(temp.get_ptr(), tilde - temp.get_ptr());
                const char* next_tilde = strchr(tilde + 1, '~');
                if (next_tilde) {
                    title.set_string(tilde + 1, next_tilde - (tilde + 1));
                } else {
                    title.set_string(tilde + 1);
                }
            } else {
                const char* dash = strstr(temp.get_ptr(), " - ");
                if (!dash) dash = strstr(temp.get_ptr(), " ˗ ");
                if (!dash) dash = strstr(temp.get_ptr(), " – ");
                if (!dash) dash = strstr(temp.get_ptr(), " / ");
                if (dash) {
                    artist.set_string(temp.get_ptr(), dash - temp.get_ptr());
                    size_t delim_len = (strstr(temp.get_ptr(), " - ") == dash || strstr(temp.get_ptr(), " / ") == dash) ? 3 : 4;
                    title = dash + delim_len;
                }
            }
        }

        if (artist.is_empty()) {
            const char* val = safe_meta_get_popup(p_info, "ALBUMARTIST");
            if (val) artist = val;
        }
        if (artist.is_empty()) {
            const char* val = safe_meta_get_popup(p_info, "PERFORMER");
            if (val) artist = val;
        }
        if (title.is_empty()) {
            const char* val = safe_meta_get_popup(p_info, "DESCRIPTION");
            if (val) title = val;
        }
        if (title.is_empty()) {
            const char* val = safe_meta_get_popup(p_info, "COMMENT");
            if (val) title = val;
        }

        if (title.is_empty() && artist.is_empty()) return;

        artist.trim(' ');
        title.trim(' ');

        if (is_inverted_stream_popup(p_info, m_pending_track.is_valid() ? m_pending_track : m_current_track)) {
            pfc::string8 temp = artist;
            artist = title;
            title = temp;
        }

        pfc::string8 stream_id;
        stream_id << artist << " - " << title;
        if (stream_id == m_last_track_path) return;
        m_last_track_path = stream_id;

        m_current_title = title.is_empty() ? "Unknown Title" : title;
        m_current_artist = artist.is_empty() ? "Unknown Artist" : artist;
        m_is_stream = true;

        // Show popup notification for stream track change
        if (m_popup_window) {
            KillTimer(m_popup_window, ARTWORK_WAIT_TIMER_ID);
            KillTimer(m_popup_window, ANIMATION_TIMER_ID);
            KillTimer(m_popup_window, POPUP_TIMER_ID);
        }
        bool was_fully_visible = (m_visible && !m_animating);
        m_animating = false;

        position_popup();
        if (was_fully_visible) {
            SetWindowPos(m_popup_window, HWND_TOPMOST, m_final_x, m_final_y, 320, 80, SWP_NOACTIVATE);
            ShowWindow(m_popup_window, SW_SHOWNOACTIVATE);
            InvalidateRect(m_popup_window, nullptr, TRUE);
            SetTimer(m_popup_window, POPUP_TIMER_ID, get_popup_duration(), hide_timer_proc);
        } else {
            start_slide_in_animation();
        }
    } catch (...) {}
}

void popup_window::on_online_artwork_received() {
    if (has_pending_online_artwork_popup()) {
        HBITMAP bitmap = get_pending_online_artwork_popup();
        if (bitmap) {
            cleanup_cover_art();
            m_cover_art_bitmap = bitmap;
            m_artwork_from_bridge = false; // We own the copy

            // Refresh track title and artist if stream metadata was discovered
            auto playback = playback_control::get();
            metadb_handle_ptr track;
            if (playback->get_now_playing(track) && track.is_valid()) {
                pfc::string8 line1, line2;
                format_display_lines_track(track, line1, line2);
                if (!line1.is_empty() && line1 != "Unknown Title") m_current_title = line1;
                if (!line2.is_empty() && line2 != "Unknown Artist") m_current_artist = line2;

                pfc::string8 path = track->get_path();
                if (is_remote_stream_path(path.get_ptr())) {
                    pfc::string8 stream_id;
                    if (!m_current_artist.is_empty() && m_current_artist != "Unknown Artist") {
                        stream_id << m_current_artist << " - " << m_current_title;
                    } else {
                        stream_id = m_current_title;
                    }

                    bool has_valid_title = !m_current_title.is_empty() && 
                                           m_current_title != "Unknown Title" && 
                                           m_current_title.find_first("http://") != 0 && 
                                           m_current_title.find_first("https://") != 0;

                    if (has_valid_title && stream_id != m_last_track_path) {
                        m_last_track_path = stream_id;
                        if (get_show_popup_notification()) {
                            if (m_popup_window) {
                                KillTimer(m_popup_window, ARTWORK_WAIT_TIMER_ID);
                                KillTimer(m_popup_window, ANIMATION_TIMER_ID);
                                KillTimer(m_popup_window, POPUP_TIMER_ID);
                            }
                            bool was_fully_visible = (m_visible && !m_animating);
                            m_animating = false;

                            position_popup();
                            if (was_fully_visible) {
                                SetWindowPos(m_popup_window, HWND_TOPMOST, m_final_x, m_final_y, 320, 80, SWP_NOACTIVATE);
                                ShowWindow(m_popup_window, SW_SHOWNOACTIVATE);
                                InvalidateRect(m_popup_window, nullptr, TRUE);
                                SetTimer(m_popup_window, POPUP_TIMER_ID, get_popup_duration(), hide_timer_proc);
                            } else {
                                start_slide_in_animation();
                            }
                            return;
                        }
                    }
                }
            }

            if (m_popup_window) {
                KillTimer(m_popup_window, ARTWORK_POLL_TIMER_ID);
                KillTimer(m_popup_window, ARTWORK_WAIT_TIMER_ID);
            }

            if (m_visible && m_popup_window) {
                InvalidateRect(m_popup_window, nullptr, TRUE);
            }
        }
    }
}

void popup_window::load_cover_art(metadb_handle_ptr p_track, bool allow_stale_fallback) {
    if (!p_track.is_valid()) return;

    // Check if artwork has arrived via callback from foo_artwork for this search
    if (has_pending_online_artwork_popup()) {
        HBITMAP bitmap = get_pending_online_artwork_popup();
        if (bitmap) {
            cleanup_cover_art();
            m_cover_art_bitmap = bitmap;
            m_artwork_from_bridge = false;
            if (m_popup_window) KillTimer(m_popup_window, ARTWORK_POLL_TIMER_ID);
            return;
        }
    }

    try {
        pfc::string8 path = p_track->get_path();
        bool is_stream = is_remote_stream_path(path.get_ptr());

        if (is_stream) {
            // Check if foo_artwork already has active artwork ready
            try {
                HBITMAP online_art = get_current_online_artwork();
                if (online_art) {
                    cleanup_cover_art();
                    m_cover_art_bitmap = online_art;
                    m_artwork_from_bridge = false;
                    if (m_popup_window) KillTimer(m_popup_window, ARTWORK_POLL_TIMER_ID);
                    return;
                }
            } catch (...) {}
        }

        // Try local/embedded artwork ONLY for local files (NEVER for streams - prevents network lockup)
        if (!is_stream) {
            album_art_data_ptr data;
            try {
                auto api_v3 = album_art_manager_v3::get();
                if (api_v3.is_valid()) {
                    auto extractor = api_v3->open(
                        pfc::list_single_ref_t<metadb_handle_ptr>(p_track),
                        pfc::list_single_ref_t<GUID>(album_art_ids::cover_front),
                        fb2k::noAbort
                    );
                    if (extractor.is_valid()) {
                        extractor->query(album_art_ids::cover_front, data, fb2k::noAbort);
                    }
                }
            } catch (...) {}

            if (!data.is_valid() || data->get_size() == 0) {
                try {
                    auto api_v2 = album_art_manager_v2::get();
                    if (api_v2.is_valid()) {
                        auto extractor = api_v2->open(
                            pfc::list_single_ref_t<metadb_handle_ptr>(p_track),
                            pfc::list_single_ref_t<GUID>(album_art_ids::cover_front),
                            fb2k::noAbort
                        );
                        if (extractor.is_valid()) {
                            data = extractor->query(album_art_ids::cover_front, fb2k::noAbort);
                        }
                    }
                } catch (...) {}
            }

            if (data.is_valid() && data->get_size() > 0) {
                // Found local/embedded artwork - replace old artwork
                cleanup_cover_art();
                m_cover_art_bitmap = convert_album_art_to_bitmap(data);
                if (m_cover_art_bitmap) return;
            }
        }

        // Fallbacks ONLY allowed for manual preview button click
        if (allow_stale_fallback) {
            // Fallback 1: Check foo_artwork active or last received online artwork
            try {
                HBITMAP online_art = get_current_online_artwork();
                if (!online_art) {
                    online_art = get_last_online_artwork();
                }
                if (online_art) {
                    cleanup_cover_art();
                    m_cover_art_bitmap = online_art;
                    m_artwork_from_bridge = false;
                    return;
                }
            } catch (...) {}

            // Fallback 2: Check Control Panel active artwork bitmap
            HBITMAP cp_art = control_panel::get_instance().get_cover_art_bitmap();
            if (cp_art) {
                cleanup_cover_art();
                m_cover_art_bitmap = copy_hbitmap_surface(cp_art);
                m_artwork_from_bridge = false;
                if (m_cover_art_bitmap) return;
            }
        }

        // No artwork available for this track yet
        cleanup_cover_art();
    } catch (...) {
        // Ignore errors
    }
}

void popup_window::cleanup_cover_art() {
    if (m_cover_art_bitmap) {
        if (!m_artwork_from_bridge) {
            DeleteObject(m_cover_art_bitmap);
        }
        m_cover_art_bitmap = nullptr;
    }
    m_artwork_from_bridge = false;
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
                RECT client_rect;
                GetClientRect(hwnd, &client_rect);
                int width = client_rect.right - client_rect.left;
                int height = client_rect.bottom - client_rect.top;

                HDC mem_dc = CreateCompatibleDC(hdc);
                HBITMAP mem_bitmap = CreateCompatibleBitmap(hdc, width, height);
                HBITMAP old_bitmap = (HBITMAP)SelectObject(mem_dc, mem_bitmap);

                popup->paint_popup(mem_dc);

                BitBlt(hdc, 0, 0, width, height, mem_dc, 0, 0, SRCCOPY);

                SelectObject(mem_dc, old_bitmap);
                DeleteObject(mem_bitmap);
                DeleteDC(mem_dc);

                EndPaint(hwnd, &ps);
                return 0;
            }

        case WM_ERASEBKGND:
            return 1;
            
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
            } else if (wparam == ARTWORK_POLL_TIMER_ID) {
                // Poll foo_artwork for completed artwork search
                if (popup && has_pending_online_artwork_popup()) {
                    HBITMAP bitmap = get_pending_online_artwork_popup();
                    if (bitmap) {
                        popup->cleanup_cover_art();
                        popup->m_cover_art_bitmap = bitmap;
                        popup->m_artwork_from_bridge = false; // We own the copy
                        KillTimer(hwnd, ARTWORK_POLL_TIMER_ID);
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
                return 0;
            } else if (wparam == ARTWORK_WAIT_TIMER_ID) {
                if (popup) {
                    popup->on_artwork_wait_timer();
                }
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

static std::unique_ptr<Gdiplus::Bitmap> create_gdiplus_bitmap_from_hbitmap(HDC hdc, HBITMAP hbmp) {
    if (!hbmp || !hdc) return nullptr;

    BITMAP bmp;
    if (!GetObject(hbmp, sizeof(bmp), &bmp) || bmp.bmWidth <= 0 || bmp.bmHeight <= 0) return nullptr;

    std::unique_ptr<Gdiplus::Bitmap> gdi_bmp(Gdiplus::Bitmap::FromHBITMAP(hbmp, nullptr));
    if (gdi_bmp && gdi_bmp->GetLastStatus() == Gdiplus::Ok) {
        return gdi_bmp;
    }

    // Fallback: Copy via GDI BitBlt
    std::unique_ptr<Gdiplus::Bitmap> copy_bmp(new Gdiplus::Bitmap(bmp.bmWidth, bmp.bmHeight, PixelFormat32bppARGB));
    Gdiplus::Graphics g(copy_bmp.get());
    HDC g_dc = g.GetHDC();
    if (g_dc) {
        HDC mem_dc = CreateCompatibleDC(hdc);
        HBITMAP old_bm = (HBITMAP)SelectObject(mem_dc, hbmp);
        BitBlt(g_dc, 0, 0, bmp.bmWidth, bmp.bmHeight, mem_dc, 0, 0, SRCCOPY);
        SelectObject(mem_dc, old_bm);
        DeleteDC(mem_dc);
        g.ReleaseHDC(g_dc);
    }
    return copy_bmp;
}

static bool is_popup_dark_mode() {
    int bg_style = get_background_style(); // 0 = Solid, 1 = Artwork Colors, 2 = Blurred Artwork
    if (bg_style != 0) {
        // Light mode has no effect for Artwork Colors or Blurred Artwork
        return true;
    }

    int theme_mode = get_theme_mode(); // 0 = Auto, 1 = Dark, 2 = Light
    if (theme_mode == 0) {
        try {
            fb2k::CCoreDarkModeHooks darkModeHooks;
            return (bool)darkModeHooks;
        } catch (...) {
            return true;
        }
    } else if (theme_mode == 1) {
        return true; // Dark mode
    } else {
        return false; // Light mode
    }
}

void popup_window::paint_popup(HDC hdc) {
    if (!hdc) return;
    
    RECT client_rect;
    GetClientRect(m_popup_window, &client_rect);
    int window_width = client_rect.right - client_rect.left;
    int window_height = client_rect.bottom - client_rect.top;
    
    int bg_style = get_background_style(); // 0 = Solid, 1 = Artwork Colors, 2 = Blurred Artwork
    bool bg_painted = false;

    if (bg_style == 1 && m_cover_art_bitmap) {
        if (window_width > 0 && window_height > 0) {
            BITMAP bmp;
            if (GetObject(m_cover_art_bitmap, sizeof(bmp), &bmp) && bmp.bmWidth > 0 && bmp.bmHeight > 0) {
                HDC mem_dc = CreateCompatibleDC(hdc);
                HBITMAP old_bm = (HBITMAP)SelectObject(mem_dc, m_cover_art_bitmap);

                long total_r = 0, total_g = 0, total_b = 0;
                int pixel_count = 0;
                const int grid_size = 8;

                for (int y = 0; y < grid_size; y++) {
                    int sample_y = (bmp.bmHeight * (y + 1)) / (grid_size + 1);
                    for (int x = 0; x < grid_size; x++) {
                        int sample_x = (bmp.bmWidth * (x + 1)) / (grid_size + 1);
                        COLORREF c = GetPixel(mem_dc, sample_x, sample_y);
                        if (c != CLR_INVALID) {
                            total_r += GetRValue(c);
                            total_g += GetGValue(c);
                            total_b += GetBValue(c);
                            pixel_count++;
                        }
                    }
                }

                SelectObject(mem_dc, old_bm);
                DeleteDC(mem_dc);

                if (pixel_count > 0) {
                    int avg_r = total_r / pixel_count;
                    int avg_g = total_g / pixel_count;
                    int avg_b = total_b / pixel_count;

                    Gdiplus::Color primary(255, avg_r, avg_g, avg_b);
                    Gdiplus::Color secondary(255, avg_r * 65 / 100, avg_g * 65 / 100, avg_b * 65 / 100);

                    Gdiplus::Graphics g(hdc);
                    Gdiplus::Rect g_rect(0, 0, window_width, window_height);
                    Gdiplus::LinearGradientBrush brush(
                        Gdiplus::Point(0, 0),
                        Gdiplus::Point(window_width, 0),
                        secondary,
                        primary
                    );
                    g.FillRectangle(&brush, g_rect);

                    Gdiplus::SolidBrush overlay(Gdiplus::Color(50, 0, 0, 0));
                    g.FillRectangle(&overlay, g_rect);
                    bg_painted = true;
                }
            }
        }
    } else if (bg_style == 2 && m_cover_art_bitmap) {
        if (window_width > 0 && window_height > 0) {
            std::unique_ptr<Gdiplus::Bitmap> src_bitmap = create_gdiplus_bitmap_from_hbitmap(hdc, m_cover_art_bitmap);
            if (src_bitmap && src_bitmap->GetLastStatus() == Gdiplus::Ok) {
                const int blur_size = 64;
                Gdiplus::Bitmap scaled(blur_size, blur_size, PixelFormat32bppARGB);
                {
                    Gdiplus::Graphics gfx(&scaled);
                    gfx.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
                    gfx.DrawImage(src_bitmap.get(), 0, 0, blur_size, blur_size);
                }

                Gdiplus::Rect lockRect(0, 0, blur_size, blur_size);
                Gdiplus::BitmapData scaledData;
                if (scaled.LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &scaledData) == Gdiplus::Ok) {
                    std::vector<BYTE> tempBuffer(blur_size * blur_size * 4);
                    std::vector<BYTE> blurBuffer(blur_size * blur_size * 4);

                    BYTE* srcPixels = static_cast<BYTE*>(scaledData.Scan0);
                    int srcStride = scaledData.Stride;
                    const int radius = 4;

                    // Pass 1: Horizontal blur
                    for (int y = 0; y < blur_size; y++) {
                        for (int x = 0; x < blur_size; x++) {
                            int total_r = 0, total_g = 0, total_b = 0, total_a = 0;
                            int count = 0;
                            for (int dx = -radius; dx <= radius; dx++) {
                                int sx = x + dx;
                                if (sx >= 0 && sx < blur_size) {
                                    BYTE* pixel = srcPixels + y * srcStride + sx * 4;
                                    total_b += pixel[0];
                                    total_g += pixel[1];
                                    total_r += pixel[2];
                                    total_a += pixel[3];
                                    count++;
                                }
                            }
                            int dstIdx = (y * blur_size + x) * 4;
                            tempBuffer[dstIdx + 0] = static_cast<BYTE>(total_b / count);
                            tempBuffer[dstIdx + 1] = static_cast<BYTE>(total_g / count);
                            tempBuffer[dstIdx + 2] = static_cast<BYTE>(total_r / count);
                            tempBuffer[dstIdx + 3] = static_cast<BYTE>(total_a / count);
                        }
                    }

                    scaled.UnlockBits(&scaledData);

                    // Pass 2: Vertical blur
                    for (int y = 0; y < blur_size; y++) {
                        for (int x = 0; x < blur_size; x++) {
                            int total_r = 0, total_g = 0, total_b = 0, total_a = 0;
                            int count = 0;
                            for (int dy = -radius; dy <= radius; dy++) {
                                int sy = y + dy;
                                if (sy >= 0 && sy < blur_size) {
                                    int srcIdx = (sy * blur_size + x) * 4;
                                    total_b += tempBuffer[srcIdx + 0];
                                    total_g += tempBuffer[srcIdx + 1];
                                    total_r += tempBuffer[srcIdx + 2];
                                    total_a += tempBuffer[srcIdx + 3];
                                    count++;
                                }
                            }
                            int dstIdx = (y * blur_size + x) * 4;
                            blurBuffer[dstIdx + 0] = static_cast<BYTE>(total_b / count);
                            blurBuffer[dstIdx + 1] = static_cast<BYTE>(total_g / count);
                            blurBuffer[dstIdx + 2] = static_cast<BYTE>(total_r / count);
                            blurBuffer[dstIdx + 3] = static_cast<BYTE>(total_a / count);
                        }
                    }

                    Gdiplus::Bitmap blurred_artwork(window_width, window_height, PixelFormat32bppARGB);
                    Gdiplus::Rect outRect(0, 0, window_width, window_height);
                    Gdiplus::BitmapData outData;
                    if (blurred_artwork.LockBits(&outRect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &outData) == Gdiplus::Ok) {
                        BYTE* outPixels = static_cast<BYTE*>(outData.Scan0);
                        int outStride = outData.Stride;

                        float targetAspect = static_cast<float>(window_width) / static_cast<float>(window_height);
                        float srcX = 0, srcY = 0, srcW = static_cast<float>(blur_size), srcH = static_cast<float>(blur_size);

                        if (targetAspect > 1.0f) {
                            srcH = blur_size / targetAspect;
                            srcY = (blur_size - srcH) / 2.0f;
                        } else {
                            srcW = blur_size * targetAspect;
                            srcX = (blur_size - srcW) / 2.0f;
                        }

                        for (int y = 0; y < window_height; y++) {
                            BYTE* outRow = outPixels + y * outStride;
                            float sy_base = srcY + (y / static_cast<float>(window_height)) * srcH;

                            for (int x = 0; x < window_width; x++) {
                                float sx = srcX + (x / static_cast<float>(window_width)) * srcW;
                                float sy = sy_base;

                                int x0 = static_cast<int>(sx);
                                int y0 = static_cast<int>(sy);
                                int x1 = (std::min)(x0 + 1, blur_size - 1);
                                int y1 = (std::min)(y0 + 1, blur_size - 1);
                                x0 = (std::max)(0, (std::min)(x0, blur_size - 1));
                                y0 = (std::max)(0, (std::min)(y0, blur_size - 1));

                                float fx = sx - static_cast<int>(sx);
                                float fy = sy - static_cast<int>(sy);
                                float fx_inv = 1.0f - fx;
                                float fy_inv = 1.0f - fy;

                                int idx00 = (y0 * blur_size + x0) * 4;
                                int idx10 = (y0 * blur_size + x1) * 4;
                                int idx01 = (y1 * blur_size + x0) * 4;
                                int idx11 = (y1 * blur_size + x1) * 4;

                                for (int c = 0; c < 4; c++) {
                                    float top_ch = blurBuffer[idx00 + c] * fx_inv + blurBuffer[idx10 + c] * fx;
                                    float bottom_ch = blurBuffer[idx01 + c] * fx_inv + blurBuffer[idx11 + c] * fx;
                                    outRow[x * 4 + c] = static_cast<BYTE>(top_ch * fy_inv + bottom_ch * fy);
                                }
                            }
                        }

                        blurred_artwork.UnlockBits(&outData);

                        Gdiplus::Graphics g(hdc);
                        g.DrawImage(&blurred_artwork, 0, 0, window_width, window_height);

                        Gdiplus::SolidBrush overlay(Gdiplus::Color(140, 0, 0, 0));
                        g.FillRectangle(&overlay, 0, 0, window_width, window_height);
                        bg_painted = true;
                    }
                }
            }
        }
    }

    bool is_dark = is_popup_dark_mode();

    if (!bg_painted) {
        COLORREF bg_color = is_dark ? RGB(45, 45, 48) : RGB(245, 245, 245);
        HBRUSH bg_brush = CreateSolidBrush(bg_color);
        FillRect(hdc, &client_rect, bg_brush);
        DeleteObject(bg_brush);
    }
    
    // Draw border
    COLORREF border_color = is_dark ? RGB(100, 100, 100) : RGB(200, 200, 200);
    HPEN border_pen = CreatePen(PS_SOLID, 1, border_color);
    HPEN old_pen = (HPEN)SelectObject(hdc, border_pen);
    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    
    Rectangle(hdc, 0, 0, client_rect.right, client_rect.bottom);
    
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_brush);
    DeleteObject(border_pen);
    
    bool show_art = get_show_cover_art();
    if (show_art) {
        bool has_margin = get_cover_margin();
        bool is_rounded = (get_cover_style() == 1);
        COLORREF placeholder_color = is_dark ? RGB(80, 80, 80) : RGB(220, 220, 220);
        
        RECT cover_rect = has_margin ? RECT{10, 10, 70, 70} : RECT{0, 0, window_height, window_height};
        int art_w = cover_rect.right - cover_rect.left;
        int art_h = cover_rect.bottom - cover_rect.top;

        if (is_rounded) {
            float radius = (art_w < art_h ? (float)art_w : (float)art_h) * 0.10f;
            if (radius < 4.0f) radius = 4.0f;
            if (radius > 12.0f) radius = 12.0f;
            float d = radius * 2.0f;

            Gdiplus::Bitmap offscreen(art_w, art_h, PixelFormat32bppPARGB);
            {
                Gdiplus::Graphics og(&offscreen);
                og.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                og.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

                if (m_cover_art_bitmap) {
                    Gdiplus::Bitmap srcBitmap(m_cover_art_bitmap, nullptr);
                    int srcW = srcBitmap.GetWidth();
                    int srcH = srcBitmap.GetHeight();
                    if (srcW > 0 && srcH > 0) {
                        float srcAspect = (float)srcW / (float)srcH;
                        float dstAspect = (float)art_w / (float)art_h;
                        int cropX = 0, cropY = 0, cropW = srcW, cropH = srcH;
                        if (srcAspect > dstAspect) {
                            cropH = srcH;
                            cropW = (int)(srcH * dstAspect);
                            cropX = (srcW - cropW) / 2;
                        } else {
                            cropW = srcW;
                            cropH = (int)(srcW / dstAspect);
                            cropY = (srcH - cropH) / 2;
                        }
                        Gdiplus::Rect destRect(0, 0, art_w, art_h);
                        og.DrawImage(&srcBitmap, destRect, cropX, cropY, cropW, cropH, Gdiplus::UnitPixel);
                    } else {
                        Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(placeholder_color), GetGValue(placeholder_color), GetBValue(placeholder_color)));
                        og.FillRectangle(&brush, 0, 0, art_w, art_h);
                    }
                } else {
                    Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(placeholder_color), GetGValue(placeholder_color), GetBValue(placeholder_color)));
                    og.FillRectangle(&brush, 0, 0, art_w, art_h);
                }
            }

            float x = (float)cover_rect.left;
            float y = (float)cover_rect.top;
            float fw = (float)art_w;
            float fh = (float)art_h;

            Gdiplus::GraphicsPath roundedPath;
            roundedPath.AddArc(x, y, d, d, 180, 90);
            roundedPath.AddArc(x + fw - d, y, d, d, 270, 90);
            roundedPath.AddArc(x + fw - d, y + fh - d, d, d, 0, 90);
            roundedPath.AddArc(x, y + fh - d, d, d, 90, 90);
            roundedPath.CloseFigure();

            Gdiplus::Graphics g(hdc);
            Gdiplus::TextureBrush texBrush(&offscreen, Gdiplus::WrapModeClamp);
            texBrush.TranslateTransform((float)cover_rect.left, (float)cover_rect.top);

            Gdiplus::SmoothingMode prevSmoothing = g.GetSmoothingMode();
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.FillPath(&texBrush, &roundedPath);
            g.SetSmoothingMode(prevSmoothing);
        } else {
            if (m_cover_art_bitmap) {
                HDC bitmap_dc = CreateCompatibleDC(hdc);
                HBITMAP old_bitmap = (HBITMAP)SelectObject(bitmap_dc, m_cover_art_bitmap);
                BITMAP bmp_info;
                GetObject(m_cover_art_bitmap, sizeof(BITMAP), &bmp_info);
                SetStretchBltMode(hdc, HALFTONE);
                StretchBlt(hdc, cover_rect.left, cover_rect.top, art_w, art_h,
                           bitmap_dc, 0, 0, bmp_info.bmWidth, bmp_info.bmHeight, SRCCOPY);
                SelectObject(bitmap_dc, old_bitmap);
                DeleteDC(bitmap_dc);
            } else {
                HBRUSH cover_brush = CreateSolidBrush(placeholder_color);
                FillRect(hdc, &cover_rect, cover_brush);
                DeleteObject(cover_brush);
            }
        }
    }
    
    // Draw track info (right side)
    draw_track_info(hdc, client_rect);
}

void popup_window::draw_track_info(HDC hdc, const RECT& client_rect) {
    if (!hdc) return;
    
    pfc::string8 title = m_current_title;
    pfc::string8 artist = m_current_artist;

    if (title.is_empty()) title = "Unknown Title";
    if (artist.is_empty()) artist = "Unknown Artist";
    
    // Setup text colors based on theme mode and background style
    bool is_dark = is_popup_dark_mode();
    int bg_style = get_background_style(); // 0 = Solid, 1 = Artwork Colors, 2 = Blurred Artwork
    
    COLORREF title_color = (bg_style == 0 && !is_dark) ? RGB(20, 20, 20) : RGB(255, 255, 255);
    COLORREF artist_color = (bg_style == 0 && !is_dark) ? RGB(80, 80, 80) : RGB(220, 220, 220);

    SetBkMode(hdc, TRANSPARENT);
    
    // Use Docked Control Panel fonts for consistency between popup and docked panel
    HFONT artist_font, title_font;
    
    if (get_cp_use_artist_custom_font()) {
        LOGFONT artist_lf = get_cp_artist_font();
        artist_font = CreateFontIndirect(&artist_lf);
    } else {
        artist_font = CreateFont(get_dpi_scaled_font_height(9), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    }

    if (get_cp_use_track_custom_font()) {
        LOGFONT title_lf = get_cp_track_font();
        title_font = CreateFontIndirect(&title_lf);
    } else {
        title_font = CreateFont(get_dpi_scaled_font_height(11), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    }
    
    bool show_art = get_show_cover_art();
    bool has_margin = get_cover_margin();
    int text_left = 15;
    if (show_art) {
        text_left = has_margin ? 85 : (client_rect.bottom - client_rect.top + 10);
    }

    // Draw title first (top line)
    HFONT old_font = (HFONT)SelectObject(hdc, title_font);
    SetTextColor(hdc, title_color);
    
    RECT title_rect = {text_left, 13, client_rect.right - 10, 39};
    pfc::stringcvt::string_wide_from_utf8 wide_title(title.c_str());
    DrawText(hdc, wide_title.get_ptr(), -1, &title_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    
    // Draw artist second (bottom line)
    SelectObject(hdc, artist_font);
    SetTextColor(hdc, artist_color);
    
    RECT artist_rect = {text_left, 44, client_rect.right - 10, 68};
    pfc::stringcvt::string_wide_from_utf8 wide_artist(artist.c_str());
    DrawText(hdc, wide_artist.get_ptr(), -1, &artist_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    
    // Cleanup fonts
    SelectObject(hdc, old_font);
    DeleteObject(artist_font);
    DeleteObject(title_font);
}

void popup_window::start_slide_in_animation() {
    if (m_popup_window) {
        KillTimer(m_popup_window, ANIMATION_TIMER_ID);
        KillTimer(m_popup_window, POPUP_TIMER_ID);
    }
    m_animating = false;

    // Determine if sliding from right or left based on popup position
    int popup_position = get_popup_position();
    if (popup_position < 0 || popup_position > 5) popup_position = 0;
    bool is_right_side = (popup_position >= 3);

    RECT work_area = {};
    if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0)) {
        work_area.left = 0;
        work_area.right = GetSystemMetrics(SM_CXSCREEN);
    }

    // Calculate start position (off-screen on the appropriate side)
    if (is_right_side) {
        m_start_x = work_area.right; // Start off-screen to the right
    } else {
        m_start_x = work_area.left - 320; // Start off-screen to the left (negative width)
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
    if (m_popup_window) {
        KillTimer(m_popup_window, ANIMATION_TIMER_ID);
        KillTimer(m_popup_window, POPUP_TIMER_ID);
    }
    m_animating = false;

    // Determine if sliding to right or left based on popup position
    int popup_position = get_popup_position();
    if (popup_position < 0 || popup_position > 5) popup_position = 0;
    bool is_right_side = (popup_position >= 3);

    RECT work_area = {};
    if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &work_area, 0)) {
        work_area.left = 0;
        work_area.right = GetSystemMetrics(SM_CXSCREEN);
    }

    // Get current position as start position
    RECT window_rect;
    GetWindowRect(m_popup_window, &window_rect);
    m_start_x = window_rect.left;
    m_start_y = window_rect.top;

    // Set final position (off-screen on the appropriate side)
    if (is_right_side) {
        m_final_x = work_area.right; // Exit off-screen to the right
    } else {
        m_final_x = work_area.left - 320; // Exit off-screen to the left
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
