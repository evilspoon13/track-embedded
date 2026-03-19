/**
 * HorizontalBarGraphWidget.cpp  Horizontal Bar Graph Widget
 *
 * @author      Justin Busker '26
 *
 * @copyright   Texas A&M University
 */

#include "Widgets.h"

void HorizontalBarGraphWidget::Draw(const Font& font) const {
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
    bool  hasUnits  = (units && units[0] != '\0');

    float textBlockH    = valueFs + (hasUnits ? unitsFs : 0.0f) + 10.0f * scale;
    float barAreaBottom = bottom - textBlockH;
    if (barAreaBottom < top + 10.0f * scale)
        barAreaBottom = top + 10.0f * scale;

    float bh = barHeight * scale;
    if (bh < 6.0f * scale) bh = 6.0f * scale;

    float barY = top + ((barAreaBottom - top) * 0.35f) - bh * 0.5f;
    if (barY < top)                barY = top;
    if (barY + bh > barAreaBottom) barY = barAreaBottom - bh;

    Rectangle barRect  = {left, barY, right - left, bh};
    DrawRectangleRounded(barRect, 0.25f, 8, barBackColor);

    float     fillW    = barRect.width * p;
    Rectangle fillRect = {barRect.x, barRect.y, fillW, barRect.height};
    DrawRectangleRounded(fillRect, 0.25f, 8, ColorForValue(value, thresholds, thresholdCount));

    int   nTicks      = validated_tick_count(tickCount);
    float tickLen     = 8.0f * scale;
    float tickY0      = barRect.y + barRect.height + 8.0f * scale;
    float tickY1      = tickY0 + tickLen;
    float tickFs      = tickLabelSize * scale;
    float tickSpacing = 1.0f * scale;

    for (int i = 0; i < nTicks; i++) {
        float t  = (float)i / (float)(nTicks - 1);
        float v  = minValue + (maxValue - minValue) * t;
        float xx = barRect.x + t * barRect.width;

        DrawLineEx({xx, tickY0}, {xx, tickY1}, 2.0f * scale, tickColor);
        if (showTickLabels) {
            const char* s  = TextFormat("%.0f", v);
            Vector2     sz = MeasureTextEx(font, s, tickFs, tickSpacing);
            DrawTextEx(font, s, {xx - sz.x * 0.5f, tickY1 + 4.0f * scale}, tickFs, tickSpacing, tickColor);
        }
    }

    const char* vstr = format_value(value, decimals);
    Vector2     vSz  = MeasureTextEx(font, vstr, valueFs, spacingS);
    float vX = lo.x + (lo.w - vSz.x) * 0.5f;
    float vY = bottom - textBlockH + 6.0f * scale;
    DrawTextEx(font, vstr, {vX, vY}, valueFs, spacingS, textColor);

    if (hasUnits) {
        Vector2 uSz = MeasureTextEx(font, units, unitsFs, spacingS);
        float   uX  = lo.x + (lo.w - uSz.x) * 0.5f;
        float   uY  = vY + vSz.y + 4.0f * scale;
        DrawTextEx(font, units, {uX, uY}, unitsFs, spacingS,
                   Color{textColor.r, textColor.g, textColor.b, 190});
    }
}
