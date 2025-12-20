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
static cfg_int cfg_always_slide_to_side(GUID{0x1234568A, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0); // Default OFF
static cfg_int cfg_use_rounded_corners(GUID{0x12345690, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 1); // Default ON (Win11 style)
static cfg_int cfg_theme_mode(GUID{0x12345691, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0); // 0=Auto, 1=Force Dark, 2=Force Light


// Font configuration - store LOGFONT structure as binary data
static cfg_struct_t<LOGFONT> cfg_artist_font(GUID{0x12345692, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -11;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return lf;
}());
static cfg_struct_t<LOGFONT> cfg_track_font(GUID{0x12345693, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -14;
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return lf;
}());
static cfg_int cfg_use_custom_fonts(GUID{0x12345694, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);

// Control Panel specific font configuration
static cfg_struct_t<LOGFONT> cfg_cp_artist_font(GUID{0x1234569A, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -11;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return lf;
}());
static cfg_struct_t<LOGFONT> cfg_cp_track_font(GUID{0x1234569B, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -14;
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return lf;
}());
static cfg_int cfg_cp_use_custom_fonts(GUID{0x1234569D, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);

// Undocked mode font configuration
static cfg_struct_t<LOGFONT> cfg_undocked_artist_font(GUID{0x123456A0, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -11;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return lf;
}());
static cfg_struct_t<LOGFONT> cfg_undocked_track_font(GUID{0x123456A1, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -14;
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return lf;
}());
static cfg_int cfg_undocked_use_custom_fonts(GUID{0x123456A2, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);

// Expanded mode font configuration
static cfg_struct_t<LOGFONT> cfg_expanded_artist_font(GUID{0x123456B0, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -11;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return lf;
}());
static cfg_struct_t<LOGFONT> cfg_expanded_track_font(GUID{0x123456B1, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -14;
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return lf;
}());
static cfg_int cfg_expanded_use_custom_fonts(GUID{0x123456B2, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);

// Compact mode font configuration
static cfg_struct_t<LOGFONT> cfg_compact_artist_font(GUID{0x123456C0, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -11;
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return lf;
}());
static cfg_struct_t<LOGFONT> cfg_compact_track_font(GUID{0x123456C1, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, []() {
    LOGFONT lf = {};
    lf.lfHeight = -14;
    lf.lfWeight = FW_BOLD;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    return lf;
}());
static cfg_int cfg_compact_use_custom_fonts(GUID{0x123456C2, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);

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

// Font configuration access functions
bool get_use_custom_fonts() {
    return cfg_use_custom_fonts != 0;
}

LOGFONT get_artist_font() {
    return cfg_artist_font.get_value();
}

LOGFONT get_track_font() {
    return cfg_track_font.get_value();
}

void set_artist_font(const LOGFONT& font) {
    cfg_artist_font = font;
    cfg_use_custom_fonts = 1;
}

void set_track_font(const LOGFONT& font) {
    cfg_track_font = font;
    cfg_use_custom_fonts = 1;
}

void reset_fonts() {
    cfg_use_custom_fonts = 0;
    // Set font configurations to new default values
    cfg_artist_font = get_default_font(true, 11);   // Artist: 11pt, regular
    cfg_track_font = get_default_font(false, 14);   // Track: 14pt, bold
}

// Control Panel font configuration access functions
bool get_cp_use_custom_fonts() {
    return cfg_cp_use_custom_fonts != 0;
}

LOGFONT get_cp_artist_font() {
    return cfg_cp_artist_font.get_value();
}

LOGFONT get_cp_track_font() {
    return cfg_cp_track_font.get_value();
}

void set_cp_artist_font(const LOGFONT& font) {
    cfg_cp_artist_font = font;
    cfg_cp_use_custom_fonts = 1;
}

void set_cp_track_font(const LOGFONT& font) {
    cfg_cp_track_font = font;
    cfg_cp_use_custom_fonts = 1;
}

void reset_cp_fonts() {
    // Set font configurations to new default values with larger sizes
    LOGFONT default_artist = get_default_font(true, 13);   // Artist: 13pt instead of 11pt
    LOGFONT default_track = get_default_font(false, 16);   // Track: 16pt instead of 14pt
    
    cfg_cp_artist_font = default_artist;
    cfg_cp_track_font = default_track;
    
    // Disable custom fonts so control panel uses defaults from get_default_font
    cfg_cp_use_custom_fonts = 0;
}

// Undocked mode font accessor functions
bool get_undocked_use_custom_fonts() {
    return cfg_undocked_use_custom_fonts != 0;
}

LOGFONT get_undocked_artist_font() {
    return cfg_undocked_artist_font.get_value();
}

LOGFONT get_undocked_track_font() {
    return cfg_undocked_track_font.get_value();
}

// Expanded mode font accessor functions
bool get_expanded_use_custom_fonts() {
    return cfg_expanded_use_custom_fonts != 0;
}

LOGFONT get_expanded_artist_font() {
    return cfg_expanded_artist_font.get_value();
}

LOGFONT get_expanded_track_font() {
    return cfg_expanded_track_font.get_value();
}

// Compact mode font accessor functions
bool get_compact_use_custom_fonts() {
    return cfg_compact_use_custom_fonts != 0;
}

LOGFONT get_compact_artist_font() {
    return cfg_compact_artist_font.get_value();
}

LOGFONT get_compact_track_font() {
    return cfg_compact_track_font.get_value();
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
    wcscpy_s(lf.lfFaceName, L"Segoe UI");
    
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
    reset_settings();
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
        CheckDlgButton(hwnd, IDC_DISABLE_MINIPLAYER, cfg_disable_miniplayer != 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_DISABLE_SLIDE_TO_SIDE, cfg_disable_slide_to_side != 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_ALWAYS_SLIDE_TO_SIDE, cfg_always_slide_to_side != 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_USE_ROUNDED_CORNERS, cfg_use_rounded_corners != 0 ? BST_CHECKED : BST_UNCHECKED);
        
        // Initialize popup position combobox (6 positions: left and right sides)
        HWND hCombo = GetDlgItem(hwnd, IDC_POPUP_POSITION_COMBO);
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Top Left");
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Middle Left");
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Bottom Left");
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Top Right");
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Middle Right");
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)L"Bottom Right");
        SendMessage(hCombo, CB_SETCURSEL, cfg_popup_position, 0);

        // Initialize popup duration combobox (1-10 seconds)
        HWND hDurationCombo = GetDlgItem(hwnd, IDC_POPUP_DURATION_COMBO);
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"1 second");
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"2 seconds");
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"3 seconds");
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"4 seconds");
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"5 seconds");
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"7 seconds");
        SendMessage(hDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"10 seconds");
        // Convert stored milliseconds to combo index
        int duration_index = 2; // Default to 3 seconds (index 2)
        int stored_duration = cfg_popup_duration;
        if (stored_duration <= 1000) duration_index = 0;
        else if (stored_duration <= 2000) duration_index = 1;
        else if (stored_duration <= 3000) duration_index = 2;
        else if (stored_duration <= 4000) duration_index = 3;
        else if (stored_duration <= 5000) duration_index = 4;
        else if (stored_duration <= 7000) duration_index = 5;
        else duration_index = 6;
        SendMessage(hDurationCombo, CB_SETCURSEL, duration_index, 0);

        // Initialize slide duration combobox
        HWND hSlideDurationCombo = GetDlgItem(hwnd, IDC_SLIDE_DURATION_COMBO);
        SendMessage(hSlideDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"Very Fast (100ms)");
        SendMessage(hSlideDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"Fast (200ms)");
        SendMessage(hSlideDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"Normal (300ms)");
        SendMessage(hSlideDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"Slow (400ms)");
        SendMessage(hSlideDurationCombo, CB_ADDSTRING, 0, (LPARAM)L"Very Slow (500ms)");
        
        // Convert stored slide duration to combo index
        int slide_index = 1; // Default to 200ms (index 1)
        int stored_slide_duration = cfg_slide_duration;
        if (stored_slide_duration <= 100) slide_index = 0;
        else if (stored_slide_duration <= 200) slide_index = 1;
        else if (stored_slide_duration <= 300) slide_index = 2;
        else if (stored_slide_duration <= 400) slide_index = 3;
        else slide_index = 4;
        SendMessage(hSlideDurationCombo, CB_SETCURSEL, slide_index, 0);
        
        // Initialize theme mode combobox
        HWND hThemeModeCombo = GetDlgItem(hwnd, IDC_THEME_MODE_COMBO);
        SendMessage(hThemeModeCombo, CB_ADDSTRING, 0, (LPARAM)L"Auto");
        SendMessage(hThemeModeCombo, CB_ADDSTRING, 0, (LPARAM)L"Dark");
        SendMessage(hThemeModeCombo, CB_ADDSTRING, 0, (LPARAM)L"Light");
        SendMessage(hThemeModeCombo, CB_SETCURSEL, cfg_theme_mode, 0);

        
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
        case IDC_USE_ROUNDED_CORNERS:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->on_changed();
            }
            break;
            
        case IDC_POPUP_POSITION_COMBO:
        case IDC_POPUP_DURATION_COMBO:
        case IDC_SLIDE_DURATION_COMBO:
        case IDC_THEME_MODE_COMBO:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                p_this->on_changed();
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
            
        case IDC_RESET_ALL_FONTS:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->reset_all_fonts_to_default();
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
    
    return (current_minimize_to_tray != cfg_always_minimize_to_tray) || 
           (current_show_popup != cfg_show_popup_notification) ||
           (current_disable_miniplayer != cfg_disable_miniplayer) ||
           (current_disable_slide != cfg_disable_slide_to_side) ||
           (current_popup_position != cfg_popup_position);
}

