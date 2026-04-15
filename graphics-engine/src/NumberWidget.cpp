/**
 * NumberWidget.cpp     Number Display Widget
 *
 * @author      Justin Busker '26
 *
 * @copyright   Texas A&M University
 */

#include "Widgets.h"

void NumberWidget::Draw(const Font &font) const {
  auto lo = compute_layout(gx, gy, wTiles, hTiles, pad, border, scale);
  draw_panel(lo, panelFill, panelBorder);

  const float labelSizeS = labelSize * scale;
  const float valueSizeS = valueSize * scale;
  const float spacingS = 1.0f * scale;
  const float gapS = gapBelowLabel * scale;

  Vector2 labelSz = MeasureTextEx(font, label.c_str(), labelSizeS, spacingS);
  float labelX = lo.x + (lo.w - labelSz.x) * 0.5f;
  float labelY = lo.y + lo.padS;
  DrawTextEx(font, label.c_str(), Vector2{labelX, labelY}, labelSizeS, spacingS,
             RAYWHITE);

  const char *vstr = TextFormat("%d", value);
  Vector2 valueSz = MeasureTextEx(font, vstr, valueSizeS, spacingS);

  float contentTop = labelY + labelSz.y + gapS;
  float contentBottom = lo.y + lo.h - lo.padS;
  float contentH = contentBottom - contentTop;
  if (contentH < 1.0f)
    contentH = 1.0f;

  float valueX = lo.x + (lo.w - valueSz.x) * 0.5f;
  float valueY = contentTop + (contentH - valueSz.y) * 0.5f;
  Color vcolor = thresholdCount > 0
                   ? ColorForValue((float)value, thresholds, thresholdCount)
                   : valueColor;
  DrawTextEx(font, vstr, Vector2{valueX, valueY}, valueSizeS, spacingS,
             vcolor);

  draw_alarm_overlay(lo, alarm_active(alarm, (float)value, criticalThreshold));
}
