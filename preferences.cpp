#include "stdafx.h"
#include "preferences.h"
#include "tray_manager.h"
#include "control_panel.h"
#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

// External declaration from main.cpp
extern HINSTANCE g_hIns;

// Configuration variables - stored in foobar2000's config system
static cfg_int cfg_always_minimize_to_tray(GUID{0x12345679, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);
static cfg_int cfg_show_popup_notification(GUID{0x12345681, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 1);
static cfg_int cfg_popup_position(GUID{0x12345685, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0); // 0=Top Left, 1=Middle Left, 2=Bottom Left, 3=Top Right, 4=Middle Right, 5=Bottom Right
static cfg_int cfg_disable_miniplayer(GUID{0x12345686, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);
static cfg_int cfg_popup_duration(GUID{0x12345687, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 3000); // Default 3 seconds (3000ms)
static cfg_int cfg_disable_slide_to_side(GUID{0x12345688, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);
static cfg_int cfg_slide_duration(GUID{0x12345689, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 200); // Default 200ms
static cfg_int cfg_always_slide_to_side(GUID{0x1234568A, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 1); // Default ON
static cfg_int cfg_use_rounded_corners(GUID{0x12345690, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 1); // Default ON (Win11 style)
static cfg_int cfg_theme_mode(GUID{0x12345691, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0); // 0=Auto, 1=Force Dark, 2=Force Light
static cfg_int cfg_show_cover_art(GUID{0x12345695, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 1); // 1=Yes, 0=No
static cfg_int cfg_cover_margin(GUID{0x12345696, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 1); // 1=Yes, 0=No
static cfg_int cfg_cover_style(GUID{0x12345697, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0); // 0=Square, 1=Rounded
static cfg_int cfg_background_style(GUID{0x12345698, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0); // 0=Solid, 1=Artwork Colors, 2=Blurred Artwork
static cfg_int cfg_show_volume_feedback(GUID{0x12345699, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 1); // 1=Yes, 0=No
static cfg_int cfg_miniplayer_border_style(GUID{0x1234569A, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 1); // 0=Square, 1=Rounded (Default Rounded)

// MiniPlayer mode size configuration
static cfg_int cfg_miniplayer_undocked_width(GUID{0x123456D1, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 400);
static cfg_int cfg_miniplayer_undocked_height(GUID{0x123456D2, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 120);
static cfg_int cfg_miniplayer_compact_width(GUID{0x123456D3, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 320);
static cfg_int cfg_miniplayer_compact_height(GUID{0x123456D4, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 75);
static cfg_int cfg_miniplayer_expanded_size(GUID{0x123456D5, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 350);

// Display format configuration
static cfg_string cfg_line1_format(GUID{0x123456E0, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, "%title%");
static cfg_string cfg_line2_format(GUID{0x123456E1, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, "%artist%");

static bool s_ignore_edit_change = false;


// Font configuration - store LOGFONT structure as binary data
static cfg_struct_t<LOGFONT> cfg_artist_font(GUID{0x12345692, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -12; // 9pt at 96 DPI
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    return lf;
}());
static cfg_struct_t<LOGFONT> cfg_track_font(GUID{0x12345693, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -15; // 11pt at 96 DPI
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    return lf;
}());
static cfg_int cfg_use_artist_custom_font(GUID{0x12345694, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);
static cfg_int cfg_use_track_custom_font(GUID{0x1234569C, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);

// Control Panel specific font configuration
static cfg_struct_t<LOGFONT> cfg_cp_artist_font(GUID{0x1234569A, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -12; // 9pt at 96 DPI
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    return lf;
}());
static cfg_struct_t<LOGFONT> cfg_cp_track_font(GUID{0x1234569B, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -15; // 11pt at 96 DPI
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    return lf;
}());
static cfg_int cfg_cp_use_artist_custom_font(GUID{0x1234569D, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);
static cfg_int cfg_cp_use_track_custom_font(GUID{0x1234569E, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);

// Undocked mode font configuration
static cfg_struct_t<LOGFONT> cfg_undocked_artist_font(GUID{0x123456A0, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -12; // 9pt at 96 DPI
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    return lf;
}());
static cfg_struct_t<LOGFONT> cfg_undocked_track_font(GUID{0x123456A1, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -15; // 11pt at 96 DPI
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    return lf;
}());
static cfg_int cfg_undocked_use_artist_custom_font(GUID{0x123456A2, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);
static cfg_int cfg_undocked_use_track_custom_font(GUID{0x123456A3, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);

// Expanded mode font configuration
static cfg_struct_t<LOGFONT> cfg_expanded_artist_font(GUID{0x123456B0, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -12; // 9pt at 96 DPI
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    return lf;
}());
static cfg_struct_t<LOGFONT> cfg_expanded_track_font(GUID{0x123456B1, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -15; // 11pt at 96 DPI
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    return lf;
}());
static cfg_int cfg_expanded_use_artist_custom_font(GUID{0x123456B2, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);
static cfg_int cfg_expanded_use_track_custom_font(GUID{0x123456B3, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);

// Compact mode font configuration
static cfg_struct_t<LOGFONT> cfg_compact_artist_font(GUID{0x123456C0, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -12; // 9pt at 96 DPI
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    return lf;
}());
static cfg_struct_t<LOGFONT> cfg_compact_track_font(GUID{0x123456C1, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -15; // 11pt at 96 DPI
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    return lf;
}());
static cfg_int cfg_compact_use_artist_custom_font(GUID{0x123456C2, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);
static cfg_int cfg_compact_use_track_custom_font(GUID{0x123456C3, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);

// Timer font configuration (shared across Docked, Undocked, Compact modes)
static cfg_struct_t<LOGFONT> cfg_timer_font(GUID{0x123456D0, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -12; // 9pt at 96 DPI
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    return lf;
}());
static cfg_int cfg_timer_use_custom_font(GUID{0x123456D1, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);

// Access functions for the configuration
bool get_always_minimize_to_tray() {
    return cfg_always_minimize_to_tray != 0;
}

// Mouse wheel volume control removed - was causing system conflicts

bool get_show_popup_notification() {
    return cfg_show_popup_notification != 0;
}

int get_popup_position() {
    return cfg_popup_position;
}

bool get_disable_miniplayer() {
    return cfg_disable_miniplayer != 0;
}

int get_popup_duration() {
    int duration = cfg_popup_duration;
    // Clamp to valid range (1-10 seconds)
    if (duration < 1000) duration = 1000;
    if (duration > 10000) duration = 10000;
    return duration;
}

bool get_disable_slide_to_side() {
    return cfg_disable_slide_to_side != 0;
}

int get_slide_duration() {
    return cfg_slide_duration;
}

bool get_use_rounded_corners() {
    return cfg_use_rounded_corners != 0;
}

bool get_always_slide_to_side() {
    return cfg_always_slide_to_side != 0;
}

int get_theme_mode() {
    int mode = cfg_theme_mode;
    // Clamp to valid range (0=Auto, 1=Force Dark, 2=Force Light)
    if (mode < 0) mode = 0;
    if (mode > 2) mode = 2;
    return mode;
}

bool get_show_cover_art() {
    return cfg_show_cover_art != 0;
}

bool get_cover_margin() {
    return cfg_cover_margin != 0;
}

int get_cover_style() {
    int style = cfg_cover_style;
    if (style < 0 || style > 1) style = 0;
    return style;
}

int get_background_style() {
    int style = cfg_background_style;
    if (style < 0 || style > 2) style = 0;
    return style;
}

int get_miniplayer_border_style() {
    int style = cfg_miniplayer_border_style;
    if (style < 0 || style > 1) style = 1;
    return style;
}

bool get_show_volume_feedback() {
    return cfg_show_volume_feedback != 0;
}

pfc::string8 get_line1_format() {
    return cfg_line1_format.get();
}

pfc::string8 get_line2_format() {
    return cfg_line2_format.get();
}

void format_display_lines(pfc::string8& line1_out, pfc::string8& line2_out) {
    try {
        auto playback = playback_control::get();
        static_api_ptr_t<titleformat_compiler> compiler;

        pfc::string8 line1_fmt = get_line1_format();
        pfc::string8 line2_fmt = get_line2_format();

        service_ptr_t<titleformat_object> script;
        if (compiler->compile(script, line1_fmt)) {
            playback->playback_format_title(nullptr, line1_out, script, nullptr, playback_control::display_level_all);
        }
        if (compiler->compile(script, line2_fmt)) {
            playback->playback_format_title(nullptr, line2_out, script, nullptr, playback_control::display_level_all);
        }
    } catch (...) {
        // Leave outputs unchanged on error
    }
}

// MiniPlayer mode size configuration access functions
int get_miniplayer_undocked_width() { return cfg_miniplayer_undocked_width; }
int get_miniplayer_undocked_height() { return cfg_miniplayer_undocked_height; }
int get_miniplayer_compact_width() { return cfg_miniplayer_compact_width; }
int get_miniplayer_compact_height() { return cfg_miniplayer_compact_height; }
int get_miniplayer_expanded_size() { return cfg_miniplayer_expanded_size; }

// Font configuration access functions
bool get_use_artist_custom_font() {
    return cfg_use_artist_custom_font != 0;
}

bool get_use_track_custom_font() {
    return cfg_use_track_custom_font != 0;
}

bool get_use_custom_fonts() {
    return get_use_artist_custom_font() || get_use_track_custom_font();
}

LOGFONT get_artist_font() {
    return cfg_artist_font.get_value();
}

LOGFONT get_track_font() {
    return cfg_track_font.get_value();
}

void set_artist_font(const LOGFONT& font) {
    cfg_artist_font = font;
    cfg_use_artist_custom_font = 1;
}

void set_track_font(const LOGFONT& font) {
    cfg_track_font = font;
    cfg_use_track_custom_font = 1;
}

void reset_fonts() {
    cfg_use_artist_custom_font = 0;
    cfg_use_track_custom_font = 0;
    cfg_artist_font = get_default_font(true, 9);
    cfg_track_font = get_default_font(false, 11);
}

// Control Panel font configuration access functions
bool get_cp_use_artist_custom_font() {
    return cfg_cp_use_artist_custom_font != 0;
}

bool get_cp_use_track_custom_font() {
    return cfg_cp_use_track_custom_font != 0;
}

bool get_cp_use_custom_fonts() {
    return get_cp_use_artist_custom_font() || get_cp_use_track_custom_font();
}

LOGFONT get_cp_artist_font() {
    return cfg_cp_artist_font.get_value();
}

LOGFONT get_cp_track_font() {
    return cfg_cp_track_font.get_value();
}

void set_cp_artist_font(const LOGFONT& font) {
    cfg_cp_artist_font = font;
    cfg_cp_use_artist_custom_font = 1;
}

void set_cp_track_font(const LOGFONT& font) {
    cfg_cp_track_font = font;
    cfg_cp_use_track_custom_font = 1;
}

void reset_cp_fonts() {
    LOGFONT default_artist = get_default_font(true, 9);
    LOGFONT default_track = get_default_font(false, 11);
    
    cfg_cp_artist_font = default_artist;
    cfg_cp_track_font = default_track;
    
    cfg_cp_use_artist_custom_font = 0;
    cfg_cp_use_track_custom_font = 0;
}

// Undocked mode font accessor functions
bool get_undocked_use_artist_custom_font() {
    return cfg_undocked_use_artist_custom_font != 0;
}

bool get_undocked_use_track_custom_font() {
    return cfg_undocked_use_track_custom_font != 0;
}

bool get_undocked_use_custom_fonts() {
    return get_undocked_use_artist_custom_font() || get_undocked_use_track_custom_font();
}

LOGFONT get_undocked_artist_font() {
    return cfg_undocked_artist_font.get_value();
}

LOGFONT get_undocked_track_font() {
    return cfg_undocked_track_font.get_value();
}

// Expanded mode font accessor functions
bool get_expanded_use_artist_custom_font() {
    return cfg_expanded_use_artist_custom_font != 0;
}

bool get_expanded_use_track_custom_font() {
    return cfg_expanded_use_track_custom_font != 0;
}

bool get_expanded_use_custom_fonts() {
    return get_expanded_use_artist_custom_font() || get_expanded_use_track_custom_font();
}

LOGFONT get_expanded_artist_font() {
    return cfg_expanded_artist_font.get_value();
}

LOGFONT get_expanded_track_font() {
    return cfg_expanded_track_font.get_value();
}

// Compact mode font accessor functions
bool get_compact_use_artist_custom_font() {
    return cfg_compact_use_artist_custom_font != 0;
}

bool get_compact_use_track_custom_font() {
    return cfg_compact_use_track_custom_font != 0;
}

bool get_compact_use_custom_fonts() {
    return get_compact_use_artist_custom_font() || get_compact_use_track_custom_font();
}

LOGFONT get_compact_artist_font() {
    return cfg_compact_artist_font.get_value();
}

LOGFONT get_compact_track_font() {
    return cfg_compact_track_font.get_value();
}

// Timer font accessor functions
bool get_timer_use_custom_font() {
    return cfg_timer_use_custom_font != 0;
}

LOGFONT get_timer_font() {
    return cfg_timer_font.get_value();
}

// Helper function to get default LOGFONT
LOGFONT get_default_font(bool is_artist, int size) {
    LOGFONT lf = {};
    
    // Always apply DPI scaling to ensure fonts display correctly on high-DPI displays
    // Convert point size to device pixels using current DPI
    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    lf.lfHeight = -MulDiv(size, dpi, 72);  // Points to pixels: size * dpi / 72
    ReleaseDC(nullptr, hdc);
    
    lf.lfWeight = is_artist ? FW_NORMAL : FW_BOLD; // Artist regular, Track bold
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS; // Use TrueType precision for better size control
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = CLEARTYPE_QUALITY; // Use ClearType for better rendering
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Microsoft YaHei UI");
    
    // Force no scaling constraints
    lf.lfWidth = 0; // Let Windows calculate width
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;
    lf.lfItalic = FALSE;
    lf.lfUnderline = FALSE;
    lf.lfStrikeOut = FALSE;
    
    
    return lf;
}

// GUID for our preferences page
static const GUID guid_preferences_page_tray = 
{ 0x12345678, 0x9abc, 0xdef0, { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0 } };

// Opens the Preferences dialog at this component's page (General tab is shown first)
void show_preferences_page() {
    ui_control::get()->show_preferences(guid_preferences_page_tray);
}

//=============================================================================
// tray_preferences - preferences page instance implementation
//=============================================================================

tray_preferences::tray_preferences(preferences_page_callback::ptr callback) 
    : m_hwnd(nullptr), m_callback(callback), m_has_changes(false), m_current_tab(0) {
}

HWND tray_preferences::get_wnd() {
    return m_hwnd;
}

t_uint32 tray_preferences::get_state() {
    t_uint32 state = preferences_state::resettable | preferences_state::dark_mode_supported;
    if (m_has_changes) {
        state |= preferences_state::changed;
    }
    return state;
}

void tray_preferences::apply() {
    apply_settings();
    m_has_changes = false;
    m_callback->on_state_changed();
}

void tray_preferences::reset() {
    // Reset General tab settings
    reset_settings();
    
    // Reset Fonts tab settings (all modes)
    cfg_cp_use_artist_custom_font = 0;
    cfg_cp_use_track_custom_font = 0;
    cfg_undocked_use_artist_custom_font = 0;
    cfg_undocked_use_track_custom_font = 0;
    cfg_expanded_use_artist_custom_font = 0;
    cfg_expanded_use_track_custom_font = 0;
    cfg_compact_use_artist_custom_font = 0;
    cfg_compact_use_track_custom_font = 0;
    cfg_use_artist_custom_font = 0;
    cfg_use_track_custom_font = 0;
    cfg_timer_use_custom_font = 0;
    
    // Update font displays to show defaults
    update_font_displays();
    
    // Notify control panel to reload fonts
    control_panel::get_instance().on_settings_changed();
    
    m_has_changes = false;
    m_callback->on_state_changed();
}

INT_PTR CALLBACK tray_preferences::ConfigProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    tray_preferences* p_this = nullptr;
    
    if (msg == WM_INITDIALOG) {
        p_this = reinterpret_cast<tray_preferences*>(lp);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, lp);
        p_this->m_hwnd = hwnd;
        
        // Initialize dark mode hooks
        p_this->m_darkMode.AddDialogWithControls(hwnd);
        
        // Enable tab page texture to fix text shadow rendering
        EnableThemeDialogTexture(hwnd, ETDT_ENABLETAB);
        
        // Initialize tab control
        p_this->init_tab_control();
        
        // Initialize checkbox states
        CheckDlgButton(hwnd, IDC_ALWAYS_MINIMIZE_TO_TRAY, cfg_always_minimize_to_tray != 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_SHOW_POPUP_NOTIFICATION, cfg_show_popup_notification != 0 ? BST_CHECKED : BST_UNCHECKED);

        // Initialize general tab comboboxes
        HWND hPosCombo = GetDlgItem(hwnd, IDC_POPUP_POSITION_COMBO);
        SendMessage(hPosCombo, CB_ADDSTRING, 0, (LPARAM)L"Top Left");
        SendMessage(hPosCombo, CB_ADDSTRING, 0, (LPARAM)L"Middle Left");
        SendMessage(hPosCombo, CB_ADDSTRING, 0, (LPARAM)L"Bottom Left");
        SendMessage(hPosCombo, CB_ADDSTRING, 0, (LPARAM)L"Top Right");
        SendMessage(hPosCombo, CB_ADDSTRING, 0, (LPARAM)L"Middle Right");
        SendMessage(hPosCombo, CB_ADDSTRING, 0, (LPARAM)L"Bottom Right");
        int pos = cfg_popup_position;
        if (pos < 0 || pos > 5) pos = 0;
        SendMessage(hPosCombo, CB_SETCURSEL, pos, 0);

        HWND hDurationCombo = GetDlgItem(hwnd, IDC_POPUP_DURATION_COMBO);
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"1 second");
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"2 seconds");
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"3 seconds");
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"4 seconds");
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"5 seconds");
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"7 seconds");
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"10 seconds");

        int duration_index = 2; // Default 3s
        if (cfg_popup_duration == 1000) duration_index = 0;
        else if (cfg_popup_duration == 2000) duration_index = 1;
        else if (cfg_popup_duration == 3000) duration_index = 2;
        else if (cfg_popup_duration == 4000) duration_index = 3;
        else if (cfg_popup_duration == 5000) duration_index = 4;
        else if (cfg_popup_duration == 7000) duration_index = 5;
        else if (cfg_popup_duration == 10000) duration_index = 6;
        SendMessage(hDurationCombo, CB_SETCURSEL, duration_index, 0);

        HWND hSlideCombo = GetDlgItem(hwnd, IDC_SLIDE_DURATION_COMBO);
        SendMessage(hSlideCombo, CB_ADDSTRING, 0, (LPARAM)L"Instant (100 ms)");
        SendMessage(hSlideCombo, CB_ADDSTRING, 0, (LPARAM)L"Fast (200 ms)");
        SendMessage(hSlideCombo, CB_ADDSTRING, 0, (LPARAM)L"Normal (300 ms)");
        SendMessage(hSlideCombo, CB_ADDSTRING, 0, (LPARAM)L"Smooth (400 ms)");
        SendMessage(hSlideCombo, CB_ADDSTRING, 0, (LPARAM)L"Slow (500 ms)");

        int slide_index = 1; // Default 200ms
        if (cfg_slide_duration == 100) slide_index = 0;
        else if (cfg_slide_duration == 200) slide_index = 1;
        else if (cfg_slide_duration == 300) slide_index = 2;
        else if (cfg_slide_duration == 400) slide_index = 3;
        else if (cfg_slide_duration == 500) slide_index = 4;
        SendMessage(hSlideCombo, CB_SETCURSEL, slide_index, 0);

        // Initialize general checkboxes
        CheckDlgButton(hwnd, IDC_ALWAYS_MINIMIZE_TO_TRAY, cfg_always_minimize_to_tray ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_SHOW_POPUP_NOTIFICATION, cfg_show_popup_notification ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_DISABLE_MINIPLAYER, cfg_disable_miniplayer ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_DISABLE_SLIDE_TO_SIDE, cfg_disable_slide_to_side ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_ALWAYS_SLIDE_TO_SIDE, cfg_always_slide_to_side ? BST_CHECKED : BST_UNCHECKED);

        // Initialize appearance tab comboboxes
        HWND hThemeCombo = GetDlgItem(hwnd, IDC_THEME_MODE_COMBO);
        SendMessage(hThemeCombo, CB_ADDSTRING, 0, (LPARAM)L"Auto");
        SendMessage(hThemeCombo, CB_ADDSTRING, 0, (LPARAM)L"Dark");
        SendMessage(hThemeCombo, CB_ADDSTRING, 0, (LPARAM)L"Light");
        SendMessage(hThemeCombo, CB_SETCURSEL, cfg_theme_mode, 0);

        HWND hCoverArtCombo = GetDlgItem(hwnd, IDC_COVER_ARTWORK_COMBO);
        SendMessage(hCoverArtCombo, CB_ADDSTRING, 0, (LPARAM)L"Yes");
        SendMessage(hCoverArtCombo, CB_ADDSTRING, 0, (LPARAM)L"No");
        SendMessage(hCoverArtCombo, CB_SETCURSEL, (cfg_show_cover_art != 0) ? 0 : 1, 0);

        HWND hCoverMarginCombo = GetDlgItem(hwnd, IDC_COVER_MARGIN_COMBO);
        SendMessage(hCoverMarginCombo, CB_ADDSTRING, 0, (LPARAM)L"Yes");
        SendMessage(hCoverMarginCombo, CB_ADDSTRING, 0, (LPARAM)L"No");
        SendMessage(hCoverMarginCombo, CB_SETCURSEL, (cfg_cover_margin != 0) ? 0 : 1, 0);

        HWND hCoverStyleCombo = GetDlgItem(hwnd, IDC_COVER_STYLE_COMBO);
        SendMessage(hCoverStyleCombo, CB_ADDSTRING, 0, (LPARAM)L"Square");
        SendMessage(hCoverStyleCombo, CB_ADDSTRING, 0, (LPARAM)L"Rounded");
        SendMessage(hCoverStyleCombo, CB_SETCURSEL, cfg_cover_style, 0);

        HWND hBgStyleCombo = GetDlgItem(hwnd, IDC_BACKGROUND_STYLE_COMBO);
        SendMessage(hBgStyleCombo, CB_ADDSTRING, 0, (LPARAM)L"Solid");
        SendMessage(hBgStyleCombo, CB_ADDSTRING, 0, (LPARAM)L"Artwork Colors");
        SendMessage(hBgStyleCombo, CB_ADDSTRING, 0, (LPARAM)L"Blurred Artwork");
        SendMessage(hBgStyleCombo, CB_SETCURSEL, cfg_background_style, 0);

        HWND hMiniPlayerBorderCombo = GetDlgItem(hwnd, IDC_MINIPLAYER_BORDER_COMBO);
        SendMessage(hMiniPlayerBorderCombo, CB_ADDSTRING, 0, (LPARAM)L"Square");
        SendMessage(hMiniPlayerBorderCombo, CB_ADDSTRING, 0, (LPARAM)L"Rounded");
        SendMessage(hMiniPlayerBorderCombo, CB_SETCURSEL, cfg_miniplayer_border_style, 0);

        CheckDlgButton(hwnd, IDC_SHOW_VOLUME_FEEDBACK, cfg_show_volume_feedback ? BST_CHECKED : BST_UNCHECKED);

        // Initialize display format edit fields
        uSetDlgItemText(hwnd, IDC_LINE1_FORMAT_EDIT, cfg_line1_format);
        uSetDlgItemText(hwnd, IDC_LINE2_FORMAT_EDIT, cfg_line2_format);

        s_ignore_edit_change = true;

        // Initialize MiniPlayer Undocked size combobox and edit fields
        HWND hUndockedPresetCombo = GetDlgItem(hwnd, IDC_MINIPLAYER_UNDOCKED_PRESET_COMBO);
        SendMessage(hUndockedPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Normal / Default (400 x 120 px)");
        SendMessage(hUndockedPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Large (480 x 140 px)");
        SendMessage(hUndockedPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Wide (540 x 120 px)");
        SendMessage(hUndockedPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Custom");

        int u_w = cfg_miniplayer_undocked_width;
        int u_h = cfg_miniplayer_undocked_height;
        int u_preset = 3; // Custom
        if (u_w == 400 && u_h == 120) u_preset = 0;
        else if (u_w == 480 && u_h == 140) u_preset = 1;
        else if (u_w == 540 && u_h == 120) u_preset = 2;
        SendMessage(hUndockedPresetCombo, CB_SETCURSEL, u_preset, 0);
        SetDlgItemInt(hwnd, IDC_MINIPLAYER_UNDOCKED_WIDTH_EDIT, u_w, FALSE);
        SetDlgItemInt(hwnd, IDC_MINIPLAYER_UNDOCKED_HEIGHT_EDIT, u_h, FALSE);

        // Initialize MiniPlayer Compact size combobox and edit fields
        HWND hCompactPresetCombo = GetDlgItem(hwnd, IDC_MINIPLAYER_COMPACT_PRESET_COMBO);
        SendMessage(hCompactPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Small (280 x 60 px)");
        SendMessage(hCompactPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Normal / Default (320 x 75 px)");
        SendMessage(hCompactPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Large (380 x 90 px)");
        SendMessage(hCompactPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Wide (440 x 75 px)");
        SendMessage(hCompactPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Custom");

        int c_w = cfg_miniplayer_compact_width;
        int c_h = cfg_miniplayer_compact_height;
        int c_preset = 4; // Custom
        if (c_w == 280 && c_h == 60) c_preset = 0;
        else if (c_w == 320 && c_h == 75) c_preset = 1;
        else if (c_w == 380 && c_h == 90) c_preset = 2;
        else if (c_w == 440 && c_h == 75) c_preset = 3;
        SendMessage(hCompactPresetCombo, CB_SETCURSEL, c_preset, 0);
        SetDlgItemInt(hwnd, IDC_MINIPLAYER_COMPACT_WIDTH_EDIT, c_w, FALSE);
        SetDlgItemInt(hwnd, IDC_MINIPLAYER_COMPACT_HEIGHT_EDIT, c_h, FALSE);

        // Initialize MiniPlayer Expanded size combobox and edit fields
        HWND hExpandedPresetCombo = GetDlgItem(hwnd, IDC_MINIPLAYER_EXPANDED_PRESET_COMBO);
        SendMessage(hExpandedPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Small (250 x 250 px)");
        SendMessage(hExpandedPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Normal / Default (350 x 350 px)");
        SendMessage(hExpandedPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Large (450 x 450 px)");
        SendMessage(hExpandedPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Extra Large (550 x 550 px)");
        SendMessage(hExpandedPresetCombo, CB_ADDSTRING, 0, (LPARAM)L"Custom");

        int e_s = cfg_miniplayer_expanded_size;
        int e_preset = 4; // Custom
        if (e_s == 250) e_preset = 0;
        else if (e_s == 350) e_preset = 1;
        else if (e_s == 450) e_preset = 2;
        else if (e_s == 550) e_preset = 3;
        SendMessage(hExpandedPresetCombo, CB_SETCURSEL, e_preset, 0);
        SetDlgItemInt(hwnd, IDC_MINIPLAYER_EXPANDED_SIZE_EDIT, e_s, FALSE);

        s_ignore_edit_change = false;

        // Initialize font displays
        p_this->update_font_displays();
        
        // Show initial tab (General)
        p_this->switch_tab(0);
        
        p_this->m_has_changes = false;
    } else {
        p_this = reinterpret_cast<tray_preferences*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (p_this == nullptr) return FALSE;
    
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_ALWAYS_MINIMIZE_TO_TRAY:
        case IDC_SHOW_POPUP_NOTIFICATION:
        case IDC_DISABLE_MINIPLAYER:
        case IDC_DISABLE_SLIDE_TO_SIDE:
        case IDC_ALWAYS_SLIDE_TO_SIDE:
        case IDC_SHOW_VOLUME_FEEDBACK:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->on_changed();
            }
            break;


        case IDC_LINE1_FORMAT_EDIT:
        case IDC_LINE2_FORMAT_EDIT:
            if (HIWORD(wp) == EN_CHANGE) {
                p_this->on_changed();
            }
            break;

        case IDC_MINIPLAYER_UNDOCKED_WIDTH_EDIT:
        case IDC_MINIPLAYER_UNDOCKED_HEIGHT_EDIT:
            if (HIWORD(wp) == EN_CHANGE) {
                if (!s_ignore_edit_change) {
                    int w = (int)GetDlgItemInt(hwnd, IDC_MINIPLAYER_UNDOCKED_WIDTH_EDIT, nullptr, FALSE);
                    int h = (int)GetDlgItemInt(hwnd, IDC_MINIPLAYER_UNDOCKED_HEIGHT_EDIT, nullptr, FALSE);
                    HWND hCombo = GetDlgItem(hwnd, IDC_MINIPLAYER_UNDOCKED_PRESET_COMBO);
                    int sel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
                    if ((sel == 0 && (w != 400 || h != 120)) ||
                        (sel == 1 && (w != 480 || h != 140)) ||
                        (sel == 2 && (w != 540 || h != 120))) {
                        SendMessage(hCombo, CB_SETCURSEL, 3, 0);
                    }
                }
                p_this->on_changed();
            }
            break;

        case IDC_MINIPLAYER_COMPACT_WIDTH_EDIT:
        case IDC_MINIPLAYER_COMPACT_HEIGHT_EDIT:
            if (HIWORD(wp) == EN_CHANGE) {
                if (!s_ignore_edit_change) {
                    int w = (int)GetDlgItemInt(hwnd, IDC_MINIPLAYER_COMPACT_WIDTH_EDIT, nullptr, FALSE);
                    int h = (int)GetDlgItemInt(hwnd, IDC_MINIPLAYER_COMPACT_HEIGHT_EDIT, nullptr, FALSE);
                    HWND hCombo = GetDlgItem(hwnd, IDC_MINIPLAYER_COMPACT_PRESET_COMBO);
                    int sel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
                    if ((sel == 0 && (w != 280 || h != 60)) ||
                        (sel == 1 && (w != 320 || h != 75)) ||
                        (sel == 2 && (w != 380 || h != 90)) ||
                        (sel == 3 && (w != 440 || h != 75))) {
                        SendMessage(hCombo, CB_SETCURSEL, 4, 0);
                    }
                }
                p_this->on_changed();
            }
            break;

        case IDC_MINIPLAYER_EXPANDED_SIZE_EDIT:
            if (HIWORD(wp) == EN_CHANGE) {
                if (!s_ignore_edit_change) {
                    int s = (int)GetDlgItemInt(hwnd, IDC_MINIPLAYER_EXPANDED_SIZE_EDIT, nullptr, FALSE);
                    HWND hCombo = GetDlgItem(hwnd, IDC_MINIPLAYER_EXPANDED_PRESET_COMBO);
                    int sel = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);
                    if ((sel == 0 && s != 250) ||
                        (sel == 1 && s != 350) ||
                        (sel == 2 && s != 450) ||
                        (sel == 3 && s != 550)) {
                        SendMessage(hCombo, CB_SETCURSEL, 4, 0);
                    }
                }
                p_this->on_changed();
            }
            break;

        case IDC_MINIPLAYER_UNDOCKED_PRESET_COMBO:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int sel = (int)SendMessage(GetDlgItem(hwnd, IDC_MINIPLAYER_UNDOCKED_PRESET_COMBO), CB_GETCURSEL, 0, 0);
                s_ignore_edit_change = true;
                if (sel == 0) { SetDlgItemInt(hwnd, IDC_MINIPLAYER_UNDOCKED_WIDTH_EDIT, 400, FALSE); SetDlgItemInt(hwnd, IDC_MINIPLAYER_UNDOCKED_HEIGHT_EDIT, 120, FALSE); }
                else if (sel == 1) { SetDlgItemInt(hwnd, IDC_MINIPLAYER_UNDOCKED_WIDTH_EDIT, 480, FALSE); SetDlgItemInt(hwnd, IDC_MINIPLAYER_UNDOCKED_HEIGHT_EDIT, 140, FALSE); }
                else if (sel == 2) { SetDlgItemInt(hwnd, IDC_MINIPLAYER_UNDOCKED_WIDTH_EDIT, 540, FALSE); SetDlgItemInt(hwnd, IDC_MINIPLAYER_UNDOCKED_HEIGHT_EDIT, 120, FALSE); }
                s_ignore_edit_change = false;
                p_this->on_changed();
            }
            break;

        case IDC_MINIPLAYER_COMPACT_PRESET_COMBO:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int sel = (int)SendMessage(GetDlgItem(hwnd, IDC_MINIPLAYER_COMPACT_PRESET_COMBO), CB_GETCURSEL, 0, 0);
                s_ignore_edit_change = true;
                if (sel == 0) { SetDlgItemInt(hwnd, IDC_MINIPLAYER_COMPACT_WIDTH_EDIT, 280, FALSE); SetDlgItemInt(hwnd, IDC_MINIPLAYER_COMPACT_HEIGHT_EDIT, 60, FALSE); }
                else if (sel == 1) { SetDlgItemInt(hwnd, IDC_MINIPLAYER_COMPACT_WIDTH_EDIT, 320, FALSE); SetDlgItemInt(hwnd, IDC_MINIPLAYER_COMPACT_HEIGHT_EDIT, 75, FALSE); }
                else if (sel == 2) { SetDlgItemInt(hwnd, IDC_MINIPLAYER_COMPACT_WIDTH_EDIT, 380, FALSE); SetDlgItemInt(hwnd, IDC_MINIPLAYER_COMPACT_HEIGHT_EDIT, 90, FALSE); }
                else if (sel == 3) { SetDlgItemInt(hwnd, IDC_MINIPLAYER_COMPACT_WIDTH_EDIT, 440, FALSE); SetDlgItemInt(hwnd, IDC_MINIPLAYER_COMPACT_HEIGHT_EDIT, 75, FALSE); }
                s_ignore_edit_change = false;
                p_this->on_changed();
            }
            break;

        case IDC_MINIPLAYER_EXPANDED_PRESET_COMBO:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int sel = (int)SendMessage(GetDlgItem(hwnd, IDC_MINIPLAYER_EXPANDED_PRESET_COMBO), CB_GETCURSEL, 0, 0);
                s_ignore_edit_change = true;
                if (sel == 0) { SetDlgItemInt(hwnd, IDC_MINIPLAYER_EXPANDED_SIZE_EDIT, 250, FALSE); }
                else if (sel == 1) { SetDlgItemInt(hwnd, IDC_MINIPLAYER_EXPANDED_SIZE_EDIT, 350, FALSE); }
                else if (sel == 2) { SetDlgItemInt(hwnd, IDC_MINIPLAYER_EXPANDED_SIZE_EDIT, 450, FALSE); }
                else if (sel == 3) { SetDlgItemInt(hwnd, IDC_MINIPLAYER_EXPANDED_SIZE_EDIT, 550, FALSE); }
                s_ignore_edit_change = false;
                p_this->on_changed();
            }
            break;

        case IDC_POPUP_POSITION_COMBO:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int pos_sel = (int)SendMessage(GetDlgItem(hwnd, IDC_POPUP_POSITION_COMBO), CB_GETCURSEL, 0, 0);
                if (pos_sel >= 0) {
                    cfg_popup_position = pos_sel;
                    popup_window::get_instance().on_settings_changed();
                }
                p_this->on_changed();
            }
            break;

        case IDC_POPUP_DURATION_COMBO:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int duration_index = (int)SendMessage(GetDlgItem(hwnd, IDC_POPUP_DURATION_COMBO), CB_GETCURSEL, 0, 0);
                int duration_values[] = {1000, 2000, 3000, 4000, 5000, 7000, 10000};
                if (duration_index >= 0 && duration_index < 7) {
                    cfg_popup_duration = duration_values[duration_index];
                    popup_window::get_instance().on_settings_changed();
                }
                p_this->on_changed();
            }
            break;

        case IDC_THEME_MODE_COMBO:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int sel = (int)SendMessage(GetDlgItem(hwnd, IDC_THEME_MODE_COMBO), CB_GETCURSEL, 0, 0);
                if (sel >= 0) {
                    cfg_theme_mode = sel;
                    popup_window::get_instance().on_settings_changed();
                    control_panel::get_instance().on_settings_changed();
                }
                p_this->on_changed();
            }
            break;

        case IDC_BACKGROUND_STYLE_COMBO:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int sel = (int)SendMessage(GetDlgItem(hwnd, IDC_BACKGROUND_STYLE_COMBO), CB_GETCURSEL, 0, 0);
                if (sel >= 0) {
                    cfg_background_style = sel;
                    popup_window::get_instance().on_settings_changed();
                    control_panel::get_instance().on_settings_changed();
                }
                p_this->on_changed();
            }
            break;

        case IDC_SLIDE_DURATION_COMBO:
        case IDC_COVER_ARTWORK_COMBO:
        case IDC_COVER_MARGIN_COMBO:
        case IDC_COVER_STYLE_COMBO:
        case IDC_MINIPLAYER_BORDER_COMBO:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                p_this->on_changed();
            }
            break;

        case IDC_PREVIEW_POPUP_BTN:
            if (HIWORD(wp) == BN_CLICKED) {
                popup_window::get_instance().show_preview();
            }
            break;

        case IDC_SELECT_ARTIST_FONT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_artist_font();
            }
            break;
            
        case IDC_SELECT_TRACK_FONT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_track_font();
            }
            break;
            
        case IDC_RESET_FONTS:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->reset_fonts_to_default();
            }
            break;
            
        // Docked mode font handlers (IDC_CP_* are aliases to IDC_DOCKED_*)
        case IDC_DOCKED_ARTIST_SELECT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_cp_artist_font();
            }
            break;
            
        case IDC_DOCKED_TRACK_SELECT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_cp_track_font();
            }
            break;
            
        // Undocked mode font handlers
        case IDC_UNDOCKED_ARTIST_SELECT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_font_for_mode(1, true); // mode 1 = undocked, artist
            }
            break;
            
        case IDC_UNDOCKED_TRACK_SELECT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_font_for_mode(1, false); // mode 1 = undocked, track
            }
            break;
            
        // Expanded mode font handlers
        case IDC_EXPANDED_ARTIST_SELECT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_font_for_mode(2, true); // mode 2 = expanded, artist
            }
            break;
            
        case IDC_EXPANDED_TRACK_SELECT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_font_for_mode(2, false); // mode 2 = expanded, track
            }
            break;
            
        // Compact mode font handlers
        case IDC_COMPACT_ARTIST_SELECT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_font_for_mode(3, true); // mode 3 = compact, artist
            }
            break;
            
        case IDC_COMPACT_TRACK_SELECT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_font_for_mode(3, false); // mode 3 = compact, track
            }
            break;
            
        case IDC_TIMER_FONT_SELECT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_timer_font();
            }
            break;
        }
        break;
        
    case WM_NOTIFY:
        {
            NMHDR* pnmhdr = reinterpret_cast<NMHDR*>(lp);
            if (pnmhdr->idFrom == IDC_TAB_CONTROL && pnmhdr->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(pnmhdr->hwndFrom);
                p_this->switch_tab(sel);
            }
        }
        break;
        
    // Note: WM_CTLCOLORSTATIC removed - shadow effect may be a foobar2000 framework issue
        
    case WM_DESTROY:
        p_this->m_hwnd = nullptr;
        break;
    }
    
    return FALSE;
}

void tray_preferences::on_changed() {
    m_has_changes = true;
    m_callback->on_state_changed();
}

bool tray_preferences::has_changed() {
    if (!m_hwnd) return false;
    
    int current_minimize_to_tray = (IsDlgButtonChecked(m_hwnd, IDC_ALWAYS_MINIMIZE_TO_TRAY) == BST_CHECKED) ? 1 : 0;
    int current_show_popup = (IsDlgButtonChecked(m_hwnd, IDC_SHOW_POPUP_NOTIFICATION) == BST_CHECKED) ? 1 : 0;
    int current_disable_miniplayer = (IsDlgButtonChecked(m_hwnd, IDC_DISABLE_MINIPLAYER) == BST_CHECKED) ? 1 : 0;
    int current_disable_slide = (IsDlgButtonChecked(m_hwnd, IDC_DISABLE_SLIDE_TO_SIDE) == BST_CHECKED) ? 1 : 0;
    int current_popup_position = (int)SendMessage(GetDlgItem(m_hwnd, IDC_POPUP_POSITION_COMBO), CB_GETCURSEL, 0, 0);
    int current_show_volume_feedback = (IsDlgButtonChecked(m_hwnd, IDC_SHOW_VOLUME_FEEDBACK) == BST_CHECKED) ? 1 : 0;
    
    return (current_minimize_to_tray != cfg_always_minimize_to_tray) || 
           (current_show_popup != cfg_show_popup_notification) ||
           (current_disable_miniplayer != cfg_disable_miniplayer) ||
           (current_disable_slide != cfg_disable_slide_to_side) ||
           (current_popup_position != cfg_popup_position) ||
           (current_show_volume_feedback != cfg_show_volume_feedback);

}

void tray_preferences::apply_settings() {
    if (m_hwnd) {
        cfg_always_minimize_to_tray = (IsDlgButtonChecked(m_hwnd, IDC_ALWAYS_MINIMIZE_TO_TRAY) == BST_CHECKED) ? 1 : 0;
        cfg_show_popup_notification = (IsDlgButtonChecked(m_hwnd, IDC_SHOW_POPUP_NOTIFICATION) == BST_CHECKED) ? 1 : 0;
        cfg_disable_miniplayer = (IsDlgButtonChecked(m_hwnd, IDC_DISABLE_MINIPLAYER) == BST_CHECKED) ? 1 : 0;
        cfg_disable_slide_to_side = (IsDlgButtonChecked(m_hwnd, IDC_DISABLE_SLIDE_TO_SIDE) == BST_CHECKED) ? 1 : 0;
        cfg_always_slide_to_side = (IsDlgButtonChecked(m_hwnd, IDC_ALWAYS_SLIDE_TO_SIDE) == BST_CHECKED) ? 1 : 0;
        cfg_popup_position = (int)SendMessage(GetDlgItem(m_hwnd, IDC_POPUP_POSITION_COMBO), CB_GETCURSEL, 0, 0);

        // Convert duration combo index to milliseconds
        int duration_index = (int)SendMessage(GetDlgItem(m_hwnd, IDC_POPUP_DURATION_COMBO), CB_GETCURSEL, 0, 0);
        int duration_values[] = {1000, 2000, 3000, 4000, 5000, 7000, 10000};
        if (duration_index >= 0 && duration_index < 7) {
            cfg_popup_duration = duration_values[duration_index];
        }

        // Convert slide duration combo index to milliseconds
        int slide_index = (int)SendMessage(GetDlgItem(m_hwnd, IDC_SLIDE_DURATION_COMBO), CB_GETCURSEL, 0, 0);
        int slide_values[] = {100, 200, 300, 400, 500};
        if (slide_index >= 0 && slide_index < 5) {
            cfg_slide_duration = slide_values[slide_index];
        }

        // Save theme mode
        cfg_theme_mode = (int)SendMessage(GetDlgItem(m_hwnd, IDC_THEME_MODE_COMBO), CB_GETCURSEL, 0, 0);
        int cover_art_sel = (int)SendMessage(GetDlgItem(m_hwnd, IDC_COVER_ARTWORK_COMBO), CB_GETCURSEL, 0, 0);
        cfg_show_cover_art = (cover_art_sel == 0) ? 1 : 0;
        int cover_margin_sel = (int)SendMessage(GetDlgItem(m_hwnd, IDC_COVER_MARGIN_COMBO), CB_GETCURSEL, 0, 0);
        cfg_cover_margin = (cover_margin_sel == 0) ? 1 : 0;
        cfg_cover_style = (int)SendMessage(GetDlgItem(m_hwnd, IDC_COVER_STYLE_COMBO), CB_GETCURSEL, 0, 0);
        cfg_background_style = (int)SendMessage(GetDlgItem(m_hwnd, IDC_BACKGROUND_STYLE_COMBO), CB_GETCURSEL, 0, 0);
        cfg_miniplayer_border_style = (int)SendMessage(GetDlgItem(m_hwnd, IDC_MINIPLAYER_BORDER_COMBO), CB_GETCURSEL, 0, 0);
        cfg_show_volume_feedback = (IsDlgButtonChecked(m_hwnd, IDC_SHOW_VOLUME_FEEDBACK) == BST_CHECKED) ? 1 : 0;

        // Save display format strings
        {
            pfc::string8 format_str;
            uGetDlgItemText(m_hwnd, IDC_LINE1_FORMAT_EDIT, format_str);
            cfg_line1_format = format_str;
            uGetDlgItemText(m_hwnd, IDC_LINE2_FORMAT_EDIT, format_str);
            cfg_line2_format = format_str;
        }

        // Save MiniPlayer mode sizes
        BOOL w_translated = FALSE, h_translated = FALSE;
        int u_w = GetDlgItemInt(m_hwnd, IDC_MINIPLAYER_UNDOCKED_WIDTH_EDIT, &w_translated, FALSE);
        int u_h = GetDlgItemInt(m_hwnd, IDC_MINIPLAYER_UNDOCKED_HEIGHT_EDIT, &h_translated, FALSE);
        if (w_translated && u_w >= 250 && u_w <= 1200) cfg_miniplayer_undocked_width = u_w;
        if (h_translated && u_h >= 80 && u_h <= 600) cfg_miniplayer_undocked_height = u_h;

        int c_w = GetDlgItemInt(m_hwnd, IDC_MINIPLAYER_COMPACT_WIDTH_EDIT, &w_translated, FALSE);
        int c_h = GetDlgItemInt(m_hwnd, IDC_MINIPLAYER_COMPACT_HEIGHT_EDIT, &h_translated, FALSE);
        if (w_translated && c_w >= 200 && c_w <= 800) cfg_miniplayer_compact_width = c_w;
        if (h_translated && c_h >= 50 && c_h <= 300) cfg_miniplayer_compact_height = c_h;

        int e_s = GetDlgItemInt(m_hwnd, IDC_MINIPLAYER_EXPANDED_SIZE_EDIT, &w_translated, FALSE);
        if (w_translated && e_s >= 200 && e_s <= 1000) cfg_miniplayer_expanded_size = e_s;

        // Notify tray manager and control panel of settings change
        tray_manager::get_instance().on_settings_changed();
        control_panel::get_instance().on_settings_changed();
    }
}

void tray_preferences::reset_settings() {
    if (m_hwnd) {
        // Reset config variables to factory defaults
        cfg_always_minimize_to_tray = 0;  // Default: OFF
        cfg_show_popup_notification = 1;  // Default: ON
        cfg_popup_position = 0;           // Default: Top Left
        cfg_disable_miniplayer = 0;       // Default: OFF
        cfg_popup_duration = 3000;        // Default: 3 seconds
        cfg_disable_slide_to_side = 0;    // Default: OFF
        cfg_slide_duration = 200;         // Default: 200ms (Fast)
        cfg_always_slide_to_side = 1;     // Default: ON
        cfg_theme_mode = 0;               // Default: Auto
        cfg_show_cover_art = 1;           // Default: Yes (1)
        cfg_cover_margin = 1;             // Default: Yes (1)
        cfg_cover_style = 0;              // Default: Square (0)
        cfg_background_style = 0;         // Default: Solid (0)
        cfg_show_volume_feedback = 1;     // Default: Yes (1)
        cfg_line1_format = "%title%";     // Default: title
        cfg_line2_format = "%artist%";    // Default: artist

        // Reset MiniPlayer mode size variables
        cfg_miniplayer_undocked_width = 400;
        cfg_miniplayer_undocked_height = 120;
        cfg_miniplayer_compact_width = 320;
        cfg_miniplayer_compact_height = 75;
        cfg_miniplayer_expanded_size = 350;

        // Update UI controls to reflect defaults
        CheckDlgButton(m_hwnd, IDC_ALWAYS_MINIMIZE_TO_TRAY, BST_UNCHECKED);
        CheckDlgButton(m_hwnd, IDC_SHOW_POPUP_NOTIFICATION, BST_CHECKED);
        CheckDlgButton(m_hwnd, IDC_DISABLE_MINIPLAYER, BST_UNCHECKED);
        CheckDlgButton(m_hwnd, IDC_DISABLE_SLIDE_TO_SIDE, BST_UNCHECKED);
        CheckDlgButton(m_hwnd, IDC_ALWAYS_SLIDE_TO_SIDE, BST_CHECKED);
        SendMessage(GetDlgItem(m_hwnd, IDC_POPUP_POSITION_COMBO), CB_SETCURSEL, 0, 0);        // Top Left
        SendMessage(GetDlgItem(m_hwnd, IDC_POPUP_DURATION_COMBO), CB_SETCURSEL, 2, 0);        // 3 seconds (index 2)
        SendMessage(GetDlgItem(m_hwnd, IDC_SLIDE_DURATION_COMBO), CB_SETCURSEL, 1, 0);        // Fast 200ms (index 1)
        SendMessage(GetDlgItem(m_hwnd, IDC_THEME_MODE_COMBO), CB_SETCURSEL, 0, 0);            // Auto
        SendMessage(GetDlgItem(m_hwnd, IDC_COVER_ARTWORK_COMBO), CB_SETCURSEL, 0, 0);         // Yes
        SendMessage(GetDlgItem(m_hwnd, IDC_COVER_MARGIN_COMBO), CB_SETCURSEL, 0, 0);          // Yes
        SendMessage(GetDlgItem(m_hwnd, IDC_COVER_STYLE_COMBO), CB_SETCURSEL, 0, 0);           // Square
        SendMessage(GetDlgItem(m_hwnd, IDC_BACKGROUND_STYLE_COMBO), CB_SETCURSEL, 0, 0);      // Solid
        SendMessage(GetDlgItem(m_hwnd, IDC_MINIPLAYER_BORDER_COMBO), CB_SETCURSEL, 1, 0);     // Rounded (index 1)
        CheckDlgButton(m_hwnd, IDC_SHOW_VOLUME_FEEDBACK, BST_CHECKED);
        uSetDlgItemText(m_hwnd, IDC_LINE1_FORMAT_EDIT, "%title%");
        uSetDlgItemText(m_hwnd, IDC_LINE2_FORMAT_EDIT, "%artist%");

        // Reset MiniPlayer mode size controls
        s_ignore_edit_change = true;
        SendMessage(GetDlgItem(m_hwnd, IDC_MINIPLAYER_UNDOCKED_PRESET_COMBO), CB_SETCURSEL, 0, 0); // Normal
        SetDlgItemInt(m_hwnd, IDC_MINIPLAYER_UNDOCKED_WIDTH_EDIT, 400, FALSE);
        SetDlgItemInt(m_hwnd, IDC_MINIPLAYER_UNDOCKED_HEIGHT_EDIT, 120, FALSE);

        SendMessage(GetDlgItem(m_hwnd, IDC_MINIPLAYER_COMPACT_PRESET_COMBO), CB_SETCURSEL, 1, 0); // Normal
        SetDlgItemInt(m_hwnd, IDC_MINIPLAYER_COMPACT_WIDTH_EDIT, 320, FALSE);
        SetDlgItemInt(m_hwnd, IDC_MINIPLAYER_COMPACT_HEIGHT_EDIT, 75, FALSE);

        SendMessage(GetDlgItem(m_hwnd, IDC_MINIPLAYER_EXPANDED_PRESET_COMBO), CB_SETCURSEL, 1, 0); // Normal
        SetDlgItemInt(m_hwnd, IDC_MINIPLAYER_EXPANDED_SIZE_EDIT, 350, FALSE);
        s_ignore_edit_change = false;

        // Notify components of settings change
        tray_manager::get_instance().on_settings_changed();
        control_panel::get_instance().on_settings_changed();
        
        update_font_displays();
    }
}

// Font management methods
void tray_preferences::update_font_displays() {
    if (!m_hwnd) return;
    
    // Update original artist font display
    if (get_use_artist_custom_font()) {
        LOGFONT lf = get_artist_font();
        pfc::string8 font_desc = format_font_name(lf);
        uSetDlgItemText(m_hwnd, IDC_ARTIST_FONT_DISPLAY, font_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_ARTIST_FONT_DISPLAY, "Microsoft YaHei UI, 11pt, Regular (Default)");
    }
    
    // Update original track font display
    if (get_use_track_custom_font()) {
        LOGFONT lf = get_track_font();
        pfc::string8 font_desc = format_font_name(lf);
        uSetDlgItemText(m_hwnd, IDC_TRACK_FONT_DISPLAY, font_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_TRACK_FONT_DISPLAY, "Microsoft YaHei UI, 14pt, Bold (Default)");
    }
    
    // Update Docked Control Panel font displays
    if (get_cp_use_artist_custom_font()) {
        LOGFONT artist_lf = get_cp_artist_font();
        pfc::string8 artist_desc = format_font_name(artist_lf);
        uSetDlgItemText(m_hwnd, IDC_DOCKED_ARTIST_DISPLAY, artist_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_DOCKED_ARTIST_DISPLAY, "Microsoft YaHei UI, 9pt (Default)");
    }
    if (get_cp_use_track_custom_font()) {
        LOGFONT track_lf = get_cp_track_font();
        pfc::string8 track_desc = format_font_name(track_lf);
        uSetDlgItemText(m_hwnd, IDC_DOCKED_TRACK_DISPLAY, track_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_DOCKED_TRACK_DISPLAY, "Microsoft YaHei UI, 11pt, Bold (Default)");
    }
    
    // Update Undocked mode font displays
    if (get_undocked_use_artist_custom_font()) {
        LOGFONT artist_lf = cfg_undocked_artist_font.get_value();
        pfc::string8 artist_desc = format_font_name(artist_lf);
        uSetDlgItemText(m_hwnd, IDC_UNDOCKED_ARTIST_DISPLAY, artist_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_UNDOCKED_ARTIST_DISPLAY, "Microsoft YaHei UI, 9pt (Default)");
    }
    if (get_undocked_use_track_custom_font()) {
        LOGFONT track_lf = cfg_undocked_track_font.get_value();
        pfc::string8 track_desc = format_font_name(track_lf);
        uSetDlgItemText(m_hwnd, IDC_UNDOCKED_TRACK_DISPLAY, track_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_UNDOCKED_TRACK_DISPLAY, "Microsoft YaHei UI, 11pt, Bold (Default)");
    }
    
    // Update Expanded mode font displays
    if (get_expanded_use_artist_custom_font()) {
        LOGFONT artist_lf = cfg_expanded_artist_font.get_value();
        pfc::string8 artist_desc = format_font_name(artist_lf);
        uSetDlgItemText(m_hwnd, IDC_EXPANDED_ARTIST_DISPLAY, artist_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_EXPANDED_ARTIST_DISPLAY, "Microsoft YaHei UI, 9pt (Default)");
    }
    if (get_expanded_use_track_custom_font()) {
        LOGFONT track_lf = cfg_expanded_track_font.get_value();
        pfc::string8 track_desc = format_font_name(track_lf);
        uSetDlgItemText(m_hwnd, IDC_EXPANDED_TRACK_DISPLAY, track_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_EXPANDED_TRACK_DISPLAY, "Microsoft YaHei UI, 11pt, Bold (Default)");
    }
    
    // Update Compact mode font displays
    if (get_compact_use_artist_custom_font()) {
        LOGFONT artist_lf = cfg_compact_artist_font.get_value();
        pfc::string8 artist_desc = format_font_name(artist_lf);
        uSetDlgItemText(m_hwnd, IDC_COMPACT_ARTIST_DISPLAY, artist_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_COMPACT_ARTIST_DISPLAY, "Microsoft YaHei UI, 9pt (Default)");
    }
    if (get_compact_use_track_custom_font()) {
        LOGFONT track_lf = cfg_compact_track_font.get_value();
        pfc::string8 track_desc = format_font_name(track_lf);
        uSetDlgItemText(m_hwnd, IDC_COMPACT_TRACK_DISPLAY, track_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_COMPACT_TRACK_DISPLAY, "Microsoft YaHei UI, 11pt, Bold (Default)");
    }
    
    // Update Timer font display
    if (cfg_timer_use_custom_font) {
        LOGFONT timer_lf = cfg_timer_font.get_value();
        pfc::string8 timer_desc = format_font_name(timer_lf);
        uSetDlgItemText(m_hwnd, IDC_TIMER_FONT_DISPLAY, timer_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_TIMER_FONT_DISPLAY, "Microsoft YaHei UI, 9pt (Default)");
    }
}

static void filter_at_fonts_from_control(HWND hCtrl) {
    wchar_t className[64] = {};
    GetClassName(hCtrl, className, 64);
    if (_wcsicmp(className, L"COMBOBOX") == 0) {
        int count = (int)SendMessage(hCtrl, CB_GETCOUNT, 0, 0);
        if (count > 0 && count != CB_ERR) {
            for (int i = count - 1; i >= 0; --i) {
                wchar_t text[256] = {};
                if (SendMessage(hCtrl, CB_GETLBTEXT, i, (LPARAM)text) != CB_ERR) {
                    if (text[0] == L'@') {
                        SendMessage(hCtrl, CB_DELETESTRING, i, 0);
                    }
                }
            }
        }
    } else if (_wcsicmp(className, L"LISTBOX") == 0) {
        int count = (int)SendMessage(hCtrl, LB_GETCOUNT, 0, 0);
        if (count > 0 && count != LB_ERR) {
            for (int i = count - 1; i >= 0; --i) {
                wchar_t text[256] = {};
                if (SendMessage(hCtrl, LB_GETTEXT, i, (LPARAM)text) != LB_ERR) {
                    if (text[0] == L'@') {
                        SendMessage(hCtrl, LB_DELETESTRING, i, 0);
                    }
                }
            }
        }
    }
}

static BOOL CALLBACK FilterAtFontsChildEnumProc(HWND hwnd, LPARAM lParam) {
    filter_at_fonts_from_control(hwnd);
    return TRUE;
}

static UINT_PTR CALLBACK FontHookProc(HWND hwndDlg, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_INITDIALOG || (msg == WM_COMMAND && HIWORD(wp) == CBN_SELCHANGE)) {
        EnumChildWindows(hwndDlg, FilterAtFontsChildEnumProc, 0);
    }
    return 0;
}

static bool show_font_picker(HWND hwndOwner, LOGFONT& lf) {
    CHOOSEFONT cf = {};
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = hwndOwner;
    cf.lpLogFont = &lf;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_ENABLEHOOK;
    cf.lpfnHook = FontHookProc;
    
    if (ChooseFont(&cf)) {
        if (lf.lfFaceName[0] == L'@') {
            wchar_t temp[LF_FACESIZE] = {};
            wcscpy_s(temp, lf.lfFaceName + 1);
            wcscpy_s(lf.lfFaceName, temp);
        }
        return true;
    }
    return false;
}

void tray_preferences::select_artist_font() {
    LOGFONT lf;
    
    // Get current font or default
    if (get_use_artist_custom_font()) {
        lf = get_artist_font();
    } else {
        lf = get_default_font(true, 11);
    }
    
    if (show_font_picker(m_hwnd, lf)) {
        set_artist_font(lf);
        update_font_displays();
        on_changed();
    }
}

void tray_preferences::select_track_font() {
    LOGFONT lf;
    
    // Get current font or default
    if (get_use_track_custom_font()) {
        lf = get_track_font();
    } else {
        lf = get_default_font(false, 14);
    }
    
    if (show_font_picker(m_hwnd, lf)) {
        set_track_font(lf);
        update_font_displays();
        on_changed();
    }
}

void tray_preferences::reset_fonts_to_default() {
    reset_fonts();
    update_font_displays();
    on_changed();
}

void tray_preferences::select_cp_artist_font() {
    LOGFONT lf;
    
    // Get current font or default
    if (get_cp_use_artist_custom_font()) {
        lf = get_cp_artist_font();
    } else {
        lf = get_default_font(true, 13);
    }
    
    if (show_font_picker(m_hwnd, lf)) {
        set_cp_artist_font(lf);
        update_font_displays();
        on_changed();
    }
}

void tray_preferences::select_cp_track_font() {
    LOGFONT lf;
    
    // Get current font or default
    if (get_cp_use_track_custom_font()) {
        lf = get_cp_track_font();
    } else {
        lf = get_default_font(false, 14);
    }
    
    if (show_font_picker(m_hwnd, lf)) {
        set_cp_track_font(lf);
        update_font_displays();
        on_changed();
    }
}

void tray_preferences::reset_cp_fonts_to_default() {
    reset_cp_fonts();
    update_font_displays();
    on_changed();
}

void tray_preferences::select_font_for_mode(int mode, bool is_artist) {
    LOGFONT lf;
    
    // Get current font or default based on mode
    switch (mode) {
    case 1: // Undocked
        if (is_artist ? get_undocked_use_artist_custom_font() : get_undocked_use_track_custom_font()) {
            lf = is_artist ? cfg_undocked_artist_font.get_value() : cfg_undocked_track_font.get_value();
        } else {
            lf = get_default_font(is_artist, is_artist ? 11 : 14);
        }
        break;
    case 2: // Expanded
        if (is_artist ? get_expanded_use_artist_custom_font() : get_expanded_use_track_custom_font()) {
            lf = is_artist ? cfg_expanded_artist_font.get_value() : cfg_expanded_track_font.get_value();
        } else {
            lf = get_default_font(is_artist, is_artist ? 11 : 14);
        }
        break;
    case 3: // Compact
        if (is_artist ? get_compact_use_artist_custom_font() : get_compact_use_track_custom_font()) {
            lf = is_artist ? cfg_compact_artist_font.get_value() : cfg_compact_track_font.get_value();
        } else {
            lf = get_default_font(is_artist, is_artist ? 11 : 14);
        }
        break;
    default:
        return;
    }
    
    if (show_font_picker(m_hwnd, lf)) {
        // Save font based on mode
        switch (mode) {
        case 1: // Undocked
            if (is_artist) {
                cfg_undocked_artist_font = lf;
                cfg_undocked_use_artist_custom_font = 1;
            } else {
                cfg_undocked_track_font = lf;
                cfg_undocked_use_track_custom_font = 1;
            }
            break;
        case 2: // Expanded
            if (is_artist) {
                cfg_expanded_artist_font = lf;
                cfg_expanded_use_artist_custom_font = 1;
            } else {
                cfg_expanded_track_font = lf;
                cfg_expanded_use_track_custom_font = 1;
            }
            break;
        case 3: // Compact
            if (is_artist) {
                cfg_compact_artist_font = lf;
                cfg_compact_use_artist_custom_font = 1;
            } else {
                cfg_compact_track_font = lf;
                cfg_compact_use_track_custom_font = 1;
            }
            break;
        }
        
        update_font_displays();
        on_changed();
    }
}

void tray_preferences::select_timer_font() {
    LOGFONT lf;
    
    // Get current timer font or default
    if (cfg_timer_use_custom_font) {
        lf = cfg_timer_font.get_value();
    } else {
        lf = get_default_font(true, 9); // 9pt like artist font
    }
    
    if (show_font_picker(m_hwnd, lf)) {
        cfg_timer_font = lf;
        cfg_timer_use_custom_font = 1;
        
        update_font_displays();
        on_changed();
        
        // Notify control panel to reload fonts
        control_panel::get_instance().on_settings_changed();
    }
}

void tray_preferences::reset_all_fonts_to_default() {
    // Reset Docked mode fonts
    reset_cp_fonts();
    
    // Reset Undocked mode fonts
    cfg_undocked_use_artist_custom_font = 0;
    cfg_undocked_use_track_custom_font = 0;
    
    // Reset Expanded mode fonts
    cfg_expanded_use_artist_custom_font = 0;
    cfg_expanded_use_track_custom_font = 0;
    
    // Reset Compact mode fonts
    cfg_compact_use_artist_custom_font = 0;
    cfg_compact_use_track_custom_font = 0;
    
    // Also reset the popup notification fonts
    cfg_use_artist_custom_font = 0;
    cfg_use_track_custom_font = 0;
    
    // Update displays and notify of change
    update_font_displays();
    on_changed();
    
    // Notify control panel to reload fonts
    control_panel::get_instance().on_settings_changed();
}

pfc::string8 tray_preferences::format_font_name(const LOGFONT& lf) {
    pfc::string8 result;
    
    // Convert font name from wide to UTF-8
    pfc::stringcvt::string_utf8_from_wide font_name(lf.lfFaceName);
    
    // Calculate point size from lfHeight
    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    int point_size = MulDiv(abs(lf.lfHeight), 72, dpi);
    ReleaseDC(nullptr, hdc);
    
    
    // Format string
    result << font_name.get_ptr() << ", " << point_size << "pt";
    
    if (lf.lfWeight >= FW_BOLD) {
        result << ", Bold";
    }
    if (lf.lfItalic) {
        result << ", Italic";
    }
    
    return result;
}

//=============================================================================
// Tab control management
//=============================================================================

void tray_preferences::init_tab_control() {
    if (!m_hwnd) return;
    
    HWND hTab = GetDlgItem(m_hwnd, IDC_TAB_CONTROL);
    if (!hTab) return;
    
    // Add tabs: General (0), Appearance (1), MiniPlayer (2), Fonts (3)
    TCITEM tie = {};
    tie.mask = TCIF_TEXT;
    
    tie.pszText = const_cast<LPWSTR>(L"General");
    TabCtrl_InsertItem(hTab, 0, &tie);
    
    tie.pszText = const_cast<LPWSTR>(L"Appearance");
    TabCtrl_InsertItem(hTab, 1, &tie);

    tie.pszText = const_cast<LPWSTR>(L"MiniPlayer");
    TabCtrl_InsertItem(hTab, 2, &tie);
    
    tie.pszText = const_cast<LPWSTR>(L"Fonts");
    TabCtrl_InsertItem(hTab, 3, &tie);
    
    // Select first tab
    TabCtrl_SetCurSel(hTab, 0);
    m_current_tab = 0;
}

void tray_preferences::switch_tab(int tab) {
    if (!m_hwnd) return;
    
    m_current_tab = tab;
    
    // General tab controls (including static text labels)
    int general_controls[] = {
        // Display Format
        IDC_DISPLAY_FORMAT_GROUP,
        IDC_LINE1_FORMAT_LABEL,
        IDC_LINE1_FORMAT_EDIT,
        IDC_LINE2_FORMAT_LABEL,
        IDC_LINE2_FORMAT_EDIT,
        IDC_ALWAYS_MINIMIZE_TO_TRAY,
        IDC_STATIC_MINIMIZE_HELP,
        IDC_STATIC_WHEEL_HELP,
        IDC_SHOW_POPUP_NOTIFICATION,
        IDC_PREVIEW_POPUP_BTN,
        IDC_POPUP_POSITION_LABEL,
        IDC_POPUP_POSITION_COMBO,
        IDC_POPUP_DURATION_LABEL,
        IDC_POPUP_DURATION_COMBO,
        IDC_DISABLE_MINIPLAYER,
        IDC_STATIC_MINIPLAYER_HELP,
        // Slide-to-Side options
        IDC_DISABLE_SLIDE_TO_SIDE,
        IDC_SLIDE_DURATION_LABEL,
        IDC_SLIDE_DURATION_COMBO,
        IDC_ALWAYS_SLIDE_TO_SIDE
    };
    
    // Appearance tab controls
    int appearance_controls[] = {
        IDC_THEME_MODE_LABEL,
        IDC_THEME_MODE_COMBO,
        IDC_COVER_ARTWORK_LABEL,
        IDC_COVER_ARTWORK_COMBO,
        IDC_COVER_MARGIN_LABEL,
        IDC_COVER_MARGIN_COMBO,
        IDC_COVER_STYLE_LABEL,
        IDC_COVER_STYLE_COMBO,
        IDC_BACKGROUND_STYLE_LABEL,
        IDC_BACKGROUND_STYLE_COMBO,
        IDC_MINIPLAYER_BORDER_LABEL,
        IDC_MINIPLAYER_BORDER_COMBO,
        IDC_SHOW_VOLUME_FEEDBACK
    };

    // MiniPlayer tab controls
    int miniplayer_controls[] = {
        IDC_MINIPLAYER_UNDOCKED_GROUP,
        IDC_MINIPLAYER_UNDOCKED_PRESET_LABEL,
        IDC_MINIPLAYER_UNDOCKED_PRESET_COMBO,
        IDC_MINIPLAYER_UNDOCKED_WIDTH_LABEL,
        IDC_MINIPLAYER_UNDOCKED_WIDTH_EDIT,
        IDC_MINIPLAYER_UNDOCKED_HEIGHT_LABEL,
        IDC_MINIPLAYER_UNDOCKED_HEIGHT_EDIT,
        
        IDC_MINIPLAYER_COMPACT_GROUP,
        IDC_MINIPLAYER_COMPACT_PRESET_LABEL,
        IDC_MINIPLAYER_COMPACT_PRESET_COMBO,
        IDC_MINIPLAYER_COMPACT_WIDTH_LABEL,
        IDC_MINIPLAYER_COMPACT_WIDTH_EDIT,
        IDC_MINIPLAYER_COMPACT_HEIGHT_LABEL,
        IDC_MINIPLAYER_COMPACT_HEIGHT_EDIT,
        
        IDC_MINIPLAYER_EXPANDED_GROUP,
        IDC_MINIPLAYER_EXPANDED_PRESET_LABEL,
        IDC_MINIPLAYER_EXPANDED_PRESET_COMBO,
        IDC_MINIPLAYER_EXPANDED_SIZE_LABEL,
        IDC_MINIPLAYER_EXPANDED_SIZE_EDIT
    };
    
    // Fonts tab controls - all 4 modes
    int fonts_controls[] = {
        // Docked
        IDC_DOCKED_TITLE,
        IDC_DOCKED_ARTIST_LABEL,
        IDC_DOCKED_ARTIST_DISPLAY,
        IDC_DOCKED_ARTIST_SELECT,
        IDC_DOCKED_TRACK_LABEL,
        IDC_DOCKED_TRACK_DISPLAY,
        IDC_DOCKED_TRACK_SELECT,
        // Undocked
        IDC_UNDOCKED_TITLE,
        IDC_UNDOCKED_ARTIST_LABEL,
        IDC_UNDOCKED_ARTIST_DISPLAY,
        IDC_UNDOCKED_ARTIST_SELECT,
        IDC_UNDOCKED_TRACK_LABEL,
        IDC_UNDOCKED_TRACK_DISPLAY,
        IDC_UNDOCKED_TRACK_SELECT,
        // Expanded
        IDC_EXPANDED_TITLE,
        IDC_EXPANDED_ARTIST_LABEL,
        IDC_EXPANDED_ARTIST_DISPLAY,
        IDC_EXPANDED_ARTIST_SELECT,
        IDC_EXPANDED_TRACK_LABEL,
        IDC_EXPANDED_TRACK_DISPLAY,
        IDC_EXPANDED_TRACK_SELECT,
        // Compact
        IDC_COMPACT_TITLE,
        IDC_COMPACT_ARTIST_LABEL,
        IDC_COMPACT_ARTIST_DISPLAY,
        IDC_COMPACT_ARTIST_SELECT,
        IDC_COMPACT_TRACK_LABEL,
        IDC_COMPACT_TRACK_DISPLAY,
        IDC_COMPACT_TRACK_SELECT,
        // Timer (shared)
        IDC_TIMER_TITLE,
        IDC_TIMER_FONT_LABEL,
        IDC_TIMER_FONT_DISPLAY,
        IDC_TIMER_FONT_SELECT
    };
    
    // Show/hide General controls
    int show_general = (tab == 0) ? SW_SHOW : SW_HIDE;
    for (int id : general_controls) {
        HWND hCtrl = GetDlgItem(m_hwnd, id);
        if (hCtrl) ShowWindow(hCtrl, show_general);
    }

    // Show/hide Appearance controls
    int show_appearance = (tab == 1) ? SW_SHOW : SW_HIDE;
    for (int id : appearance_controls) {
        HWND hCtrl = GetDlgItem(m_hwnd, id);
        if (hCtrl) ShowWindow(hCtrl, show_appearance);
    }

    // Show/hide MiniPlayer controls
    int show_miniplayer = (tab == 2) ? SW_SHOW : SW_HIDE;
    for (int id : miniplayer_controls) {
        HWND hCtrl = GetDlgItem(m_hwnd, id);
        if (hCtrl) ShowWindow(hCtrl, show_miniplayer);
    }
    
    // Show/hide Fonts controls
    int show_fonts = (tab == 3) ? SW_SHOW : SW_HIDE;
    for (int id : fonts_controls) {
        HWND hCtrl = GetDlgItem(m_hwnd, id);
        if (hCtrl) ShowWindow(hCtrl, show_fonts);
    }
}

//=============================================================================
// tray_preferences_page - preferences page factory implementation
//=============================================================================

const char* tray_preferences_page::get_name() {
    return "Tray Controls";
}

GUID tray_preferences_page::get_guid() {
    return guid_preferences_page_tray;
}

GUID tray_preferences_page::get_parent_guid() {
    return preferences_page::guid_tools;
}

preferences_page_instance::ptr tray_preferences_page::instantiate(HWND parent, preferences_page_callback::ptr callback) {
    auto instance = fb2k::service_new<tray_preferences>(callback);
    
    HWND hwnd = CreateDialogParam(
        g_hIns, 
        MAKEINTRESOURCE(IDD_PREFERENCES_TRAY), 
        parent, 
        tray_preferences::ConfigProc, 
        reinterpret_cast<LPARAM>(instance.get_ptr())
    );
    
    if (hwnd == nullptr) {
        throw exception_win32(GetLastError());
    }
    
    return instance;
}

// Service registration
static preferences_page_factory_t<tray_preferences_page> g_tray_preferences_page_factory;
