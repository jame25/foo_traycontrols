#pragma once

#include "stdafx.h"
#include "resource.h"

// Configuration access functions
bool get_always_minimize_to_tray();
bool get_double_click_actions();
bool get_show_popup_notification();
int get_popup_position();
bool get_disable_miniplayer();
int get_popup_duration(); // Returns popup duration in milliseconds (1000-10000)
bool get_disable_slide_to_side();
int get_slide_duration(); // Returns slide duration in milliseconds
bool get_use_rounded_corners(); // Windows 11 style rounded corners
bool get_always_slide_to_side(); // Always slide instead of close
int get_theme_mode(); // 0=Auto, 1=Force Dark, 2=Force Light
bool get_show_cover_art(); // true=Yes, false=No
bool get_cover_margin(); // true=Yes, false=No
int get_cover_style(); // 0=Square, 1=Rounded
int get_background_style(); // 0=Solid, 1=Artwork Colors, 2=Blurred Artwork
int get_miniplayer_border_style(); // 0=Square, 1=Rounded
int get_ticker_speed(); // 0=Off, 1=Slowest, 2=Slow, 3=Fast, 4=Fastest
bool get_show_volume_feedback(); // true=Enabled, false=Disabled
bool get_hover_circles_enabled(); // true=Show (default), false=Hide
int get_alternative_icons_style(); // 0=Style 1 (default), 1=Style 2, 2=Style 3
COLORREF get_compact_progress_color(); // Compact MiniPlayer track progress bar fill color
COLORREF get_volume_osd_color();       // Volume OSD bar fill color

// MiniPlayer mode size functions
int get_miniplayer_undocked_width();
int get_miniplayer_undocked_height();
int get_miniplayer_compact_width();
int get_miniplayer_compact_height();
int get_miniplayer_expanded_size();

// Display format functions
pfc::string8 get_line1_format();
pfc::string8 get_line2_format();
void format_display_lines(pfc::string8& line1_out, pfc::string8& line2_out);
void format_display_lines_track(metadb_handle_ptr track, pfc::string8& line1_out, pfc::string8& line2_out);

// Opens the Preferences dialog at this component's page (General tab is shown first)
void show_preferences_page();



// Font configuration functions
bool get_use_custom_fonts();
bool get_use_artist_custom_font();
bool get_use_track_custom_font();
LOGFONT get_artist_font();
LOGFONT get_track_font();
void set_artist_font(const LOGFONT& font);
void set_track_font(const LOGFONT& font);
void reset_fonts();
LOGFONT get_default_font(bool is_artist, int size);

// Docked Control Panel font configuration functions
bool get_cp_use_custom_fonts();
bool get_cp_use_artist_custom_font();
bool get_cp_use_track_custom_font();
LOGFONT get_cp_artist_font();
LOGFONT get_cp_track_font();
void set_cp_artist_font(const LOGFONT& font);
void set_cp_track_font(const LOGFONT& font);
void reset_cp_fonts();

// Undocked mode font configuration functions
bool get_undocked_use_custom_fonts();
bool get_undocked_use_artist_custom_font();
bool get_undocked_use_track_custom_font();
LOGFONT get_undocked_artist_font();
LOGFONT get_undocked_track_font();

// Expanded mode font configuration functions  
bool get_expanded_use_custom_fonts();
bool get_expanded_use_artist_custom_font();
bool get_expanded_use_track_custom_font();
LOGFONT get_expanded_artist_font();
LOGFONT get_expanded_track_font();

// Compact mode font configuration functions
bool get_compact_use_custom_fonts();
bool get_compact_use_artist_custom_font();
bool get_compact_use_track_custom_font();
LOGFONT get_compact_artist_font();
LOGFONT get_compact_track_font();

// Timer font configuration functions (shared across modes)
bool get_timer_use_custom_font();
LOGFONT get_timer_font();

// Preferences page instance - the actual dialog
class tray_preferences : public preferences_page_instance {
private:
    HWND m_hwnd;
    preferences_page_callback::ptr m_callback;
    bool m_has_changes;
    fb2k::CCoreDarkModeHooks m_darkMode;
    int m_current_tab; // 0 = General, 1 = Appearance, 2 = Icons, 3 = MiniPlayer, 4 = Fonts
    
public:
    tray_preferences(preferences_page_callback::ptr callback);
    
    // preferences_page_instance implementation
    HWND get_wnd() override;
    t_uint32 get_state() override;
    void apply() override;
    void reset() override;
    
    // Dialog procedure
    static INT_PTR CALLBACK ConfigProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    
private:
    void on_changed();
    bool has_changed();
    void apply_settings();
    void reset_settings();
    void update_font_displays();
    void select_artist_font();
    void select_track_font();
    void reset_fonts_to_default();
    void select_cp_artist_font();
    void select_cp_track_font();
    void reset_cp_fonts_to_default();
    pfc::string8 format_font_name(const LOGFONT& lf);
    
    // Tab control methods
    void init_tab_control();
    void switch_tab(int tab);
    void select_font_for_mode(int mode, bool is_artist); // mode: 1=undocked, 2=expanded, 3=compact
    void select_timer_font();
    void reset_all_fonts_to_default();
};

// Preferences page factory
class tray_preferences_page : public preferences_page_v3 {
public:
    const char* get_name() override;
    GUID get_guid() override;
    GUID get_parent_guid() override;
    preferences_page_instance::ptr instantiate(HWND parent, preferences_page_callback::ptr callback) override;
};
