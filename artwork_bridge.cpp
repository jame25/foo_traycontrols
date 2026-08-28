#include "stdafx.h"
#include "artwork_bridge.h"
#include "control_panel.h"
#include "popup_window.h"
#include <mutex>

// Global function pointers
pfn_foo_artwork_search g_artwork_search = nullptr;
pfn_foo_artwork_get_bitmap g_artwork_get_bitmap = nullptr;
pfn_foo_artwork_is_loading g_artwork_is_loading = nullptr;
pfn_foo_artwork_set_callback g_artwork_set_callback = nullptr;
pfn_foo_artwork_remove_callback g_artwork_remove_callback = nullptr;

// Module handle for foo_artwork
static HMODULE g_foo_artwork_module = nullptr;

// Pending artwork from callback (guarded by g_pending_mutex)
static std::mutex g_pending_mutex;
static HBITMAP g_pending_artwork_bitmap = nullptr;
static bool g_has_pending_artwork_popup = false;
static bool g_has_pending_artwork_panel = false;

// Create an independent copy of an HBITMAP (caller owns the copy and must DeleteObject it)
static HBITMAP copy_hbitmap(HBITMAP source) {
    if (!source) return nullptr;

    BITMAP bm;
    if (!GetObject(source, sizeof(bm), &bm)) return nullptr;

    HDC screen_dc = GetDC(nullptr);
    HDC src_dc = CreateCompatibleDC(screen_dc);
    HDC dst_dc = CreateCompatibleDC(screen_dc);

    HBITMAP copy = CreateCompatibleBitmap(screen_dc, bm.bmWidth, bm.bmHeight);
    if (copy) {
        HBITMAP old_src = (HBITMAP)SelectObject(src_dc, source);
        HBITMAP old_dst = (HBITMAP)SelectObject(dst_dc, copy);

        BitBlt(dst_dc, 0, 0, bm.bmWidth, bm.bmHeight, src_dc, 0, 0, SRCCOPY);

        SelectObject(src_dc, old_src);
        SelectObject(dst_dc, old_dst);
    }

    DeleteDC(src_dc);
    DeleteDC(dst_dc);
    ReleaseDC(nullptr, screen_dc);
    return copy;
}

// Callback function that receives artwork results from foo_artwork.
// Called on foo_artwork's worker thread - must synchronize and marshal to main thread.
static void artwork_result_callback(bool success, HBITMAP bitmap) {
    if (success && bitmap) {
        HBITMAP copy = copy_hbitmap(bitmap);
        if (copy) {
            {
                std::lock_guard<std::mutex> lock(g_pending_mutex);
                if (g_pending_artwork_bitmap) {
                    DeleteObject(g_pending_artwork_bitmap);
                }
                g_pending_artwork_bitmap = copy;
                g_has_pending_artwork_popup = true;
                g_has_pending_artwork_panel = true;
            }

            // Marshal notification to main thread
            fb2k::inMainThread([]() {
                control_panel::get_instance().on_online_artwork_received();
                popup_window::get_instance().on_online_artwork_received();
            });
        }
    }
}

bool init_artwork_bridge() {
    // Try to get handle to already-loaded foo_artwork module
    g_foo_artwork_module = GetModuleHandleW(L"foo_artwork.dll");

    if (!g_foo_artwork_module) {
        return false;
    }

    // Resolve function pointers - these are extern "C" exports from foo_artwork
    g_artwork_search = (pfn_foo_artwork_search)
        GetProcAddress(g_foo_artwork_module, "foo_artwork_search");

    g_artwork_get_bitmap = (pfn_foo_artwork_get_bitmap)
        GetProcAddress(g_foo_artwork_module, "foo_artwork_get_bitmap");

    g_artwork_is_loading = (pfn_foo_artwork_is_loading)
        GetProcAddress(g_foo_artwork_module, "foo_artwork_is_loading");

    g_artwork_set_callback = (pfn_foo_artwork_set_callback)
        GetProcAddress(g_foo_artwork_module, "foo_artwork_set_callback");

    g_artwork_remove_callback = (pfn_foo_artwork_remove_callback)
        GetProcAddress(g_foo_artwork_module, "foo_artwork_remove_callback");

    // Register our callback to receive artwork results
    // foo_artwork now supports multiple callbacks, so this won't conflict with foo_nowbar
    if (g_artwork_set_callback) {
        g_artwork_set_callback(artwork_result_callback);
    }

    return g_artwork_search != nullptr;
}

