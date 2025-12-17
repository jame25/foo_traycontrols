#include "stdafx.h"
#include "svg_icon.h"

using namespace Gdiplus;

HICON svg_icon::create_tray_icon(int width, int height) {
    HICON icon = nullptr;
    
    // Initialize GDI+ if not already done
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Status status = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    
    if (status != Ok) {
        return nullptr;
    }
    
    try {
        // Create bitmap
        Bitmap bitmap(width, height, PixelFormat32bppARGB);
        if (bitmap.GetLastStatus() != Ok) {
            GdiplusShutdown(gdiplusToken);
            return nullptr;
        }
        
        Graphics graphics(&bitmap);
        if (graphics.GetLastStatus() != Ok) {
            GdiplusShutdown(gdiplusToken);
            return nullptr;
        }
        
        // Enable high quality rendering
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        
        // Render the foobar2000 logo
        render_foobar_logo(graphics, width, height);
        
        // Convert to HICON
        icon = bitmap_to_icon(&bitmap);
    }
    catch (...) {
        // Ensure cleanup on exception
        icon = nullptr;
    }
    
    // Cleanup GDI+
    GdiplusShutdown(gdiplusToken);
    
    return icon;
}

HBITMAP svg_icon::create_tray_bitmap(int width, int height) {
    HBITMAP hbitmap = nullptr;
    
    // Initialize GDI+ if not already done
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Status status = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
    
    if (status != Ok) {
        return nullptr;
    }
    
    try {
        // Create bitmap
        Bitmap bitmap(width, height, PixelFormat32bppARGB);
        if (bitmap.GetLastStatus() != Ok) {
            GdiplusShutdown(gdiplusToken);
            return nullptr;
        }
        
        Graphics graphics(&bitmap);
        if (graphics.GetLastStatus() != Ok) {
            GdiplusShutdown(gdiplusToken);
            return nullptr;
        }
        
        // Enable high quality rendering
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        
        // Render the foobar2000 logo
        render_foobar_logo(graphics, width, height);
        
        // Convert to HBITMAP
        hbitmap = bitmap_to_hbitmap(&bitmap);
    }
    catch (...) {
        // Ensure cleanup on exception
        hbitmap = nullptr;
    }
    
    // Cleanup GDI+
    GdiplusShutdown(gdiplusToken);
    
    return hbitmap;
}

