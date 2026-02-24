#include "stdafx.h"
#include "artwork_bridge.h"

// Global function pointers
pfn_foo_artwork_search g_artwork_search = nullptr;
pfn_foo_artwork_get_bitmap g_artwork_get_bitmap = nullptr;
pfn_foo_artwork_is_loading g_artwork_is_loading = nullptr;
pfn_foo_artwork_set_callback g_artwork_set_callback = nullptr;
pfn_foo_artwork_remove_callback g_artwork_remove_callback = nullptr;

// Module handle for foo_artwork
static HMODULE g_foo_artwork_module = nullptr;

// Pending artwork from callback
static HBITMAP g_pending_artwork_bitmap = nullptr;
static bool g_has_pending_artwork = false;

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

// Callback function that receives artwork results from foo_artwork
static void artwork_result_callback(bool success, HBITMAP bitmap) {
    if (success && bitmap) {
        g_pending_artwork_bitmap = bitmap;
        g_has_pending_artwork = true;
        // The ARTWORK_POLL_TIMER in control_panel/popup_window will pick this up
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
    g_pending_artwork_bitmap = nullptr;
    g_has_pending_artwork = false;
}

void request_online_artwork(const char* artist, const char* title) {
    if (!g_artwork_search) {
        return;
    }

    // Clear any pending artwork from previous search
    g_pending_artwork_bitmap = nullptr;
    g_has_pending_artwork = false;

    g_artwork_search(artist, title);
}

bool has_pending_online_artwork() {
    return g_has_pending_artwork;
}

HBITMAP get_pending_online_artwork() {
    // Return an independent COPY of the bitmap. The caller owns the copy and must
    // DeleteObject it. This is necessary because foo_artwork owns the original and
    // can DeleteObject it at any time (e.g. when foo_nowbar re-requests artwork).
    HBITMAP copy = copy_hbitmap(g_pending_artwork_bitmap);
    g_has_pending_artwork = false;
    return copy;
}

HBITMAP get_last_online_artwork() {
    // Return an independent COPY of the last received bitmap.
    // Used to re-acquire artwork after mode switches that call cleanup_cover_art().
    return copy_hbitmap(g_pending_artwork_bitmap);
}
