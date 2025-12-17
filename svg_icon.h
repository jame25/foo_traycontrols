#pragma once

#include "stdafx.h"
#include <gdiplus.h>

// SVG-based icon renderer for foobar2000 tray controls
class svg_icon {
public:
    // Create HICON from SVG data at specified size
    static HICON create_tray_icon(int width, int height);
    
    // Create HBITMAP from SVG data at specified size  
    static HBITMAP create_tray_bitmap(int width, int height);
    
private:
    // Render the foobar2000 logo using GDI+
    static void render_foobar_logo(Gdiplus::Graphics& graphics, int width, int height);
    
    // Helper to create rounded rectangle path
    static Gdiplus::GraphicsPath* create_rounded_rect(Gdiplus::RectF rect, float radius);
    
    // Convert GDI+ bitmap to HICON
    static HICON bitmap_to_icon(Gdiplus::Bitmap* bitmap);
    
    // Convert GDI+ bitmap to HBITMAP
    static HBITMAP bitmap_to_hbitmap(Gdiplus::Bitmap* bitmap);
};