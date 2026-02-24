#pragma once

#include "stdafx.h"

// Popup notification window class
class popup_window {
public:
    static popup_window& get_instance();
    
    // Lifecycle management
    void initialize();
    void cleanup();
    
    // Show popup with track information
    void show_track_info(metadb_handle_ptr p_track);
    void hide_popup();
    void refresh_track_info();
    
    // Settings
    void on_settings_changed();
    
private:
    popup_window();
    ~popup_window();
    
    // Member variables
    HWND m_popup_window;
    HWND m_cover_art_window;
    HWND m_artist_label;
    HWND m_title_label;
    bool m_initialized;
    bool m_visible;
    bool m_animating;
    bool m_sliding_in;
    int m_animation_step;
    int m_final_x, m_final_y;
    int m_start_x, m_start_y;
    
    // Timer for auto-hide and animation
    static const UINT POPUP_TIMER_ID = 3001;
    static const UINT ANIMATION_TIMER_ID = 3002;
    // POPUP_DISPLAY_TIME is now configurable via get_popup_duration() in preferences
    static const UINT ANIMATION_DURATION = 300; // 300ms slide animation
    static const UINT ANIMATION_STEPS = 20; // Number of animation frames
    static const UINT ARTWORK_POLL_TIMER_ID = 3003;
    static const UINT ARTWORK_POLL_INTERVAL = 200; // ms
    
    // Cover art and track info
    HBITMAP m_cover_art_bitmap;
    bool m_artwork_from_bridge; // true if m_cover_art_bitmap is owned by foo_artwork (do NOT DeleteObject)
    pfc::string8 m_last_track_path;
    metadb_handle_ptr m_current_track;
    
    // Window management
    void create_popup_window();
    void position_popup();
    void update_track_info(metadb_handle_ptr p_track);
    void load_cover_art(metadb_handle_ptr p_track);
    void cleanup_cover_art();
    HBITMAP convert_album_art_to_bitmap(album_art_data_ptr art_data);
    
    // Animation
    void start_slide_in_animation();
    void start_slide_out_animation();
    void update_animation();
    
    // Window procedure
    static LRESULT CALLBACK popup_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    static VOID CALLBACK hide_timer_proc(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time);
    static VOID CALLBACK animation_timer_proc(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time);
    
    // Drawing
    void paint_popup(HDC hdc);
    void draw_cover_art(HDC hdc, const RECT& rect);
    void draw_track_info(HDC hdc, const RECT& rect);
    
    static popup_window* s_instance;
};
