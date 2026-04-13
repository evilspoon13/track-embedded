/**
 * WidgetHelpers.cpp    Widget Helper Utilities
 *
 * @author      Justin Busker '26
 *
 * @copyright   Texas A&M University
 */

#include "WidgetHelpers.h"
#include "Widgets.h"   // GaugeThreshold definition

Color ColorForValue(float value, const GaugeThreshold* th, int n) {
    if (n <= 0) return GREEN;
    for (int i = 0; i < n; i++)
        if (value <= th[i].value)
            return th[i].color;
    return th[n - 1].color;
}

WidgetLayout compute_layout(int gx, int gy, int wTiles, int hTiles,
                             float pad, float border, float scale) {
    const float base = BASE_TILE * scale;
    WidgetLayout lo;
    lo.x       = gx     * base;
    lo.y       = gy     * base;
    lo.w       = wTiles * base;
    lo.h       = hTiles * base;
    lo.padS    = pad    * scale;
    lo.borderS = border * scale;
    return lo;
}

void draw_panel(const WidgetLayout& lo, Color fill, Color border_color) {
    DrawRectangle(Px(lo.x), Px(lo.y), Px(lo.w), Px(lo.h), fill);
    DrawRectangleLinesEx(Rectangle{lo.x, lo.y, lo.w, lo.h}, lo.borderS, border_color);
}

bool alarm_active(bool alarm, float value, float critical) {
    if (!alarm) return false;
    if (value < critical) return false;
    // 2 Hz blink (0.5 s on, 0.5 s off) based on monotonic wall time.
    return ((int)(GetTime() * 2.0)) & 1;
}

void draw_alarm_overlay(const WidgetLayout& lo, bool active) {
    if (!active) return;
    const float t = 4.0f;  // overlay thickness (px @ scale=1 already baked into panel)
    DrawRectangleLinesEx(Rectangle{lo.x, lo.y, lo.w, lo.h}, t, RED);
}

float normalize_value(float value, float min_value, float max_value) {
    float denom = max_value - min_value;
    float p = (denom != 0.0f) ? ((value - min_value) / denom) : 0.0f;
    return ClampF(p, 0.0f, 1.0f);
}

int validated_tick_count(int tick_count) {
    return (tick_count < 2) ? 2 : tick_count;
}

const char* format_value(float value, int decimals) {
    const char* fmt = (decimals <= 0) ? "%.0f"
                    : (decimals == 1) ? "%.1f"
                    : (decimals == 2) ? "%.2f"
                                      : "%.3f";
    return TextFormat(fmt, value);
}

void draw_value_with_units(const Font& font,
                           const char* vstr,
                           const std::string& units,
                           float value_fs, float units_fs, float spacing,
                           float cx, float cy,
                           Color text_color, float scale) {
    const float gap = 6.0f * scale;

    Vector2 vSz = MeasureTextEx(font, vstr, value_fs, spacing);
    bool hasUnits = !units.empty();

    Vector2 uSz = {0, 0};
    if (hasUnits)
        uSz = MeasureTextEx(font, units.c_str(), units_fs, spacing);

    float blockH = vSz.y + (hasUnits ? gap + uSz.y : 0.0f);
    float blockTop = cy - blockH * 0.5f;

    DrawTextEx(font, vstr, {cx - vSz.x * 0.5f, blockTop}, value_fs, spacing, text_color);

    if (hasUnits) {
        float uY = blockTop + vSz.y + gap;
        DrawTextEx(font, units.c_str(), {cx - uSz.x * 0.5f, uY}, units_fs, spacing,
                   Color{text_color.r, text_color.g, text_color.b, 190});
    }
}
