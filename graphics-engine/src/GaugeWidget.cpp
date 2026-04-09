/**
 * GaugeWidget.cpp      Gauge Widget
 *
 * @author      Justin Busker '26
 *
 * @copyright   Texas A&M University
 */

#include "Widgets.h"
#include <cmath>

void GaugeWidget::Draw(const Font& font) const {
    auto lo = compute_layout(gx, gy, wTiles, hTiles, pad, border, scale);
    draw_panel(lo, panelFill, panelBorder);

    Vector2 c = {lo.x + lo.w * 0.5f, lo.y + lo.h * 0.5f};

    float safe   = lo.padS + lo.borderS + ringMargin * scale;
    float radius = fminf(lo.w, lo.h) * 0.5f - safe;
    if (radius < 1.0f) radius = 1.0f;

    float thick  = ringThickness * scale;
    if (thick < 1.0f) thick = 1.0f;

    float outerR = radius;
    float innerR = radius - thick;
    if (innerR < 1.0f) innerR = 1.0f;

    float p     = normalize_value(value, minValue, maxValue);
    float sweep = endDeg - startDeg;

    DrawRing(c, innerR, outerR, startDeg, endDeg, 120, ringBackColor);

    Color progColor = ColorForValue(value, thresholds, thresholdCount);
    DrawRing(c, innerR, outerR, startDeg, startDeg + sweep * p, 120, progColor);

    int   nTicks     = validated_tick_count(tickCount);
    float tickOuter  = outerR - 2.0f * scale;
    float tickInner  = tickOuter - thick * 0.55f;
    float tickFs     = tickLabelSize * scale;
    float tickSpacing = 1.0f * scale;

    for (int i = 0; i < nTicks; i++) {
        float t      = (float)i / (float)(nTicks - 1);
        float angDeg = startDeg + sweep * t;
        float angRad = angDeg * DEG2RAD;

        Vector2 p0 = {c.x + cosf(angRad) * tickInner, c.y + sinf(angRad) * tickInner};
        Vector2 p1 = {c.x + cosf(angRad) * tickOuter, c.y + sinf(angRad) * tickOuter};
        DrawLineEx(p0, p1, 2.0f * scale, tickColor);

        if (showTickLabels) {
            float v   = minValue + (maxValue - minValue) * t;
            const char* s = TextFormat("%.0f", v);
            Vector2 sz    = MeasureTextEx(font, s, tickFs, tickSpacing);
            float labelR  = tickOuter + tickLabelOffset * scale;
            Vector2 lp    = {c.x + cosf(angRad) * labelR, c.y + sinf(angRad) * labelR};
            DrawTextEx(font, s, {lp.x - sz.x * 0.5f, lp.y - sz.y * 0.5f}, tickFs, tickSpacing, tickColor);
        }
    }

    const char* vstr = format_value(value, decimals);
    draw_value_with_units(font, vstr, units,
                          valueTextSize * scale, unitsTextSize * scale, 1.0f * scale,
                          c.x, c.y, textColor, scale);
}