void tray_preferences::apply_settings() {
    if (m_hwnd) {
        cfg_always_minimize_to_tray = (IsDlgButtonChecked(m_hwnd, IDC_ALWAYS_MINIMIZE_TO_TRAY) == BST_CHECKED) ? 1 : 0;
        cfg_show_popup_notification = (IsDlgButtonChecked(m_hwnd, IDC_SHOW_POPUP_NOTIFICATION) == BST_CHECKED) ? 1 : 0;
        cfg_disable_miniplayer = (IsDlgButtonChecked(m_hwnd, IDC_DISABLE_MINIPLAYER) == BST_CHECKED) ? 1 : 0;
        cfg_disable_slide_to_side = (IsDlgButtonChecked(m_hwnd, IDC_DISABLE_SLIDE_TO_SIDE) == BST_CHECKED) ? 1 : 0;
        cfg_always_slide_to_side = (IsDlgButtonChecked(m_hwnd, IDC_ALWAYS_SLIDE_TO_SIDE) == BST_CHECKED) ? 1 : 0;
        cfg_use_rounded_corners = (IsDlgButtonChecked(m_hwnd, IDC_USE_ROUNDED_CORNERS) == BST_CHECKED) ? 1 : 0;
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

        // Notify tray manager and control panel of settings change
        tray_manager::get_instance().on_settings_changed();
        control_panel::get_instance().on_settings_changed();
    }
}

