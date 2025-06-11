#pragma once

// Prevent socket conflicts
#define _WINSOCKAPI_
#define NOMINMAX

// Standard C++ headers first
#include <memory>

// Windows headers - include COM definitions
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <objbase.h>
#include <unknwn.h>
#include <shellapi.h>
#include <commctrl.h>
#include <mmsystem.h>

// Include the full foobar2000 SDK
#include "lib/foobar2000_SDK/foobar2000/SDK/foobar2000.h"
#include "lib/foobar2000_SDK/foobar2000/SDK/playback_control.h"
#include "lib/foobar2000_SDK/foobar2000/SDK/play_callback.h"
#include "lib/foobar2000_SDK/foobar2000/SDK/metadb_handle.h"
#include "lib/foobar2000_SDK/foobar2000/SDK/file_info.h"
#include "lib/foobar2000_SDK/foobar2000/SDK/file_info_impl.h"