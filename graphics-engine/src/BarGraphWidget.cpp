/**
 * BarGraphWidget.cpp   Bar Graph Widget
 *
 * @author      Justin Busker '26
 *
 * @copyright   Texas A&M University
 */

#include "Widgets.h"

void BarGraphWidget::Draw(const Font& font) const {
    auto lo = compute_layout(gx, gy, wTiles, hTiles, pad, border, scale);
    draw_panel(lo, panelFill, panelBorder);

    float p = normalize_value(value, minValue, maxValue);

    float left   = lo.x + lo.padS;
    float right  = lo.x + lo.w - lo.padS;
    float top    = lo.y + lo.padS;
    float bottom = lo.y + lo.h - lo.padS;

    float valueFs   = valueTextSize * scale;
    float unitsFs   = unitsTextSize * scale;
    float spacingS  = 1.0f * scale;
    bool  hasUnits  = !units.empty();

    float textBlockH = valueFs + (hasUnits ? unitsFs : 0.0f) + 10.0f * scale;
    float barBottom  = bottom - textBlockH;
    if (barBottom < top + 10.0f * scale)
        barBottom = top + 10.0f * scale;

    float bw = barWidth * scale;
    if (bw < 6.0f * scale) bw = 6.0f * scale;

    float barX = left + (right - left) * barXFrac - bw * 0.5f;
    if (barX < left)        barX = left;
    if (barX + bw > right)  barX = right - bw;

    Rectangle barRect  = {barX, top, bw, barBottom - top};
    DrawRectangleRounded(barRect, 0.25f, 8, barBackColor);

    float     fillH    = barRect.height * p;
    Rectangle fillRect = {barRect.x, barRect.y + (barRect.height - fillH), barRect.width, fillH};
    DrawRectangleRounded(fillRect, 0.25f, 8, ColorForValue(value, thresholds, thresholdCount));

    int   nTicks     = validated_tick_count(tickCount);
    float tickLen    = 8.0f * scale;
    float tickX0     = barRect.x + barRect.width + 8.0f * scale;
    float tickX1     = tickX0 + tickLen;
    float tickFs     = tickLabelSize * scale;
    float tickSpacing = 1.0f * scale;

    for (int i = 0; i < nTicks; i++) {
        float t  = (float)i / (float)(nTicks - 1);
        float v  = minValue + (maxValue - minValue) * t;
        float yy = barRect.y + (1.0f - t) * barRect.height;

        DrawLineEx({tickX0, yy}, {tickX1, yy}, 2.0f * scale, tickColor);
        if (showTickLabels) {
            const char* s  = TextFormat("%.0f", v);
            Vector2     sz = MeasureTextEx(font, s, tickFs, tickSpacing);
            DrawTextEx(font, s, {tickX1 + 6.0f * scale, yy - sz.y * 0.5f}, tickFs, tickSpacing, tickColor);
        }
    }

    const char* vstr = format_value(value, decimals);
    Vector2     vSz  = MeasureTextEx(font, vstr, valueFs, spacingS);
    float vX = lo.x + (lo.w - vSz.x) * 0.5f;
    float vY = barBottom + 6.0f * scale;
    DrawTextEx(font, vstr, {vX, vY}, valueFs, spacingS, textColor);

    if (hasUnits) {
        Vector2 uSz = MeasureTextEx(font, units.c_str(), unitsFs, spacingS);
        float   uX  = lo.x + (lo.w - uSz.x) * 0.5f;
        float   uY  = vY + vSz.y + 4.0f * scale;
        DrawTextEx(font, units.c_str(), {uX, uY}, unitsFs, spacingS,
                   Color{textColor.r, textColor.g, textColor.b, 190});
    }

    draw_alarm_overlay(lo, alarm_active(alarm, value, criticalThreshold));
}
