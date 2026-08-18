#pragma once

#include "stdafx.h"
#include "resource.h"
#include "popup_window.h"
#include "control_panel.h"

// Tray manager class - singleton that handles all tray functionality
class tray_manager {
public:
    // Singleton access
    static tray_manager& get_instance();
    
    // Lifecycle management
    void initialize();
    void cleanup();
    
    // Playback event handlers
    void update_tooltip(metadb_handle_ptr p_track);
    void update_tooltip_from_playback();
    void update_tooltip_with_dynamic_info(const file_info & p_info);
    void update_playback_state(const char* state);
    
    // Tray functionality - public interface
    void minimize_to_tray();
    void restore_from_tray();
    void on_settings_changed();
    
    static tray_manager* s_instance;

private:
    tray_manager();
    ~tray_manager();
    
    // Member variables
    HWND m_main_window;
    HWND m_tray_window;
    NOTIFYICONDATA m_nid;
    bool m_tray_added;
    bool m_initialized;
    bool m_was_visible;
    bool m_was_minimized;
    bool m_processing_minimize;
    bool m_ignore_next_lbuttonup;
    DWORD m_last_dblclk_time;
    WNDPROC m_original_wndproc;
    // Mouse hook removed - was causing conflicts with artwork downloading
    
    // Timer for periodic tooltip updates and single-click disambiguation
    static const UINT TOOLTIP_TIMER_ID = 2001;
    static const UINT TRAY_SINGLE_CLICK_TIMER_ID = 2002;
    pfc::string8 m_last_track_path;
    pfc::string8 m_last_track_metadata;
    pfc::string8 m_last_stream_artist;
    pfc::string8 m_last_stream_title;
    metadb_handle_ptr m_last_loaded_track;

    // Low-level mouse hook for volume wheel control over tray icon
    static HHOOK s_mouse_hook;
    static LRESULT CALLBACK low_level_mouse_proc(int nCode, WPARAM wParam, LPARAM lParam);
    
    // Window management
    HWND find_main_window();
    bool create_tray_window();
    static BOOL CALLBACK find_window_callback(HWND hwnd, LPARAM lparam);
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK tray_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    
    // Tray functionality
    void execute_single_click();
    void show_context_menu(int x, int y);
    void handle_menu_command(int cmd);
    bool is_cursor_over_tray_icon();
    // Mouse wheel and hook functions removed - were causing conflicts with artwork downloading
    void force_update_tooltip();
    void check_for_track_changes();
    void check_window_visibility();
    static VOID CALLBACK tooltip_timer_proc(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time);
    static VOID CALLBACK single_click_timer_proc(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time);
};