void shutdown_artwork_bridge() {
    // Unregister our specific callback (multi-callback safe)
    if (g_artwork_remove_callback) {
        g_artwork_remove_callback(artwork_result_callback);
    } else if (g_artwork_set_callback) {
        g_artwork_set_callback(nullptr); // Fallback for older foo_artwork
    }
    std::lock_guard<std::mutex> lock(g_pending_mutex);
    if (g_pending_artwork_bitmap) {
        DeleteObject(g_pending_artwork_bitmap);
        g_pending_artwork_bitmap = nullptr;
    }
    g_has_pending_artwork_popup = false;
    g_has_pending_artwork_panel = false;
}

// Last requested artist & title for search deduplication
static std::string g_last_requested_artist;
static std::string g_last_requested_title;

void clear_pending_online_artwork() {
    std::lock_guard<std::mutex> lock(g_pending_mutex);
    if (g_pending_artwork_bitmap) {
        DeleteObject(g_pending_artwork_bitmap);
        g_pending_artwork_bitmap = nullptr;
    }
    g_has_pending_artwork_popup = false;
    g_has_pending_artwork_panel = false;
}

void request_online_artwork(const char* artist, const char* title) {
    if (!g_artwork_search) {
        init_artwork_bridge();
        if (!g_artwork_search) return;
    }

    const char* safe_artist = artist ? artist : "";
    const char* safe_title = title ? title : "";

    if (safe_artist[0] == '\0' && safe_title[0] == '\0') {
        return;
    }

    // Deduplicate: If already searching or searched for this exact artist & title and artwork is pending, don't re-issue search
    {
        std::lock_guard<std::mutex> lock(g_pending_mutex);
        if (g_last_requested_artist == safe_artist && g_last_requested_title == safe_title && (g_has_pending_artwork_popup || g_has_pending_artwork_panel)) {
            return;
        }
        g_last_requested_artist = safe_artist;
        g_last_requested_title = safe_title;
        if (g_pending_artwork_bitmap) {
            DeleteObject(g_pending_artwork_bitmap);
            g_pending_artwork_bitmap = nullptr;
        }
        g_has_pending_artwork_popup = false;
        g_has_pending_artwork_panel = false;
    }

    g_artwork_search(safe_artist, safe_title);
}

bool has_pending_online_artwork() {
    std::lock_guard<std::mutex> lock(g_pending_mutex);
    return g_has_pending_artwork_popup || g_has_pending_artwork_panel;
}

bool has_pending_online_artwork_popup() {
    std::lock_guard<std::mutex> lock(g_pending_mutex);
    return g_has_pending_artwork_popup;
}

bool has_pending_online_artwork_panel() {
    std::lock_guard<std::mutex> lock(g_pending_mutex);
    return g_has_pending_artwork_panel;
}

HBITMAP get_pending_online_artwork() {
    HBITMAP src = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_pending_mutex);
        src = g_pending_artwork_bitmap;
        g_has_pending_artwork_popup = false;
        g_has_pending_artwork_panel = false;
    }
    return copy_hbitmap(src);
}

HBITMAP get_pending_online_artwork_popup() {
    HBITMAP src = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_pending_mutex);
        if (g_has_pending_artwork_popup) {
            src = g_pending_artwork_bitmap;
            g_has_pending_artwork_popup = false;
        }
    }
    return copy_hbitmap(src);
}

HBITMAP get_pending_online_artwork_panel() {
    HBITMAP src = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_pending_mutex);
        if (g_has_pending_artwork_panel) {
            src = g_pending_artwork_bitmap;
            g_has_pending_artwork_panel = false;
        }
    }
    return copy_hbitmap(src);
}

HBITMAP get_last_online_artwork() {
    HBITMAP src = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_pending_mutex);
        src = g_pending_artwork_bitmap;
    }
    return copy_hbitmap(src);
}

HBITMAP get_current_online_artwork() {
    // Check if foo_artwork already has an active artwork bitmap available (e.g. displayed in main window)
    if (g_artwork_get_bitmap) {
        HBITMAP bmp = g_artwork_get_bitmap();
        if (bmp) {
            return copy_hbitmap(bmp);
        }
    }
    return nullptr;
}

bool is_online_artwork_loading() {
    if (g_artwork_is_loading) {
        return g_artwork_is_loading();
    }
    return false;
}
