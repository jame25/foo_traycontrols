#pragma once

#include "stdafx.h"

// Control panel popup window class
class control_panel {
public:
    static control_panel& get_instance();
    
    // Lifecycle management
    void initialize();
    void cleanup();
    
    // Show/hide control panel
    void show_control_panel(bool force_docked = false);
    void show_control_panel_simple(); // Simple popup behavior like original
    void hide_control_panel();
    void hide_control_panel_immediate(); // Hide without animation
    void toggle_control_panel();
    
    // Update with current track info
    void update_track_info();
    
    // Settings change notification
    void on_settings_changed();
    
    // Public accessors for tray manager
    bool is_undocked() const { return m_is_undocked; }
    bool is_artwork_expanded() const { return m_is_artwork_expanded; }
    HWND get_control_window() const { return m_control_window; }
    void set_undocked(bool undocked);
    void toggle_artwork_expanded();
    void toggle_compact_mode();
    
private:
    control_panel();
    ~control_panel();
    
    // Member variables
    HWND m_control_window;
    bool m_initialized;
    bool m_visible;
    
    // Control button IDs
    static const int BTN_MENU = 1001;
    static const int BTN_PREV = 1002;
    static const int BTN_PLAYPAUSE = 1003;
    static const int BTN_NEXT = 1004;
    static const int BTN_STOP = 1005;
    
    // Timer for updating time display
    static const UINT UPDATE_TIMER_ID = 4001;
    static const UINT ANIMATION_TIMER_ID = 4003;
    static const UINT BUTTON_FADE_TIMER_ID = 9001;
    
    // Current track info
    pfc::string8 m_current_artist;
    pfc::string8 m_current_title;
    double m_current_time;
    double m_track_length;
    bool m_is_playing;
    bool m_is_paused;
    bool m_is_undocked;
    bool m_is_artwork_expanded;
    
    // Double-click detection for artwork expanded mode
    DWORD m_last_click_time;
    POINT m_last_click_pos;
    
    // Dimension memory for mode switching
    int m_saved_undocked_width;
    int m_saved_undocked_height;
    int m_saved_expanded_width;
    int m_saved_expanded_height;
    
    // Mouse hover state for overlay
    bool m_overlay_visible;
    DWORD m_last_mouse_move_time;
    int m_overlay_opacity; // 0-100 for fade animation
    DWORD m_fade_start_time;
    
    // Undocked mode artwork hover overlay
    bool m_undocked_overlay_visible;
    int m_undocked_overlay_opacity; // 0-100 for fade animation
    DWORD m_undocked_fade_start_time;
    
    // Manual dragging state for expanded artwork mode
    bool m_is_dragging;
    POINT m_drag_start_pos;
    
    // Button fade state for undocked mode
    bool m_buttons_visible;
    int m_button_opacity; // 0-100 for fade animation
    DWORD m_button_fade_start_time;
    DWORD m_last_button_mouse_time;
    
    // Compact mode state
    bool m_is_compact_mode;
    int m_saved_normal_width;
    int m_saved_normal_height;
    int m_saved_compact_width; // Remember compact mode width when resized
    bool m_was_compact_before_expanded; // Remember if we were in compact mode before entering expanded mode
    
    // Compact mode hover control overlay
    bool m_compact_controls_visible;
    DWORD m_last_compact_mouse_time;
    
    
    // Roll-up/roll-down animation state
    bool m_is_rolling_animation;
    bool m_rolling_to_compact; // true = rolling to compact, false = rolling to normal
    int m_roll_animation_step;
    DWORD m_roll_animation_start_time;
    static const int ROLL_ANIMATION_STEPS = 15;
    static const int ROLL_ANIMATION_DURATION = 250; // ms
    
    
    // Album art
    HBITMAP m_cover_art_bitmap;
    HBITMAP m_cover_art_bitmap_large; // High quality version for expanded view
    HBITMAP m_cover_art_bitmap_original; // Full resolution original for expanded view
    int m_original_art_width;
    int m_original_art_height;
    
    // Custom fonts
    HFONT m_artist_font;
    HFONT m_track_font;
    
    // Vector icon drawing
    void draw_play_icon(HDC hdc, int x, int y, int size);
    void draw_pause_icon(HDC hdc, int x, int y, int size);
    void draw_previous_icon(HDC hdc, int x, int y, int size);
    void draw_next_icon(HDC hdc, int x, int y, int size);
    void draw_play_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity);
    void draw_pause_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity);
    void draw_previous_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity);
    void draw_next_icon_with_opacity(HDC hdc, int x, int y, int size, int opacity);
    void draw_up_arrow(HDC hdc, int x, int y, int size);
    void draw_down_arrow(HDC hdc, int x, int y, int size);
    void draw_up_arrow_with_opacity(HDC hdc, int x, int y, int size, int opacity);
    void draw_down_arrow_with_opacity(HDC hdc, int x, int y, int size, int opacity);
    void draw_roll_dots(HDC hdc, int x, int y, int size);
    void start_roll_animation(bool to_compact);
    void update_roll_animation();
    
    // Animation state
    bool m_animating;
    bool m_closing;
    int m_animation_step;
    int m_start_x, m_start_y;
    int m_final_x, m_final_y;
    static const int ANIMATION_STEPS = 20;
    static const int ANIMATION_DURATION = 300; // ms
    
    // Window management
    void create_control_window();
    void position_control_panel();
    void create_controls();
    void update_play_button();
    void load_cover_art();
    void cleanup_cover_art();
    HBITMAP convert_album_art_to_bitmap(album_art_data_ptr art_data);
    HBITMAP convert_album_art_to_bitmap_large(album_art_data_ptr art_data);
    HBITMAP convert_album_art_to_bitmap_original(album_art_data_ptr art_data);
    void load_fonts();
    void cleanup_fonts();
    
    // Event handlers
    void handle_button_click(int button_id);
    void handle_timer();
    
    // Animation
    void start_slide_out_animation();
    void update_animation();
    
    // Window procedure
    static LRESULT CALLBACK control_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    
    // Drawing
    void paint_control_panel(HDC hdc);
    void paint_artwork_expanded(HDC hdc, const RECT& rect);
    void paint_compact_mode(HDC hdc, const RECT& rect);
    void draw_track_info(HDC hdc, const RECT& rect, int art_size = 80);
    void draw_time_info(HDC hdc, const RECT& rect);
    void draw_track_info_overlay(HDC hdc, int window_width, int window_height);
    void draw_control_overlay(HDC hdc, int window_width, int window_height);
    void draw_undocked_artwork_overlay(HDC hdc, int window_width, int window_height);
    void draw_compact_control_overlay(HDC hdc, int window_width, int window_height);
    
    static control_panel* s_instance;
};
