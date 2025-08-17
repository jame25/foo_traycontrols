// main.cpp - Entry point for the Foobar2000 Tray Controls component

#include "stdafx.h"
#include "tray_manager.h"
#include "preferences.h"
#include "popup_window.h"
#include "control_panel.h"

// Component's DLL instance handle
HINSTANCE g_hIns = NULL;

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_hIns = hModule;
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// Component version declaration using the proper SDK macro
DECLARE_COMPONENT_VERSION(
    "Tray Controls",
    "1.1.6",
    "System tray controls for foobar2000.\n"
    "Features:\n"
    "- Minimize to system tray\n"
    "- Right-click context menu with playback controls\n"
    "- Track information tooltips\n\n"
    "Author: jame25\n"
    "Build date: " __DATE__ "\n\n"
    "This component adds system tray functionality to foobar2000."
);

// Validate component compatibility using the proper SDK macro
VALIDATE_COMPONENT_FILENAME("foo_traycontrols.dll");

// Tray Controls initialization handler
class tray_init : public initquit {
public:
    void on_init() override {
        // Initialize the tray manager
        tray_manager::get_instance().initialize();
    }
    
    void on_quit() override {
        // Clean up the tray manager, popup window, and control panel
        tray_manager::get_instance().cleanup();
        popup_window::get_instance().cleanup();
        control_panel::get_instance().cleanup();
    }
};

// Since app_close_blocker doesn't work as expected, let's remove it and just 
// focus on making the minimize to tray functionality work well
// The "minimize on close" feature appears to be impossible to implement reliably 
// with the current foobar2000 SDK architecture

// Playback callback to update tray tooltips with current track info
class tray_play_callback : public play_callback_static {
public:
    void on_playback_new_track(metadb_handle_ptr p_track) override {
        // Update tray tooltip with new track information
        tray_manager::get_instance().update_tooltip(p_track);
        // Update control panel with new track information
        control_panel::get_instance().update_track_info();
        // Show popup notification for new tracks
        popup_window::get_instance().show_track_info(p_track);
    }
    
    void on_playback_starting(play_control::t_track_command p_command, bool p_paused) override {
        // Also update when playback starts
        static_api_ptr_t<playback_control> pc;
        metadb_handle_ptr track;
        if (pc->get_now_playing(track) && track.is_valid()) {
            tray_manager::get_instance().update_tooltip(track);
            control_panel::get_instance().update_track_info();
        }
    }
    
    void on_playback_pause(bool p_state) override {
        // Update tray tooltip to show pause state
        tray_manager::get_instance().update_playback_state(p_state ? "Paused" : "Playing");
        // Update control panel playback state
        control_panel::get_instance().update_track_info();
    }
    
    void on_playback_stop(play_control::t_stop_reason p_reason) override {
        // Update tray tooltip to show stopped state
        tray_manager::get_instance().update_playback_state("Stopped");
        // Update control panel playback state
        control_panel::get_instance().update_track_info();
    }
    
    // Required overrides for play_callback_static
    void on_playback_seek(double p_time) override {}
    void on_playback_edited(metadb_handle_ptr p_track) override {
        // Update tooltip when track metadata is edited
        tray_manager::get_instance().update_tooltip(p_track);
        // Update control panel when track metadata is edited
        control_panel::get_instance().update_track_info();
    }
    void on_playback_dynamic_info(const file_info & p_info) override {
        // Update tooltip with dynamic metadata info (for streaming sources)
        tray_manager::get_instance().update_tooltip_with_dynamic_info(p_info);
        // Update control panel with dynamic info (for streaming sources)
        control_panel::get_instance().update_track_info();
        // Note: Do NOT trigger popup here - this fires too frequently for streams
        // Popup will be handled by track change detection in tray_manager timer
    }
    void on_playback_dynamic_info_track(const file_info & p_info) override {
        // Update tooltip with track-specific dynamic info (for streaming sources)
        tray_manager::get_instance().update_tooltip_with_dynamic_info(p_info);
        // Update control panel with track-specific dynamic info (for streaming sources)
        control_panel::get_instance().update_track_info();
        // Note: Do NOT trigger popup here - this fires too frequently for streams
        // Popup will be handled by track change detection in tray_manager timer
    }
    void on_playback_time(double p_time) override {}
    void on_volume_change(float p_new_val) override {}
    unsigned get_flags() override { return 0; }
};

// Service factory registrations
static initquit_factory_t<tray_init> g_tray_init_factory;
static play_callback_static_factory_t<tray_play_callback> g_tray_play_callback_factory;
