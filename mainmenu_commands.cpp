// mainmenu_commands.cpp - Mainmenu commands for Tray Controls component
// Provides toolbar buttons for launching the MiniPlayer

#include "stdafx.h"
#include "control_panel.h"

// GUIDs for menu group and commands
// Generate unique GUIDs for this component
static const GUID guid_traycontrols_menu_group = { 0x8a7b3c4d, 0x5e6f, 0x4a1b, { 0x9c, 0x2d, 0x3e, 0x4f, 0x5a, 0x6b, 0x7c, 0x8d } };
static const GUID guid_launch_miniplayer = { 0x1a2b3c4d, 0x5e6f, 0x7a8b, { 0x9c, 0xad, 0xbe, 0xcf, 0xd0, 0xe1, 0xf2, 0x03 } };

// Register "Tray Controls" menu group under View menu
static mainmenu_group_popup_factory g_traycontrols_menu_group(
    guid_traycontrols_menu_group, 
    mainmenu_groups::view, 
    mainmenu_commands::sort_priority_dontcare, 
    "Tray Controls"
);

// Mainmenu commands implementation
class mainmenu_commands_traycontrols : public mainmenu_commands {
public:
    enum {
        cmd_launch_miniplayer = 0,
        cmd_total
    };
    
    t_uint32 get_command_count() override {
        return cmd_total;
    }
    
    GUID get_command(t_uint32 p_index) override {
        switch(p_index) {
            case cmd_launch_miniplayer: return guid_launch_miniplayer;
            default: uBugCheck();
        }
    }
    
    void get_name(t_uint32 p_index, pfc::string_base & p_out) override {
        switch(p_index) {
            case cmd_launch_miniplayer: p_out = "Launch MiniPlayer"; break;
            default: uBugCheck();
        }
    }
    
    bool get_description(t_uint32 p_index, pfc::string_base & p_out) override {
        switch(p_index) {
            case cmd_launch_miniplayer: 
                p_out = "Opens the MiniPlayer in Undocked mode at its previous position."; 
                return true;
            default: 
                return false;
        }
    }
    
    GUID get_parent() override {
        return guid_traycontrols_menu_group;
    }
    
    void execute(t_uint32 p_index, service_ptr_t<service_base> p_callback) override {
        switch(p_index) {
            case cmd_launch_miniplayer:
                control_panel::get_instance().show_undocked_miniplayer();
                break;
            default:
                uBugCheck();
        }
    }
};

// Register the mainmenu commands factory
static mainmenu_commands_factory_t<mainmenu_commands_traycontrols> g_mainmenu_commands_traycontrols_factory;
