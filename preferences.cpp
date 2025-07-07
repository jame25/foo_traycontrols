#include "stdafx.h"
#include "preferences.h"
#include "tray_manager.h"

// External declaration from main.cpp
extern HINSTANCE g_hIns;

// Configuration variables - stored in foobar2000's config system
static cfg_int cfg_always_minimize_to_tray(GUID{0x12345679, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 0);
static cfg_int cfg_show_popup_notification(GUID{0x12345681, 0x9abc, 0xdef0, {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0}}, 1);

// Access functions for the configuration
bool get_always_minimize_to_tray() {
    return cfg_always_minimize_to_tray != 0;
}

// Mouse wheel volume control removed - was causing system conflicts

bool get_show_popup_notification() {
    return cfg_show_popup_notification != 0;
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
        p_this->m_has_changes = false;
    } else {
        p_this = reinterpret_cast<tray_preferences*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (p_this == nullptr) return FALSE;
    
    switch (msg) {
    case WM_COMMAND:
        if (HIWORD(wp) == BN_CLICKED && (LOWORD(wp) == IDC_ALWAYS_MINIMIZE_TO_TRAY || LOWORD(wp) == IDC_SHOW_POPUP_NOTIFICATION)) {
            p_this->on_changed();
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
    
    return (current_minimize_to_tray != cfg_always_minimize_to_tray) || (current_show_popup != cfg_show_popup_notification);
}

void tray_preferences::apply_settings() {
    if (m_hwnd) {
        cfg_always_minimize_to_tray = (IsDlgButtonChecked(m_hwnd, IDC_ALWAYS_MINIMIZE_TO_TRAY) == BST_CHECKED) ? 1 : 0;
        cfg_show_popup_notification = (IsDlgButtonChecked(m_hwnd, IDC_SHOW_POPUP_NOTIFICATION) == BST_CHECKED) ? 1 : 0;
        
        // Notify tray manager of settings change
        tray_manager::get_instance().on_settings_changed();
    }
}

void tray_preferences::reset_settings() {
    if (m_hwnd) {
        CheckDlgButton(m_hwnd, IDC_ALWAYS_MINIMIZE_TO_TRAY, cfg_always_minimize_to_tray != 0 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(m_hwnd, IDC_SHOW_POPUP_NOTIFICATION, cfg_show_popup_notification != 0 ? BST_CHECKED : BST_UNCHECKED);
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
