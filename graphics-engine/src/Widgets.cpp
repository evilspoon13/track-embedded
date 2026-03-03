#include "Widgets.h"
#include <cmath>

static inline int Px(float v) { return (int)(v + 0.5f); }

static inline float ClampF(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline Color ColorForValue(float value, const GaugeThreshold* th, int n)
{
    if (n <= 0) return GREEN;
    for (int i = 0; i < n; i++)
        if (value <= th[i].value) return th[i].color;
    return th[n - 1].color;
}

void NumberWidget::Draw(const Font& font) const
{
    const float base = BASE_TILE * scale;
    const float x = gx * base;
    const float y = gy * base;
    const float w = wTiles * base;
    const float h = hTiles * base;

    const float padS       = pad * scale;
    const float borderS    = border * scale;
    const float labelSizeS = labelSize * scale;
    const float valueSizeS = valueSize * scale;
    const float spacingS   = 1.0f * scale;
    const float gapS       = gapBelowLabel * scale;

    DrawRectangle(Px(x), Px(y), Px(w), Px(h), panelFill);
    DrawRectangleLinesEx(Rectangle{ x, y, w, h }, borderS, panelBorder);

    Vector2 labelSz = MeasureTextEx(font, label.c_str(), labelSizeS, spacingS);
    float labelX = x + (w - labelSz.x) * 0.5f;
    float labelY = y + padS;
    DrawTextEx(font, label.c_str(), Vector2{ labelX, labelY }, labelSizeS, spacingS, RAYWHITE);

    const char* vstr = TextFormat("%d", value);
    Vector2 valueSz = MeasureTextEx(font, vstr, valueSizeS, spacingS);

    float contentTop = labelY + labelSz.y + gapS;
    float contentBottom = y + h - padS;
    float contentH = contentBottom - contentTop;
    if (contentH < 1) contentH = 1;

    float valueX = x + (w - valueSz.x) * 0.5f;
    float valueY = contentTop + (contentH - valueSz.y) * 0.5f;

    DrawTextEx(font, vstr, Vector2{ valueX, valueY }, valueSizeS, spacingS, valueColor);
}

void IndicatorLight::Draw(const Font& font) const
{
    const float base = BASE_TILE * scale;
    const float x = gx * base;
    const float y = gy * base;
    const float w = wTiles * base;
    const float h = hTiles * base;

    const float padS       = pad * scale;
    const float borderS    = border * scale;
    const float labelSizeS = labelSize * scale;
    const float spacingS   = 1.0f * scale;
    const float gapS       = gapBelowLabel * scale;

    DrawRectangle(Px(x), Px(y), Px(w), Px(h), panelFill);
    DrawRectangleLinesEx(Rectangle{ x, y, w, h }, borderS, panelBorder);

    Vector2 labelSz = MeasureTextEx(font, label.c_str(), labelSizeS, spacingS);
    float labelX = x + (w - labelSz.x) * 0.5f;
    float labelY = y + padS;
    DrawTextEx(font, label.c_str(), Vector2{ labelX, labelY }, labelSizeS, spacingS, RAYWHITE);

    float contentTop = labelY + labelSz.y + gapS;
    float contentBottom = y + h - padS;
    float contentH = contentBottom - contentTop;
    if (contentH < 1) contentH = 1;

    Vector2 c = Vector2{ x + w * 0.5f, contentTop + contentH * 0.5f };

    float maxR_byWidth  = (fminf(w, h) * 0.5f) - padS;
    float maxR_byHeight = (contentH * 0.5f) - padS;
    float radius = (maxR_byWidth < maxR_byHeight) ? maxR_byWidth : maxR_byHeight;
    if (radius < 1) radius = 1;

    Color fill = on ? onColor : offColor;
    if (!on) fill.a = 120;

    DrawCircleV(c, radius, fill);
    DrawCircleLines((int)c.x, (int)c.y, radius, Color{255, 255, 255, 80});
}

void GaugeWidget::Draw(const Font& font) const
{
    const float base = BASE_TILE * scale;
    const float x = gx * base;
    const float y = gy * base;
    const float w = wTiles * base;
    const float h = hTiles * base;

    const float padS    = pad * scale;
    const float borderS = border * scale;

    DrawRectangle(Px(x), Px(y), Px(w), Px(h), panelFill);
    DrawRectangleLinesEx(Rectangle{ x, y, w, h }, borderS, panelBorder);

    Vector2 c = Vector2{ x + w * 0.5f, y + h * 0.5f };

    float safe = padS + borderS + (12.0f * scale);
    float radius = (fminf(w, h) * 0.5f) - safe;
    if (radius < 1.0f) radius = 1.0f;

    float thick = ringThickness * scale;
    if (thick < 1.0f) thick = 1.0f;

    float outerR = radius;
    float innerR = radius - thick;
    if (innerR < 1.0f) innerR = 1.0f;

    float denom = (maxValue - minValue);
    float p = (denom != 0.0f) ? ((value - minValue) / denom) : 0.0f;
    p = ClampF(p, 0.0f, 1.0f);

    float sweep = (endDeg - startDeg);

    DrawRing(c, innerR, outerR, startDeg, endDeg, 120, ringBackColor);

    Color progColor = ColorForValue(value, thresholds, thresholdCount);
    float progEnd = startDeg + sweep * p;
    DrawRing(c, innerR, outerR, startDeg, progEnd, 120, progColor);

    int nTicks = tickCount;
    if (nTicks < 2) nTicks = 2;

    float tickOuter = outerR - (2.0f * scale);
    float tickInner = tickOuter - (thick * 0.55f);

    float tickFs = 9.0f * scale;
    float tickSpacing = 1.0f * scale;

    for (int i = 0; i < nTicks; i++)
    {
        float t = (float)i / (float)(nTicks - 1);
        float angDeg = startDeg + sweep * t;
        float angRad = angDeg * DEG2RAD;

        Vector2 p0 = Vector2{ c.x + cosf(angRad) * tickInner, c.y + sinf(angRad) * tickInner };
        Vector2 p1 = Vector2{ c.x + cosf(angRad) * tickOuter, c.y + sinf(angRad) * tickOuter };

        DrawLineEx(p0, p1, 2.0f * scale, tickColor);

        if (showTickLabels)
        {
            float v = minValue + (maxValue - minValue) * t;
            const char* s = TextFormat("%.0f", v);
            Vector2 sz = MeasureTextEx(font, s, tickFs, tickSpacing);

            float labelR = tickOuter + (8.0f * scale);
            Vector2 lp = Vector2{ c.x + cosf(angRad) * labelR, c.y + sinf(angRad) * labelR };

            DrawTextEx(font, s, Vector2{ lp.x - sz.x * 0.5f, lp.y - sz.y * 0.5f }, tickFs, tickSpacing, tickColor);
        }
    }

    const char* fmt = (decimals <= 0) ? "%.0f" :
                      (decimals == 1) ? "%.1f" :
                      (decimals == 2) ? "%.2f" : "%.3f";
    const char* vstr = TextFormat(fmt, value);

    float valueSizeS = valueTextSize * scale;
    float unitsSizeS = unitsTextSize * scale;
    float spacingS   = 1.0f * scale;

    Vector2 vSz = MeasureTextEx(font, vstr, valueSizeS, spacingS);

    bool hasUnits = !units.empty();
    float yOffset = hasUnits ? (unitsSizeS * 0.5f) : 0.0f;

    Vector2 vPos = Vector2{ c.x - vSz.x * 0.5f, c.y - vSz.y * 0.5f - yOffset };
    DrawTextEx(font, vstr, vPos, valueSizeS, spacingS, textColor);

    if (hasUnits)
    {
        Vector2 uSz = MeasureTextEx(font, units.c_str(), unitsSizeS, spacingS);
        Vector2 uPos = Vector2{ c.x - uSz.x * 0.5f, vPos.y + vSz.y - (2.0f * scale) };
        DrawTextEx(font, units.c_str(), uPos, unitsSizeS, spacingS, Color{ textColor.r, textColor.g, textColor.b, 190 });
    }
}

void BarGraphWidget::Draw(const Font& font) const
{
    const float base = BASE_TILE * scale;
    const float x = gx * base;
    const float y = gy * base;
    const float w = wTiles * base;
    const float h = hTiles * base;

    const float padS    = pad * scale;
    const float borderS = border * scale;

    DrawRectangle(Px(x), Px(y), Px(w), Px(h), panelFill);
    DrawRectangleLinesEx(Rectangle{ x, y, w, h }, borderS, panelBorder);

    // Compute progress
    float denom = (maxValue - minValue);
    float p = (denom != 0.0f) ? ((value - minValue) / denom) : 0.0f;
    p = ClampF(p, 0.0f, 1.0f);

    // Content area inside panel
    float left   = x + padS;
    float right  = x + w - padS;
    float top    = y + padS;
    float bottom = y + h - padS;

    // Reserve bottom area for value+units
    float valueFs = valueTextSize * scale;
    float unitsFs = unitsTextSize * scale;
    float spacingS = 1.0f * scale;

    bool hasUnits = !units.empty();

    float textBlockH = valueFs + (hasUnits ? unitsFs : 0.0f) + (10.0f * scale);
    float barTop = top;
    float barBottom = bottom - textBlockH;
    if (barBottom < barTop + 10.0f * scale) barBottom = barTop + 10.0f * scale;

    // Bar dimensions
    float bw = barWidth * scale;
    if (bw < 6.0f * scale) bw = 6.0f * scale;

    float barX = left + (right - left) * barXFrac - bw * 0.5f;
    if (barX < left) barX = left;
    if (barX + bw > right) barX = right - bw;

    // Bar background (empty)
    Rectangle barRect = Rectangle{ barX, barTop, bw, barBottom - barTop };
    DrawRectangleRounded(barRect, 0.25f, 8, barBackColor);

    // Filled portion from bottom up
    float fillH = barRect.height * p;
    Rectangle fillRect = Rectangle{ barRect.x, barRect.y + (barRect.height - fillH), barRect.width, fillH };

    Color fillColor = ColorForValue(value, thresholds, thresholdCount);
    DrawRectangleRounded(fillRect, 0.25f, 8, fillColor);

    // Ticks and labels on the right side of the bar
    int nTicks = tickCount;
    if (nTicks < 2) nTicks = 2;

    float tickLen = 8.0f * scale;
    float tickX0 = barRect.x + barRect.width + (8.0f * scale);
    float tickX1 = tickX0 + tickLen;

    float tickFs = tickLabelSize * scale;
    float tickSpacing = 1.0f * scale;

    for (int i = 0; i < nTicks; i++)
    {
        float t = (float)i / (float)(nTicks - 1);
        float v = minValue + (maxValue - minValue) * t;

        // y position: top corresponds to max
        float yy = barRect.y + (1.0f - t) * barRect.height;

        DrawLineEx(Vector2{ tickX0, yy }, Vector2{ tickX1, yy }, 2.0f * scale, tickColor);

        if (showTickLabels)
        {
            const char* s = TextFormat("%.0f", v);
            Vector2 sz = MeasureTextEx(font, s, tickFs, tickSpacing);
            DrawTextEx(font, s, Vector2{ tickX1 + (6.0f * scale), yy - sz.y * 0.5f }, tickFs, tickSpacing, tickColor);
        }
    }

    // Center value text at bottom (like your gauge center)
    const char* fmt = (decimals <= 0) ? "%.0f" :
                      (decimals == 1) ? "%.1f" :
                      (decimals == 2) ? "%.2f" : "%.3f";
    const char* vstr = TextFormat(fmt, value);

    Vector2 vSz = MeasureTextEx(font, vstr, valueFs, spacingS);
    float vX = x + (w - vSz.x) * 0.5f;
    float vY = barBottom + (6.0f * scale);
    DrawTextEx(font, vstr, Vector2{ vX, vY }, valueFs, spacingS, textColor);

    if (hasUnits)
    {
        Vector2 uSz = MeasureTextEx(font, units.c_str(), unitsFs, spacingS);
        float uX = x + (w - uSz.x) * 0.5f;
        float uY = vY + vSz.y - (2.0f * scale);
        DrawTextEx(font, units.c_str(), Vector2{ uX, uY }, unitsFs, spacingS, Color{ textColor.r, textColor.g, textColor.b, 190 });
    }
}

void HorizontalBarGraphWidget::Draw(const Font& font) const
{
    const float base = BASE_TILE * scale;
    const float x = gx * base;
    const float y = gy * base;
    const float w = wTiles * base;
    const float h = hTiles * base;

    const float padS    = pad * scale;
    const float borderS = border * scale;

    DrawRectangle(Px(x), Px(y), Px(w), Px(h), panelFill);
    DrawRectangleLinesEx(Rectangle{ x, y, w, h }, borderS, panelBorder);

    float denom = (maxValue - minValue);
    float p = (denom != 0.0f) ? ((value - minValue) / denom) : 0.0f;
    p = ClampF(p, 0.0f, 1.0f);

    float left   = x + padS;
    float right  = x + w - padS;
    float top    = y + padS;
    float bottom = y + h - padS;

    float valueFs = valueTextSize * scale;
    float unitsFs = unitsTextSize * scale;
    float spacingS = 1.0f * scale;
    bool hasUnits = (units && units[0] != '\0');

    float textBlockH = valueFs + (hasUnits ? unitsFs : 0.0f) + (10.0f * scale);
    float barAreaTop = top;
    float barAreaBottom = bottom - textBlockH;
    if (barAreaBottom < barAreaTop + 10.0f * scale) barAreaBottom = barAreaTop + 10.0f * scale;

    float bh = barHeight * scale;
    if (bh < 6.0f * scale) bh = 6.0f * scale;

    float barY = barAreaTop + ((barAreaBottom - barAreaTop) * 0.35f) - (bh * 0.5f);
    if (barY < barAreaTop) barY = barAreaTop;
    if (barY + bh > barAreaBottom) barY = barAreaBottom - bh;

    Rectangle barRect = Rectangle{ left, barY, right - left, bh };

    DrawRectangleRounded(barRect, 0.25f, 8, barBackColor);

    float fillW = barRect.width * p;
    Rectangle fillRect = Rectangle{ barRect.x, barRect.y, fillW, barRect.height };

    Color fillColor = ColorForValue(value, thresholds, thresholdCount);
    DrawRectangleRounded(fillRect, 0.25f, 8, fillColor);

    int nTicks = tickCount;
    if (nTicks < 2) nTicks = 2;

    float tickLen = 8.0f * scale;
    float tickY0 = barRect.y + barRect.height + (8.0f * scale);
    float tickY1 = tickY0 + tickLen;

    float tickFs = tickLabelSize * scale;
    float tickSpacing = 1.0f * scale;

    for (int i = 0; i < nTicks; i++)
    {
        float t = (float)i / (float)(nTicks - 1);
        float v = minValue + (maxValue - minValue) * t;

        float xx = barRect.x + t * barRect.width;

        DrawLineEx(Vector2{ xx, tickY0 }, Vector2{ xx, tickY1 }, 2.0f * scale, tickColor);

        if (showTickLabels)
        {
            const char* s = TextFormat("%.0f", v);
            Vector2 sz = MeasureTextEx(font, s, tickFs, tickSpacing);
            DrawTextEx(font,
                       s,
                       Vector2{ xx - sz.x * 0.5f, tickY1 + (4.0f * scale) },
                       tickFs,
                       tickSpacing,
                       tickColor);
        }
    }

    const char* fmt = (decimals <= 0) ? "%.0f" :
                      (decimals == 1) ? "%.1f" :
                      (decimals == 2) ? "%.2f" : "%.3f";
    const char* vstr = TextFormat(fmt, value);

    Vector2 vSz = MeasureTextEx(font, vstr, valueFs, spacingS);
    float vX = x + (w - vSz.x) * 0.5f;
    float vY = bottom - textBlockH + (6.0f * scale);
    DrawTextEx(font, vstr, Vector2{ vX, vY }, valueFs, spacingS, textColor);

    if (hasUnits)
    {
        Vector2 uSz = MeasureTextEx(font, units, unitsFs, spacingS);
        float uX = x + (w - uSz.x) * 0.5f;
        float uY = vY + vSz.y - (2.0f * scale);
        DrawTextEx(font,
                   units,
                   Vector2{ uX, uY },
                   unitsFs,
                   spacingS,
                   Color{ textColor.r, textColor.g, textColor.b, 190 });
    }
}

void GraphWidget::Draw(const Font& font) const
{
    const float base = BASE_TILE * scale;
    const float x = gx * base;
    const float y = gy * base;
    const float w = wTiles * base;
    const float h = hTiles * base;

    const float padS = pad * scale;
    const float borderS = border * scale;
    const float tickFs = tickLabelSize * scale;
    const float axisFs = axisLabelSize * scale;
    const float spacingS = 1.0f * scale;

    // Panel
    DrawRectangle(Px(x), Px(y), Px(w), Px(h), panelFill);
    DrawRectangleLinesEx(Rectangle{ x, y, w, h }, borderS, panelBorder);

    // Plot area inside panel
    const float plotX = x + (leftMargin * scale);
    const float plotY = y + (topMargin * scale);
    const float plotW = w - ((leftMargin + rightMargin) * scale);
    const float plotH = h - ((topMargin + bottomMargin) * scale);

    if (plotW <= 1.0f || plotH <= 1.0f) return;

    DrawRectangle(Px(plotX), Px(plotY), Px(plotW), Px(plotH), plotBackColor);

    // Helpers
    const float xRange = (xMax - xMin);
    const float yRange = (yMax - yMin);
    if (xRange == 0.0f || yRange == 0.0f) return;

    auto ToScreen = [&](float gxv, float gyv) -> Vector2
    {
        float tx = (gxv - xMin) / xRange;
        float ty = (gyv - yMin) / yRange;

        float sx = plotX + tx * plotW;
        float sy = plotY + plotH - ty * plotH;   // invert y for screen space
        return Vector2{ sx, sy };
    };

    // Grid + ticks + labels
    int xTicks = (xTickCount < 2) ? 2 : xTickCount;
    int yTicks = (yTickCount < 2) ? 2 : yTickCount;

    if (showGrid)
    {
        for (int i = 0; i < xTicks; i++)
        {
            float t = (float)i / (float)(xTicks - 1);
            float xx = plotX + t * plotW;
            DrawLineEx(Vector2{ xx, plotY }, Vector2{ xx, plotY + plotH }, 1.0f * scale, gridColor);
        }

        for (int i = 0; i < yTicks; i++)
        {
            float t = (float)i / (float)(yTicks - 1);
            float yy = plotY + plotH - t * plotH;
            DrawLineEx(Vector2{ plotX, yy }, Vector2{ plotX + plotW, yy }, 1.0f * scale, gridColor);
        }
    }

    // Axes
    DrawLineEx(Vector2{ plotX, plotY + plotH }, Vector2{ plotX + plotW, plotY + plotH }, 2.0f * scale, axisColor);
    DrawLineEx(Vector2{ plotX, plotY }, Vector2{ plotX, plotY + plotH }, 2.0f * scale, axisColor);

    // X ticks + labels
    for (int i = 0; i < xTicks; i++)
    {
        float t = (float)i / (float)(xTicks - 1);
        float v = xMin + (xMax - xMin) * t;
        float xx = plotX + t * plotW;

        DrawLineEx(Vector2{ xx, plotY + plotH }, Vector2{ xx, plotY + plotH + (6.0f * scale) }, 2.0f * scale, axisColor);

        if (showXTickLabels)
        {
            const char* s = TextFormat("%.0f", v);
            Vector2 sz = MeasureTextEx(font, s, tickFs, spacingS);
            DrawTextEx(font,
                       s,
                       Vector2{ xx - sz.x * 0.5f, plotY + plotH + (8.0f * scale) },
                       tickFs,
                       spacingS,
                       textColor);
        }
    }

    // Y ticks + labels
    for (int i = 0; i < yTicks; i++)
    {
        float t = (float)i / (float)(yTicks - 1);
        float v = yMin + (yMax - yMin) * t;
        float yy = plotY + plotH - t * plotH;

        DrawLineEx(Vector2{ plotX - (6.0f * scale), yy }, Vector2{ plotX, yy }, 2.0f * scale, axisColor);

        if (showYTickLabels)
        {
            const char* s = TextFormat("%.0f", v);
            Vector2 sz = MeasureTextEx(font, s, tickFs, spacingS);
            DrawTextEx(font,
                       s,
                       Vector2{ plotX - sz.x - (10.0f * scale), yy - sz.y * 0.5f },
                       tickFs,
                       spacingS,
                       textColor);
        }
    }

    // Axis unit labels
    if (showXUnits && xUnits && xUnits[0] != '\0')
    {
        Vector2 sz = MeasureTextEx(font, xUnits, axisFs, spacingS);
        DrawTextEx(font,
                   xUnits,
                   Vector2{ plotX + (plotW - sz.x) * 0.5f, y + h - sz.y - (6.0f * scale) },
                   axisFs,
                   spacingS,
                   textColor);
    }

    if (showYUnits && yUnits && yUnits[0] != '\0')
    {
        Vector2 sz = MeasureTextEx(font, yUnits, axisFs, spacingS);

        DrawTextPro(font,
                    yUnits,
                    Vector2{ x + sz.y + (4.0f * scale), plotY + plotH * 0.5f + sz.x * 0.5f },
                    Vector2{ 0.0f, 0.0f },
                    -90.0f,
                    axisFs,
                    spacingS,
                    textColor);
    }

    // Draw all series
    for (const auto& s : series)
    {
        if (!s.visible || s.points.size() < 2) continue;

        for (size_t i = 1; i < s.points.size(); i++)
        {
            Vector2 a = ToScreen(s.points[i - 1].x, s.points[i - 1].y);
            Vector2 b = ToScreen(s.points[i].x, s.points[i].y);
            DrawLineEx(a, b, s.thickness * scale, s.color);
        }

        // Optional markers on this series
        for (const auto& m : s.markers)
        {
            Vector2 p = ToScreen(m.x, m.y);
            float r = m.radius * scale;

            if (m.filled) DrawCircleV(p, r, m.color);
            else DrawCircleLines((int)p.x, (int)p.y, r, m.color);
        }
    }
}