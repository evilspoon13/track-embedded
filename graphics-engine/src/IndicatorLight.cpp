/**
 * IndicatorLight.cpp   Indicator Light Widget
 *
 * @author      Justin Busker '26
 *
 * @copyright   Texas A&M University
 */

#include "Widgets.h"
#include <cmath>

void IndicatorLight::Draw(const Font &font) const {
  auto lo = compute_layout(gx, gy, wTiles, hTiles, pad, border, scale);
  draw_panel(lo, panelFill, panelBorder);

  const float labelSizeS = labelSize * scale;
  const float spacingS = 1.0f * scale;
  const float gapS = gapBelowLabel * scale;

  Vector2 labelSz = MeasureTextEx(font, label.c_str(), labelSizeS, spacingS);
  float labelX = lo.x + (lo.w - labelSz.x) * 0.5f;
  float labelY = lo.y + lo.padS;
  DrawTextEx(font, label.c_str(), Vector2{labelX, labelY}, labelSizeS, spacingS,
             RAYWHITE);

  float contentTop = labelY + labelSz.y + gapS;
  float contentBottom = lo.y + lo.h - lo.padS;
  float contentH = contentBottom - contentTop;
  if (contentH < 1.0f)
    contentH = 1.0f;

  Vector2 c = {lo.x + lo.w * 0.5f, contentTop + contentH * 0.5f};

  float maxR_byWidth = (fminf(lo.w, lo.h) * 0.5f) - lo.padS;
  float maxR_byHeight = (contentH * 0.5f) - lo.padS;
  float radius = (maxR_byWidth < maxR_byHeight) ? maxR_byWidth : maxR_byHeight;
  if (radius < 1.0f)
    radius = 1.0f;

  Color fill = on ? onColor : offColor;
  if (!on)
    fill.a = 120;

  DrawCircleV(c, radius, fill);
  DrawCircleLines((int)c.x, (int)c.y, radius, Color{255, 255, 255, 80});
}
