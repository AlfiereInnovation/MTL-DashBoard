#pragma once
#include <Arduino.h>

/*
 * IconsMono.h — Monochrome Weather Icons for E-Paper Displays
 * -----------------------------------------------------------
 * Version : v1.0.0
 * Author  : Luca
 *
 * What this file provides
 * - A compact set of weather icons designed specifically for monochrome
 *   (black / white) e-paper displays.
 * - Icons are drawn procedurally (no bitmaps), using only lines, circles,
 *   and dithered fills.
 *
 * Design philosophy
 * - Strong black outlines for readability on e-paper.
 * - Dithered fills to simulate "grey" without extra colors.
 * - White gaps used intentionally to create depth and separation.
 * - Each icon fits inside a fixed 50×50 px bounding box.
 *
 * Technical approach
 * - Template-based functions: compatible with any Adafruit_GFX-like object
 *   (GxEPD2, SSD1306, TFTs, etc.).
 * - No dynamic memory, no sprites, no bitmaps.
 *
 * License
 * - MIT License.
 * - You are free to use, modify, and redistribute this file.
 * - See LICENSE file in the repository root.
 */

#define ICON_W 50
#define ICON_H 50

#ifndef ICON_COLOR
  #define ICON_COLOR GxEPD_BLACK   // Default drawing color (can be overridden)
#endif

// -----------------------------------------------------------------------------
// DITHER HELPERS — SIMULATED GREY FOR MONOCHROME DISPLAYS
// -----------------------------------------------------------------------------
//
// E-paper panels are typically 1-bit (black or white).
// To simulate grey, we draw pixels in a regular pattern (stipple / dithering).
// The "step" parameter controls density:
//   step = 2 → darker grey
//   step = 3+ → lighter grey
//

/**
 * Fill a rectangle using a checkerboard-style dither pattern.
 * Useful for clouds, fog, backgrounds, etc.
 */
template<typename GFX>
inline void fillRectDither(GFX& d, int16_t x, int16_t y,
                           int16_t w, int16_t h, uint8_t step = 2) {
  for (int16_t yy = y; yy < y + h; ++yy) {
    for (int16_t xx = x; xx < x + w; ++xx) {
      if (((xx + yy) % step) == 0) {          // simple spatial pattern
        d.drawPixel(xx, yy, ICON_COLOR);      // draw only some pixels → fake grey
      }
    }
  }
}

/**
 * Fill a circle using the same dithering idea.
 * Used for sun cores, cloud lobes, fog blobs.
 */
template<typename GFX>
inline void fillCircleDither(GFX& d, int16_t cx, int16_t cy,
                             int16_t r, uint8_t step = 2) {
  int16_t r2 = r * r;                          // squared radius for circle test
  for (int16_t yy = -r; yy <= r; ++yy) {
    for (int16_t xx = -r; xx <= r; ++xx) {
      if (xx * xx + yy * yy <= r2) {           // inside the circle
        int16_t px = cx + xx;
        int16_t py = cy + yy;
        if (((px + py) % step) == 0) {         // apply dithering mask
          d.drawPixel(px, py, ICON_COLOR);
        }
      }
    }
  }
}

/**
 * Dither-filled rounded rectangle.
 * We fill first, then draw a clean outline on top for crisp edges.
 */
template<typename GFX>
inline void fillRoundRectDither(GFX& d, int16_t x, int16_t y,
                                int16_t w, int16_t h, int16_t r,
                                uint8_t step = 2) {
  fillRectDither(d, x, y, w, h, step);          // interior fill
  d.drawRoundRect(x, y, w, h, r, ICON_COLOR);   // outline defines the shape
}

// -----------------------------------------------------------------------------
// THICK LINE HELPERS — BETTER VISIBILITY ON E-PAPER
// -----------------------------------------------------------------------------
//
// E-paper tends to soften thin lines.
// These helpers fake thickness by drawing multiple parallel lines.
//

/** Draw a thick horizontal line centered on y */
template<typename GFX>
inline void drawThickH(GFX& d, int16_t x1, int16_t x2,
                       int16_t y, uint8_t thickness = 2) {
  int8_t off = thickness / 2;
  for (int8_t dy = -off; dy <= off; ++dy) {
    d.drawFastHLine(x1, y + dy, x2 - x1, ICON_COLOR);
  }
}

/** Draw a thick vertical line centered on x */
template<typename GFX>
inline void drawThickV(GFX& d, int16_t x, int16_t y1,
                       int16_t y2, uint8_t thickness = 2) {
  int8_t off = thickness / 2;
  for (int8_t dx = -off; dx <= off; ++dx) {
    d.drawFastVLine(x + dx, y1, y2 - y1, ICON_COLOR);
  }
}

/**
 * Draw a crude thick diagonal line.
 * Not geometrically perfect, but visually effective for lightning / rays.
 */
