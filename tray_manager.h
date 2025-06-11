#pragma once

#include "stdafx.h"
#include "resource.h"

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
    void update_playback_state(const char* state);
    
    // Tray functionality - public interface
    void minimize_to_tray();
    void restore_from_tray();
    
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
    WNDPROC m_original_wndproc;
    
    // Timer for periodic tooltip updates
    static const UINT TOOLTIP_TIMER_ID = 2001;
    pfc::string8 m_last_track_path;
    
    // Window management
    HWND find_main_window();
    bool create_tray_window();
    static BOOL CALLBACK find_window_callback(HWND hwnd, LPARAM lparam);
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK tray_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    
    // Tray functionality
    void show_context_menu(int x, int y);
    void handle_menu_command(int cmd);
    void force_update_tooltip();
    void check_for_track_changes();
    void check_window_visibility();
    static VOID CALLBACK tooltip_timer_proc(HWND hwnd, UINT msg, UINT_PTR timer_id, DWORD time);
};