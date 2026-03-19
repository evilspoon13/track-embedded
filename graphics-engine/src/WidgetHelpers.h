/**
 * WidgetHelpers.h      Widget Helper Utilities
 *
 * @author      Justin Busker '26
 *
 * @copyright   Texas A&M University
 */

#pragma once
#include "raylib.h"
#include <string>

// ── Low-level helpers ────────────────────────────────────────────────────────

static inline int   Px(float v)                      { return (int)(v + 0.5f); }
static inline float ClampF(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

struct GaugeThreshold;  // forward-declared in Widgets.h; defined there

Color ColorForValue(float value, const GaugeThreshold* th, int n);

// ── Layout ───────────────────────────────────────────────────────────────────

struct WidgetLayout {
    float x, y, w, h;
    float padS;
    float borderS;
};

WidgetLayout compute_layout(int gx, int gy, int wTiles, int hTiles,
                             float pad, float border, float scale);

void draw_panel(const WidgetLayout& lo, Color fill, Color border_color);

// ── Value helpers ─────────────────────────────────────────────────────────────

float       normalize_value(float value, float min_value, float max_value);
int         validated_tick_count(int tick_count);
const char* format_value(float value, int decimals);

// Draw centered value text + optional units label below it.
// cx/cy is the centre point.  units may be empty string.
void draw_value_with_units(const Font& font,
                           const char* vstr,
                           const std::string& units,
                           float value_fs, float units_fs, float spacing,
                           float cx, float cy,
                           Color text_color, float scale);