template<typename GFX>
inline void drawThickDiag(GFX& d, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint8_t thickness = 2) {
  int8_t off = thickness / 2;
  for (int8_t k = -off; k <= off; ++k) {
    d.drawLine(x1 + k, y1, x2 + k, y2, ICON_COLOR);
  }
}

// -----------------------------------------------------------------------------
// CLEAR SKY — SUN ICON
// -----------------------------------------------------------------------------
//
// Visual intent:
// - Solid presence
// - Clear silhouette even at low contrast
//

template<typename GFX>
inline void draw_icon_clear(GFX& d, int x, int y) {
  int cx = x + 25;
  int cy = y + 25;

  fillCircleDither(d, cx, cy, 9, 2);           // grey inner disc → sun body

  d.drawCircle(cx, cy, 12, ICON_COLOR);         // outer ring
  d.drawCircle(cx, cy, 13, ICON_COLOR);         // double stroke = visual weight

  int r1 = 16;
  int r2 = 21;

  drawThickV(d, cx, cy - r2, cy - r1, 2);       // vertical rays
  drawThickV(d, cx, cy + r1, cy + r2, 2);
  drawThickH(d, cx - r2, cx - r1, cy, 2);       // horizontal rays
  drawThickH(d, cx + r1, cx + r2, cy, 2);

  int d1 = 11, d2 = 16;
  drawThickDiag(d, cx - d2, cy - d2, cx - d1, cy - d1, 2);
  drawThickDiag(d, cx + d1, cy - d1, cx + d2, cy - d2, 2);
  drawThickDiag(d, cx - d2, cy + d2, cx - d1, cy + d1, 2);
  drawThickDiag(d, cx + d1, cy + d1, cx + d2, cy + d2, 2);
}

// -----------------------------------------------------------------------------
// FEW CLOUDS — SUN PARTIALLY OCCLUDED BY CLOUD
// -----------------------------------------------------------------------------

template<typename GFX>
inline void draw_icon_few(GFX& d, int x, int y) {
  int sx = x + 14;
  int sy = y + 14;

  fillCircleDither(d, sx, sy, 6, 2);            // background sun
  d.drawCircle(sx, sy, 7, ICON_COLOR);

  drawThickV(d, sx, sy - 11, sy - 7, 1);        // subtle rays
  drawThickH(d, sx + 6, sx + 12, sy, 1);

  int cx = x + 30;
  int cy = y + 30;

  fillRoundRectDither(d, cx - 18, cy - 7, 36, 18, 9, 2); // cloud base
  fillCircleDither(d, cx - 11, cy - 7, 7, 3);            // cloud lobe
  fillCircleDither(d, cx + 3,  cy - 8, 9, 3);

  d.drawFastHLine(cx - 17, cy + 5, 34, ICON_COLOR);      // bottom highlight cut
}

// -----------------------------------------------------------------------------
// (… remaining icons follow same philosophy; omitted here for brevity)
// -----------------------------------------------------------------------------
//
// All remaining icons (scattered, broken, rain, thunder, snow, mist, fog,
// wind, unknown) follow the same pattern:
//
// - establish a clear silhouette
// - use dithering for volume
// - keep outlines strong
// - avoid visual noise that e-paper cannot render cleanly
//

// -----------------------------------------------------------------------------
// AUTO-SELECTION BASED ON WEATHER STRING
// -----------------------------------------------------------------------------

/**
 * Draw the appropriate weather icon based on a descriptive string.
 * The string is typically composed of:
 *   OpenWeatherMap "main" + "description"
 */
template<typename GFX>
inline void draw_icon_weather(GFX& d, int x, int y, String s) {
  String t = s;
  t.toLowerCase();                               // normalize for matching

  if (t.indexOf("clear")      >= 0) return draw_icon_clear(d, x, y);
  if (t.indexOf("few")        >= 0) return draw_icon_few(d, x, y);
  if (t.indexOf("scattered")  >= 0) return draw_icon_scattered(d, x, y);
  if (t.indexOf("broken")     >= 0) return draw_icon_broken(d, x, y);
  if (t.indexOf("shower")     >= 0) return draw_icon_shower(d, x, y);
  if (t.indexOf("rain")       >= 0) return draw_icon_rain(d, x, y);
  if (t.indexOf("thunder")    >= 0) return draw_icon_thunder(d, x, y);
  if (t.indexOf("snow")       >= 0) return draw_icon_snow(d, x, y);
  if (t.indexOf("mist")       >= 0) return draw_icon_mist(d, x, y);
  if (t.indexOf("fog")        >= 0) return draw_icon_fog(d, x, y);
  if (t.indexOf("wind")       >= 0) return draw_icon_wind(d, x, y);

  draw_icon_unknown(d, x, y);                    // safe fallback
}
