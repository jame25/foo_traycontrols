// main.cpp - Entry point for the Foobar2000 Tray Controls component

#include "stdafx.h"
#include "tray_manager.h"
#include "preferences.h"
#include "popup_window.h"
#include "control_panel.h"
#include "artwork_bridge.h"

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
    "1.4.7",
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

static ULONG_PTR g_gdiplusToken = 0;

// Metadb callback to update control panel, popup window, and tray tooltip when stream metadata or display fields change
class tray_metadb_callback : public metadb_io_callback_dynamic_impl_base {
public:
    void on_changed_sorted(metadb_handle_list_cref p_items_sorted, bool p_fromhook) override {
        auto playback = playback_control::get();
        if (!playback->is_playing() && !playback->is_paused()) return;

        metadb_handle_ptr track;
        if (playback->get_now_playing(track) && track.is_valid()) {
            if (metadb_handle_list_helper::bsearch_by_pointer(p_items_sorted, track) != pfc_infinite) {
                control_panel::get_instance().update_track_info(track);
                popup_window::get_instance().update_track_info(track);
                tray_manager::get_instance().update_tooltip(track);
            }
        }
    }
};

static std::unique_ptr<tray_metadb_callback> g_metadb_callback;

// Tray Controls initialization handler
class tray_init : public initquit {
public:
    void on_init() override {
        // Initialize GDI+ globally
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);

        // Initialize the popup window and tray manager
        popup_window::get_instance().initialize();
        tray_manager::get_instance().initialize();
        // Initialize foo_artwork bridge for online artwork support
        init_artwork_bridge();
        // Initialize metadb callback for dynamic stream metadata updates
        g_metadb_callback = std::make_unique<tray_metadb_callback>();
    }

    void on_quit() override {
        // Destroy metadb callback
        g_metadb_callback.reset();
        // Unregister foo_artwork callback before other cleanup
        shutdown_artwork_bridge();
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
        control_panel::get_instance().update_track_info(p_track);
        // Show popup notification for new tracks
        popup_window::get_instance().show_track_info(p_track);
    }
    
    void on_playback_starting(play_control::t_track_command p_command, bool p_paused) override {}
    
    void on_playback_pause(bool p_state) override {
        // Update tray tooltip to show pause state
        tray_manager::get_instance().update_playback_state(p_state ? "Paused" : "Playing");
        // Update control panel playback state
        control_panel::get_instance().update_track_info();
    }
    
    void on_playback_stop(play_control::t_stop_reason p_reason) override {
        if (p_reason == play_control::stop_reason_starting_another) {
            return;
        }
        // Update tray tooltip to show stopped state
        tray_manager::get_instance().update_playback_state("Stopped");
        // Update control panel playback state
        control_panel::get_instance().update_track_info();
        popup_window::get_instance().hide_popup();
    }
    
    // Required overrides for play_callback_static
    void on_playback_seek(double p_time) override {}
    void on_playback_edited(metadb_handle_ptr p_track) override {
        // Update tooltip when track metadata is edited
        tray_manager::get_instance().update_tooltip(p_track);
        // Update control panel when track metadata is edited
        control_panel::get_instance().update_track_info(p_track);
    }
    void on_playback_dynamic_info(const file_info & p_info) override {
        tray_manager::get_instance().update_tooltip_with_dynamic_info(p_info);
        control_panel::get_instance().update_stream_metadata(p_info);
        popup_window::get_instance().update_stream_metadata(p_info);
    }
    void on_playback_dynamic_info_track(const file_info & p_info) override {
        tray_manager::get_instance().update_tooltip_with_dynamic_info(p_info);
        control_panel::get_instance().update_stream_metadata(p_info);
        popup_window::get_instance().update_stream_metadata(p_info);
    }
    void on_playback_time(double p_time) override {}
    void on_volume_change(float p_new_val) override {}
    unsigned get_flags() override {
        return flag_on_playback_starting | flag_on_playback_new_track | 
               flag_on_playback_stop | flag_on_playback_pause | 
               flag_on_playback_edited | flag_on_playback_dynamic_info | 
               flag_on_playback_dynamic_info_track;
    }
};

// Theme change callback to update control panel when dark mode is toggled
// This inherits from ui_config_callback_impl which auto-registers/unregisters
class theme_change_callback : public ui_config_callback_impl {
public:
    void ui_colors_changed() override {
        // Immediately update theme colors when dark mode is toggled in foobar2000
        control_panel::get_instance().on_settings_changed();
        // Also update popup window if it's visible
        popup_window::get_instance().on_settings_changed();
    }
    
    void ui_fonts_changed() override {
        // Also handle font changes (just in case)
        control_panel::get_instance().on_settings_changed();
    }
};

// Theme callback instance - must be instantiated after services are available
static std::unique_ptr<theme_change_callback> g_theme_callback;

// Theme callback initializer - creates the callback after services are available
class theme_callback_init : public initquit {
public:
    void on_init() override {
        // Create theme change callback
        g_theme_callback = std::make_unique<theme_change_callback>();
    }
    
    void on_quit() override {
        // Destroy theme change callback
        g_theme_callback.reset();
    }
};

// Service factory registrations
static initquit_factory_t<tray_init> g_tray_init_factory;
static initquit_factory_t<theme_callback_init> g_theme_callback_init_factory;
static play_callback_static_factory_t<tray_play_callback> g_tray_play_callback_factory;
