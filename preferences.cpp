#include "stdafx.h"
#include "preferences.h"
#include "tray_manager.h"
#include "control_panel.h"

// External declaration from main.cpp
extern HINSTANCE g_hIns;

// Configuration variables - stored in foobar2000's config system
static cfg_int cfg_always_minimize_to_tray(GUID{0x12345679, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);
static cfg_int cfg_show_popup_notification(GUID{0x12345681, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 1);
static cfg_int cfg_popup_position(GUID{0x12345685, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0); // 0=Top Left, 1=Middle Left, 2=Bottom Left, 3=Top Right, 4=Middle Right, 5=Bottom Right
static cfg_int cfg_disable_miniplayer(GUID{0x12345686, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);
static cfg_int cfg_popup_duration(GUID{0x12345687, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 3000); // Default 3 seconds (3000ms)


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

// Helper function to get default LOGFONT
LOGFONT get_default_font(bool is_artist, int size) {
    LOGFONT lf = {};
    
    // Try multiple approaches to ensure correct font size
    // Method 1: Simple negative point size (original approach)
    lf.lfHeight = -size;
    
    // Method 2: Alternative - try DPI calculation if simple method fails
    if (size < 10) { // If we're getting constrained to small sizes
        HDC hdc = GetDC(nullptr);
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        lf.lfHeight = -MulDiv(size, dpi, 72);
        ReleaseDC(nullptr, hdc);
    }
    
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
    : m_hwnd(nullptr), m_callback(callback), m_has_changes(false) {
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
        
        // Initialize checkbox states
        CheckDlgButton(hwnd, IDC_ALWAYS_MINIMIZE_TO_TRAY, cfg_always_minimize_to_tray != 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_SHOW_POPUP_NOTIFICATION, cfg_show_popup_notification != 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwnd, IDC_DISABLE_MINIPLAYER, cfg_disable_miniplayer != 0 ? BST_CHECKED : BST_UNCHECKED);
        
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
        
        
        
        // Initialize font displays
        p_this->update_font_displays();
        
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
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->on_changed();
            }
            break;
            
        case IDC_POPUP_POSITION_COMBO:
        case IDC_POPUP_DURATION_COMBO:
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
            
        case IDC_CP_SELECT_ARTIST_FONT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_cp_artist_font();
            }
            break;
            
        case IDC_CP_SELECT_TRACK_FONT:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->select_cp_track_font();
            }
            break;
            
        case IDC_CP_RESET_FONTS:
            if (HIWORD(wp) == BN_CLICKED) {
                p_this->reset_cp_fonts_to_default();
            }
            break;
        }
        break;
        
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
    int current_popup_position = (int)SendMessage(GetDlgItem(m_hwnd, IDC_POPUP_POSITION_COMBO), CB_GETCURSEL, 0, 0);
    
    return (current_minimize_to_tray != cfg_always_minimize_to_tray) || 
           (current_show_popup != cfg_show_popup_notification) ||
           (current_disable_miniplayer != cfg_disable_miniplayer) ||
           (current_popup_position != cfg_popup_position);
}

void tray_preferences::apply_settings() {
    if (m_hwnd) {
        cfg_always_minimize_to_tray = (IsDlgButtonChecked(m_hwnd, IDC_ALWAYS_MINIMIZE_TO_TRAY) == BST_CHECKED) ? 1 : 0;
        cfg_show_popup_notification = (IsDlgButtonChecked(m_hwnd, IDC_SHOW_POPUP_NOTIFICATION) == BST_CHECKED) ? 1 : 0;
        cfg_disable_miniplayer = (IsDlgButtonChecked(m_hwnd, IDC_DISABLE_MINIPLAYER) == BST_CHECKED) ? 1 : 0;
        cfg_popup_position = (int)SendMessage(GetDlgItem(m_hwnd, IDC_POPUP_POSITION_COMBO), CB_GETCURSEL, 0, 0);

        // Convert duration combo index to milliseconds
        int duration_index = (int)SendMessage(GetDlgItem(m_hwnd, IDC_POPUP_DURATION_COMBO), CB_GETCURSEL, 0, 0);
        int duration_values[] = {1000, 2000, 3000, 4000, 5000, 7000, 10000};
        if (duration_index >= 0 && duration_index < 7) {
            cfg_popup_duration = duration_values[duration_index];
        }

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
        SendMessage(GetDlgItem(m_hwnd, IDC_POPUP_POSITION_COMBO), CB_SETCURSEL, cfg_popup_position, 0);
        
        
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
    
    // Update Control Panel font displays
    if (get_cp_use_custom_fonts()) {
        LOGFONT artist_lf = get_cp_artist_font();
        pfc::string8 artist_desc = format_font_name(artist_lf);
        uSetDlgItemText(m_hwnd, IDC_CP_ARTIST_FONT_DISPLAY, artist_desc);
        
        LOGFONT track_lf = get_cp_track_font();
        pfc::string8 track_desc = format_font_name(track_lf);
        uSetDlgItemText(m_hwnd, IDC_CP_TRACK_FONT_DISPLAY, track_desc);
    } else {
        uSetDlgItemText(m_hwnd, IDC_CP_ARTIST_FONT_DISPLAY, "Segoe UI, 13pt, Regular (Default)");
        uSetDlgItemText(m_hwnd, IDC_CP_TRACK_FONT_DISPLAY, "Segoe UI, 16pt, Bold (Default)");
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
