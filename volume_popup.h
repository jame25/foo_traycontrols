#pragma once

#include "stdafx.h"

class volume_popup {
public:
    static volume_popup& get_instance();

    void initialize();
    void cleanup();

    // Show popup centered at x, y (bottom center of the popup will be at x, y)
    // Or anchored to a button position
    void show_at(int x, int y);
    void hide();
    bool is_visible() const { return m_visible; }

private:
    volume_popup();
    ~volume_popup();

    static volume_popup* s_instance;

    HWND m_window;
    bool m_initialized;
    bool m_visible;
    bool m_is_dragging;
    
    // UI state
    float m_current_volume_db; // Current volume in dB
    int m_hover_level; // 0-100 for slider hover effect

    // Visual constants
    // Visual constants
    static const int POPUP_WIDTH = 150;
    static const int POPUP_HEIGHT = 50; // increased for bubble arrow
    static const int SLIDER_MARGIN_X = 20; // Left/Right margin for slider track
    static const int SLIDER_MARGIN_Y = 15; // Top/Bottom margin (centering)
    static const int THUMB_SIZE = 14;
    static const int ARROW_HEIGHT = 8;
    static const int ARROW_WIDTH = 16;
    static const int CORNER_RADIUS = 8;

    void create_window();
    void register_class();
    void paint(HDC hdc);
    void update_volume_from_point(POINT pt);
    
    // Timer IDs
    static const UINT UPDATE_TIMER_ID = 5001;
    static const UINT FADE_TIMER_ID = 5002;

    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
};