void svg_icon::render_foobar_logo(Graphics& graphics, int width, int height) {
    // Scale factor for the design (SVG is 512x512)
    float scale = (width < height ? width : height) / 512.0f;
    
    // Clear with transparent background first
    graphics.Clear(Color(0, 0, 0, 0));
    
    // Create rounded rectangle background (#1B1817 with 15% radius)
    float radius = 512 * 0.15f * scale; // 15% of 512
    RectF bgRect(0, 0, (float)width, (float)height);
    GraphicsPath* bgPath = create_rounded_rect(bgRect, radius);
    
    // Fill background
    SolidBrush bgBrush(Color(255, 0x1B, 0x18, 0x17)); // #1B1817
    graphics.FillPath(&bgBrush, bgPath);
    
    // Create the main foobar2000 logo shape (white) - alien-like mask/face
    SolidBrush whiteBrush(Color(255, 255, 255, 255));
    
    // Main logo path based on SVG: Create the distinctive bulb/teardrop with wings
    GraphicsPath logoPath;
    
    // Scale all coordinates from 512x512 SVG coordinate system
    float cx = 256 * scale;  // Center X
    
    // Main body - create the distinctive foobar2000 shape
    // This is a complex curve, so we'll approximate it with bezier curves
    logoPath.StartFigure();
    
    // Start from bottom center (256, 473.5)
    PointF start(256 * scale, 473.5f * scale);
    
    // Create main bulb shape using multiple bezier curves to approximate the SVG path
    // Left side going up
    PointF cp1(186 * scale, 445 * scale);   // Left bottom curve
    PointF cp2(118 * scale, 350 * scale);   // Left middle
    PointF cp3(118 * scale, 290 * scale);   // Left top area
    PointF top1(197 * scale, 132 * scale);  // Left top connection
    
    logoPath.AddBezier(start, cp1, cp2, cp3);
    logoPath.AddBezier(cp3, PointF(150 * scale, 200 * scale), PointF(180 * scale, 150 * scale), top1);
    
    // Top connection area
    PointF top2(315 * scale, 132 * scale);  // Right top connection
    logoPath.AddLine(top1, top2);
    
    // Right side going down (mirror of left)
    PointF cp4(394 * scale, 290 * scale);   // Right top area
    PointF cp5(394 * scale, 350 * scale);   // Right middle  
    PointF cp6(326 * scale, 445 * scale);   // Right bottom curve
    
    logoPath.AddBezier(top2, PointF(332 * scale, 150 * scale), PointF(362 * scale, 200 * scale), cp4);
    logoPath.AddBezier(cp4, cp5, cp6, start);
    
    logoPath.CloseFigure();
    graphics.FillPath(&whiteBrush, &logoPath);
    
    // Add the two side wing/eye elements 
    // Left element (approximate position from SVG)
    GraphicsPath leftWing;
    RectF leftArea(110 * scale, 190 * scale, 60 * scale, 40 * scale);
    leftWing.AddEllipse(leftArea);
    graphics.FillPath(&whiteBrush, &leftWing);
    
    // Right element  
    GraphicsPath rightWing;
    RectF rightArea(342 * scale, 190 * scale, 60 * scale, 40 * scale);
    rightWing.AddEllipse(rightArea);
    graphics.FillPath(&whiteBrush, &rightWing);
    
    // Add the distinctive "eyes" - black ellipses inside the wings
    SolidBrush blackBrush(Color(255, 0x1B, 0x18, 0x17)); // Same as background
    
    // Left eye
    RectF leftEye(125 * scale, 200 * scale, 30 * scale, 20 * scale);
    graphics.FillEllipse(&blackBrush, leftEye);
    
    // Right eye  
    RectF rightEye(357 * scale, 200 * scale, 30 * scale, 20 * scale);
    graphics.FillEllipse(&blackBrush, rightEye);
    
    // Add the bottom arrow/chevron (more prominent)
    GraphicsPath arrowPath;
    float arrowCenterX = 256 * scale;
    float arrowY = 380 * scale;
    float arrowSize = 25 * scale;
    
    PointF arrowPoints[] = {
        PointF(arrowCenterX - arrowSize, arrowY - 10 * scale),
        PointF(arrowCenterX, arrowY + 10 * scale),
        PointF(arrowCenterX + arrowSize, arrowY - 10 * scale),
        PointF(arrowCenterX, arrowY - 20 * scale)  // Create pointed top
    };
    
    arrowPath.AddPolygon(arrowPoints, 4);
    graphics.FillPath(&whiteBrush, &arrowPath);
    
    // Cleanup
    delete bgPath;
}

GraphicsPath* svg_icon::create_rounded_rect(RectF rect, float radius) {
    GraphicsPath* path = new GraphicsPath();
    
    if (radius <= 0) {
        path->AddRectangle(rect);
        return path;
    }
    
    float diameter = radius * 2;
    RectF arcRect(rect.X, rect.Y, diameter, diameter);
    
    // Top left arc
    path->AddArc(arcRect, 180, 90);
    
    // Top right arc
    arcRect.X = rect.GetRight() - diameter;
    path->AddArc(arcRect, 270, 90);
    
    // Bottom right arc
    arcRect.Y = rect.GetBottom() - diameter;
    path->AddArc(arcRect, 0, 90);
    
    // Bottom left arc
    arcRect.X = rect.X;
    path->AddArc(arcRect, 90, 90);
    
    path->CloseFigure();
    return path;
}

HICON svg_icon::bitmap_to_icon(Bitmap* bitmap) {
    HICON icon = nullptr;
    
    // Convert GDI+ bitmap to HICON
    bitmap->GetHICON(&icon);
    
    return icon;
}

HBITMAP svg_icon::bitmap_to_hbitmap(Bitmap* bitmap) {
    HBITMAP hbitmap = nullptr;
    
    // Get the HBITMAP handle from GDI+ bitmap
    Color transparent(0, 0, 0, 0);
    bitmap->GetHBITMAP(transparent, &hbitmap);
    
    return hbitmap;
}