void tray_preferences::reset_settings() {
    if (m_hwnd) {
        CheckDlgButton(m_hwnd, IDC_ALWAYS_MINIMIZE_TO_TRAY, cfg_always_minimize_to_tray != 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(m_hwnd, IDC_SHOW_POPUP_NOTIFICATION, cfg_show_popup_notification != 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(m_hwnd, IDC_DISABLE_MINIPLAYER, cfg_disable_miniplayer != 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(m_hwnd, IDC_DISABLE_SLIDE_TO_SIDE, cfg_disable_slide_to_side != 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(m_hwnd, IDC_ALWAYS_SLIDE_TO_SIDE, cfg_always_slide_to_side != 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(m_hwnd, IDC_USE_ROUNDED_CORNERS, cfg_use_rounded_corners != 0 ? BST_CHECKED : BST_UNCHECKED);
        SendMessage(GetDlgItem(m_hwnd, IDC_POPUP_POSITION_COMBO), CB_SETCURSEL, cfg_popup_position, 0);
        SendMessage(GetDlgItem(m_hwnd, IDC_THEME_MODE_COMBO), CB_SETCURSEL, cfg_theme_mode, 0);
        
        update_font_displays();
    }
}

// Font management methods
void tray_preferences::update_font_displays() {
    if (!m_hwnd) return;
    
    // Update original artist font display
    if (get_use_custom_fonts()) {
        LOGFONT lf = get_artist_font();
        pfc::string8 font_desc = format_font_name(lf);
        uSetDlgItemText(m_hwnd, IDC_ARTIST_FONT_DISPLAY, font_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_ARTIST_FONT_DISPLAY, "Segoe UI, 11pt, Regular (Default)");
    }
    
    // Update original track font display
    if (get_use_custom_fonts()) {
        LOGFONT lf = get_track_font();
        pfc::string8 font_desc = format_font_name(lf);
        uSetDlgItemText(m_hwnd, IDC_TRACK_FONT_DISPLAY, font_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_TRACK_FONT_DISPLAY, "Segoe UI, 14pt, Bold (Default)");
    }
    
    // Update Docked Control Panel font displays
    if (get_cp_use_custom_fonts()) {
        LOGFONT artist_lf = get_cp_artist_font();
        pfc::string8 artist_desc = format_font_name(artist_lf);
        uSetDlgItemText(m_hwnd, IDC_DOCKED_ARTIST_DISPLAY, artist_desc);
        
        LOGFONT track_lf = get_cp_track_font();
        pfc::string8 track_desc = format_font_name(track_lf);
        uSetDlgItemText(m_hwnd, IDC_DOCKED_TRACK_DISPLAY, track_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_DOCKED_ARTIST_DISPLAY, "Segoe UI, 13pt, Regular (Default)");
        uSetDlgItemText(m_hwnd, IDC_DOCKED_TRACK_DISPLAY, "Segoe UI, 16pt, Bold (Default)");
    }
    
    // Update Undocked mode font displays
    if (cfg_undocked_use_custom_fonts) {
        LOGFONT artist_lf = cfg_undocked_artist_font.get_value();
        pfc::string8 artist_desc = format_font_name(artist_lf);
        uSetDlgItemText(m_hwnd, IDC_UNDOCKED_ARTIST_DISPLAY, artist_desc);
        
        LOGFONT track_lf = cfg_undocked_track_font.get_value();
        pfc::string8 track_desc = format_font_name(track_lf);
        uSetDlgItemText(m_hwnd, IDC_UNDOCKED_TRACK_DISPLAY, track_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_UNDOCKED_ARTIST_DISPLAY, "Segoe UI, 11pt, Regular (Default)");
        uSetDlgItemText(m_hwnd, IDC_UNDOCKED_TRACK_DISPLAY, "Segoe UI, 14pt, Bold (Default)");
    }
    
    // Update Expanded mode font displays
    if (cfg_expanded_use_custom_fonts) {
        LOGFONT artist_lf = cfg_expanded_artist_font.get_value();
        pfc::string8 artist_desc = format_font_name(artist_lf);
        uSetDlgItemText(m_hwnd, IDC_EXPANDED_ARTIST_DISPLAY, artist_desc);
        
        LOGFONT track_lf = cfg_expanded_track_font.get_value();
        pfc::string8 track_desc = format_font_name(track_lf);
        uSetDlgItemText(m_hwnd, IDC_EXPANDED_TRACK_DISPLAY, track_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_EXPANDED_ARTIST_DISPLAY, "Segoe UI, 11pt, Regular (Default)");
        uSetDlgItemText(m_hwnd, IDC_EXPANDED_TRACK_DISPLAY, "Segoe UI, 14pt, Bold (Default)");
    }
    
    // Update Compact mode font displays
    if (cfg_compact_use_custom_fonts) {
        LOGFONT artist_lf = cfg_compact_artist_font.get_value();
        pfc::string8 artist_desc = format_font_name(artist_lf);
        uSetDlgItemText(m_hwnd, IDC_COMPACT_ARTIST_DISPLAY, artist_desc);
        
        LOGFONT track_lf = cfg_compact_track_font.get_value();
        pfc::string8 track_desc = format_font_name(track_lf);
        uSetDlgItemText(m_hwnd, IDC_COMPACT_TRACK_DISPLAY, track_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_COMPACT_ARTIST_DISPLAY, "Segoe UI, 11pt, Regular (Default)");
        uSetDlgItemText(m_hwnd, IDC_COMPACT_TRACK_DISPLAY, "Segoe UI, 14pt, Bold (Default)");
    }
}

void tray_preferences::select_artist_font() {
    CHOOSEFONT cf = {};
    LOGFONT lf;
    
    // Get current font or default
    if (get_use_custom_fonts()) {
        lf = get_artist_font();
    } else {
        lf = get_default_font(true, 11);
    }
    
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = m_hwnd;
    cf.lpLogFont = &lf;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
    
    if (ChooseFont(&cf)) {
        set_artist_font(lf);
        update_font_displays();
        on_changed();
    }
}

void tray_preferences::select_track_font() {
    CHOOSEFONT cf = {};
    LOGFONT lf;
    
    // Get current font or default
    if (get_use_custom_fonts()) {
        lf = get_track_font();
    } else {
        lf = get_default_font(false, 14);
    }
    
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = m_hwnd;
    cf.lpLogFont = &lf;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
    
    if (ChooseFont(&cf)) {
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
    CHOOSEFONT cf = {};
    LOGFONT lf;
    
    // Get current font or default
    if (get_cp_use_custom_fonts()) {
        lf = get_cp_artist_font();
    } else {
        lf = get_default_font(true, 13);
    }
    
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = m_hwnd;
    cf.lpLogFont = &lf;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
    
    if (ChooseFont(&cf)) {
        set_cp_artist_font(lf);
        update_font_displays();
        on_changed();
    }
}

void tray_preferences::select_cp_track_font() {
    CHOOSEFONT cf = {};
    LOGFONT lf;
    
    // Get current font or default
    if (get_cp_use_custom_fonts()) {
        lf = get_cp_track_font();
    } else {
        lf = get_default_font(false, 14);
    }
    
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = m_hwnd;
    cf.lpLogFont = &lf;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
    
    if (ChooseFont(&cf)) {
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
    CHOOSEFONT cf = {};
    LOGFONT lf;
    
    // Get current font or default based on mode
    switch (mode) {
    case 1: // Undocked
        if (cfg_undocked_use_custom_fonts) {
            lf = is_artist ? cfg_undocked_artist_font.get_value() : cfg_undocked_track_font.get_value();
        } else {
            lf = get_default_font(is_artist, is_artist ? 11 : 14);
        }
        break;
    case 2: // Expanded
        if (cfg_expanded_use_custom_fonts) {
            lf = is_artist ? cfg_expanded_artist_font.get_value() : cfg_expanded_track_font.get_value();
        } else {
            lf = get_default_font(is_artist, is_artist ? 11 : 14);
        }
        break;
    case 3: // Compact
        if (cfg_compact_use_custom_fonts) {
            lf = is_artist ? cfg_compact_artist_font.get_value() : cfg_compact_track_font.get_value();
        } else {
            lf = get_default_font(is_artist, is_artist ? 11 : 14);
        }
        break;
    default:
        return;
    }
    
    cf.lStructSize = sizeof(CHOOSEFONT);
    cf.hwndOwner = m_hwnd;
    cf.lpLogFont = &lf;
    cf.Flags = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS;
    
    if (ChooseFont(&cf)) {
        // Save font based on mode
        switch (mode) {
        case 1: // Undocked
            if (is_artist) {
                cfg_undocked_artist_font = lf;
            } else {
                cfg_undocked_track_font = lf;
            }
            cfg_undocked_use_custom_fonts = 1;
            break;
        case 2: // Expanded
            if (is_artist) {
                cfg_expanded_artist_font = lf;
            } else {
                cfg_expanded_track_font = lf;
            }
            cfg_expanded_use_custom_fonts = 1;
            break;
        case 3: // Compact
            if (is_artist) {
                cfg_compact_artist_font = lf;
            } else {
                cfg_compact_track_font = lf;
            }
            cfg_compact_use_custom_fonts = 1;
            break;
        }
        
        update_font_displays();
        on_changed();
    }
}

void tray_preferences::reset_all_fonts_to_default() {
    // Reset Docked mode fonts
    cfg_cp_use_custom_fonts = 0;
    
    // Reset Undocked mode fonts
    cfg_undocked_use_custom_fonts = 0;
    
    // Reset Expanded mode fonts
    cfg_expanded_use_custom_fonts = 0;
    
    // Reset Compact mode fonts
    cfg_compact_use_custom_fonts = 0;
    
    // Also reset the popup notification fonts
    cfg_use_custom_fonts = 0;
    
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
    
    // Add tabs
    TCITEM tie = {};
    tie.mask = TCIF_TEXT;
    
    tie.pszText = const_cast<LPWSTR>(L"General");
    TabCtrl_InsertItem(hTab, 0, &tie);
    
    tie.pszText = const_cast<LPWSTR>(L"Fonts");
    TabCtrl_InsertItem(hTab, 1, &tie);
    
    // Select first tab
    TabCtrl_SetCurSel(hTab, 0);
    m_current_tab = 0;
}

void tray_preferences::switch_tab(int tab) {
    if (!m_hwnd) return;
    
    m_current_tab = tab;
    
    // General tab controls (including static text labels)
    int general_controls[] = {
        IDC_ALWAYS_MINIMIZE_TO_TRAY,
        IDC_STATIC_MINIMIZE_HELP,
        IDC_STATIC_WHEEL_HELP,
        IDC_SHOW_POPUP_NOTIFICATION,
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
        IDC_ALWAYS_SLIDE_TO_SIDE,
        // Window style options
        IDC_USE_ROUNDED_CORNERS,
        IDC_THEME_MODE_LABEL,
        IDC_THEME_MODE_COMBO
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
        // Reset button
        IDC_RESET_ALL_FONTS
    };
    
    // Show/hide General controls
    int show_general = (tab == 0) ? SW_SHOW : SW_HIDE;
    for (int id : general_controls) {
        HWND hCtrl = GetDlgItem(m_hwnd, id);
        if (hCtrl) ShowWindow(hCtrl, show_general);
    }
    
    // Show/hide Fonts controls
    int show_fonts = (tab == 1) ? SW_SHOW : SW_HIDE;
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
