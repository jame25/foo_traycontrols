#pragma once

#define IDI_TRAY_ICON           101
#define IDI_RADIO_ICON          102
#define IDI_PLAY_ICON           103
#define IDI_PAUSE_ICON          104
#define IDI_PREVIOUS_ICON       105
#define IDI_NEXT_ICON           106
#define IDR_TRAY_PNG            107

#define IDD_PREFERENCES_TRAY    200
#define IDC_ALWAYS_MINIMIZE_TO_TRAY  201
#define IDC_ALWAYS_MINIMIZE_ON_CLOSE 202
#define IDC_MOUSE_WHEEL_VOLUME       203
#define IDC_SHOW_POPUP_NOTIFICATION  204
#define IDC_ARTIST_FONT_DISPLAY      205
#define IDC_SELECT_ARTIST_FONT       206
#define IDC_TRACK_FONT_DISPLAY       207
#define IDC_SELECT_TRACK_FONT        208
#define IDC_RESET_FONTS              209
#define IDC_POPUP_POSITION_LABEL     210
#define IDC_POPUP_POSITION_COMBO     211
#define IDC_DISABLE_MINIPLAYER       212
#define IDC_POPUP_DURATION_LABEL     220
#define IDC_POPUP_DURATION_COMBO     221
#define IDC_DISABLE_SLIDE_TO_SIDE    226
#define IDC_SLIDE_DURATION_LABEL     227
#define IDC_SLIDE_DURATION_COMBO     228
#define IDC_ALWAYS_SLIDE_TO_SIDE     229

// Tab control
#define IDC_TAB_CONTROL              222

// Static text labels for General tab (need IDs so we can hide/show them)
#define IDC_STATIC_MINIMIZE_HELP     223
#define IDC_STATIC_WHEEL_HELP        224
#define IDC_STATIC_MINIPLAYER_HELP   225

// Docked mode font controls (existing, renamed for clarity)
#define IDC_DOCKED_TITLE             230
#define IDC_DOCKED_ARTIST_LABEL      231
#define IDC_DOCKED_ARTIST_DISPLAY    232
#define IDC_DOCKED_ARTIST_SELECT     233
#define IDC_DOCKED_TRACK_LABEL       234
#define IDC_DOCKED_TRACK_DISPLAY     235
#define IDC_DOCKED_TRACK_SELECT      236

// Undocked mode font controls
#define IDC_UNDOCKED_TITLE           240
#define IDC_UNDOCKED_ARTIST_LABEL    241
#define IDC_UNDOCKED_ARTIST_DISPLAY  242
#define IDC_UNDOCKED_ARTIST_SELECT   243
#define IDC_UNDOCKED_TRACK_LABEL     244
#define IDC_UNDOCKED_TRACK_DISPLAY   245
#define IDC_UNDOCKED_TRACK_SELECT    246

// Expanded mode font controls
#define IDC_EXPANDED_TITLE           250
#define IDC_EXPANDED_ARTIST_LABEL    251
#define IDC_EXPANDED_ARTIST_DISPLAY  252
#define IDC_EXPANDED_ARTIST_SELECT   253
#define IDC_EXPANDED_TRACK_LABEL     254
#define IDC_EXPANDED_TRACK_DISPLAY   255
#define IDC_EXPANDED_TRACK_SELECT    256

// Compact mode font controls
#define IDC_COMPACT_TITLE            260
#define IDC_COMPACT_ARTIST_LABEL     261
#define IDC_COMPACT_ARTIST_DISPLAY   262
#define IDC_COMPACT_ARTIST_SELECT    263
#define IDC_COMPACT_TRACK_LABEL      264
#define IDC_COMPACT_TRACK_DISPLAY    265
#define IDC_COMPACT_TRACK_SELECT     266

// Reset all fonts button
#define IDC_RESET_ALL_FONTS          267

// Window corner style option
#define IDC_USE_ROUNDED_CORNERS      270

// Theme mode option (Dark/Light/Auto)
#define IDC_THEME_MODE_LABEL         271
#define IDC_THEME_MODE_COMBO         272

// Timer font controls (shared across Docked, Undocked, Compact modes)
#define IDC_TIMER_TITLE              280
#define IDC_TIMER_FONT_LABEL         281
#define IDC_TIMER_FONT_DISPLAY       282
#define IDC_TIMER_FONT_SELECT        283

// Display Format controls (General tab)
#define IDC_DISPLAY_FORMAT_GROUP     290
#define IDC_LINE1_FORMAT_LABEL       291
#define IDC_LINE1_FORMAT_EDIT        292
#define IDC_LINE2_FORMAT_LABEL       293
#define IDC_LINE2_FORMAT_EDIT        294


// Keep old IDs for backward compatibility (mapped to docked)
#define IDC_CP_ARTIST_FONT_LABEL         IDC_DOCKED_ARTIST_LABEL
#define IDC_CP_ARTIST_FONT_DISPLAY       IDC_DOCKED_ARTIST_DISPLAY
#define IDC_CP_SELECT_ARTIST_FONT        IDC_DOCKED_ARTIST_SELECT
#define IDC_CP_TRACK_FONT_LABEL          IDC_DOCKED_TRACK_LABEL
#define IDC_CP_TRACK_FONT_DISPLAY        IDC_DOCKED_TRACK_DISPLAY
#define IDC_CP_SELECT_TRACK_FONT         IDC_DOCKED_TRACK_SELECT
#define IDC_CP_RESET_FONTS               219

#define IDC_STATIC              -1

#define IDM_TRAY_PLAY           1001
#define IDM_TRAY_PAUSE          1002
#define IDM_TRAY_PREV           1003
#define IDM_TRAY_NEXT           1004
#define IDM_TRAY_RESTORE        1005
#define IDM_TRAY_EXIT           1006
