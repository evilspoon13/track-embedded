/**
 * GraphWidget.cpp      Graph Widget
 *
 * @author      Justin Busker '26
 *
 * @copyright   Texas A&M University
 */

#include "Widgets.h"

void GraphWidget::Draw(const Font &font) const {
  auto lo = compute_layout(gx, gy, wTiles, hTiles, pad, border, scale);
  draw_panel(lo, panelFill, panelBorder);

  const float tickFs = tickLabelSize * scale;
  const float axisFs = axisLabelSize * scale;
  const float spacingS = 1.0f * scale;

  const float plotX = lo.x + leftMargin * scale;
  const float plotY = lo.y + topMargin * scale;
  const float plotW = lo.w - (leftMargin + rightMargin) * scale;
  const float plotH = lo.h - (topMargin + bottomMargin) * scale;

  if (plotW <= 1.0f || plotH <= 1.0f)
    return;

  DrawRectangle(Px(plotX), Px(plotY), Px(plotW), Px(plotH), plotBackColor);

  const float xRange = xMax - xMin;
  const float yRange = yMax - yMin;
  if (xRange == 0.0f || yRange == 0.0f)
    return;

  auto ToScreen = [&](float gxv, float gyv) -> Vector2 {
    float tx = (gxv - xMin) / xRange;
    float ty = (gyv - yMin) / yRange;
    return {plotX + tx * plotW, plotY + plotH - ty * plotH};
  };

  int xTicks = (xTickCount < 2) ? 2 : xTickCount;
  int yTicks = (yTickCount < 2) ? 2 : yTickCount;

  if (showGrid) {
    for (int i = 0; i < xTicks; i++) {
      float xx = plotX + ((float)i / (xTicks - 1)) * plotW;
      DrawLineEx({xx, plotY}, {xx, plotY + plotH}, 1.0f * scale, gridColor);
    }
    for (int i = 0; i < yTicks; i++) {
      float yy = plotY + plotH - ((float)i / (yTicks - 1)) * plotH;
      DrawLineEx({plotX, yy}, {plotX + plotW, yy}, 1.0f * scale, gridColor);
    }
  }

  DrawLineEx({plotX, plotY + plotH}, {plotX + plotW, plotY + plotH},
             2.0f * scale, axisColor);
  DrawLineEx({plotX, plotY}, {plotX, plotY + plotH}, 2.0f * scale, axisColor);

  for (int i = 0; i < xTicks; i++) {
    float t = (float)i / (xTicks - 1);
    float v = xMin + (xMax - xMin) * t;
    float xx = plotX + t * plotW;
    DrawLineEx({xx, plotY + plotH}, {xx, plotY + plotH + 6.0f * scale},
               2.0f * scale, axisColor);
    if (showXTickLabels) {
      const char *s = TextFormat("%.0f", v);
      Vector2 sz = MeasureTextEx(font, s, tickFs, spacingS);
      DrawTextEx(font, s, {xx - sz.x * 0.5f, plotY + plotH + 8.0f * scale},
                 tickFs, spacingS, textColor);
    }
  }

  for (int i = 0; i < yTicks; i++) {
    float t = (float)i / (yTicks - 1);
    float v = yMin + (yMax - yMin) * t;
    float yy = plotY + plotH - t * plotH;
    DrawLineEx({plotX - 6.0f * scale, yy}, {plotX, yy}, 2.0f * scale,
               axisColor);
    if (showYTickLabels) {
      const char *s = TextFormat("%.0f", v);
      Vector2 sz = MeasureTextEx(font, s, tickFs, spacingS);
      DrawTextEx(font, s, {plotX - sz.x - 10.0f * scale, yy - sz.y * 0.5f},
                 tickFs, spacingS, textColor);
    }
  }

  if (showXUnits && !xUnits.empty()) {
    Vector2 sz = MeasureTextEx(font, xUnits.c_str(), axisFs, spacingS);
    DrawTextEx(
        font, xUnits.c_str(),
        {plotX + (plotW - sz.x) * 0.5f, lo.y + lo.h - sz.y - 6.0f * scale},
        axisFs, spacingS, textColor);
  }

  if (showYUnits && !yUnits.empty()) {
    Vector2 sz = MeasureTextEx(font, yUnits.c_str(), axisFs, spacingS);
    DrawTextPro(
        font, yUnits.c_str(),
        {lo.x + sz.y + 4.0f * scale, plotY + plotH * 0.5f + sz.x * 0.5f},
        {0.0f, 0.0f}, -90.0f, axisFs, spacingS, textColor);
  }

  for (const auto &s : series) {
    if (!s.visible || s.points.size() < 2)
      continue;
    for (size_t i = 1; i < s.points.size(); i++) {
      DrawLineEx(ToScreen(s.points[i - 1].x, s.points[i - 1].y),
                 ToScreen(s.points[i].x, s.points[i].y), s.thickness * scale,
                 s.color);
    }
    for (const auto &m : s.markers) {
      Vector2 p = ToScreen(m.x, m.y);
      float r = m.radius * scale;
      if (m.filled)
        DrawCircleV(p, r, m.color);
      else
        DrawCircleLines((int)p.x, (int)p.y, r, m.color);
    }
  }

  draw_alarm_overlay(lo, alarm_active(alarm, latestY, criticalThreshold));
}

void GraphWidget::push_y(float t, float v) {
    latestY = v;
    if (series.empty()) return;
    auto& pts = series[0].points;
    pts.push_back({t, v});

    // Drop points older than the scrolling window — this is what keeps the
    // data within [xMin, xMax] so lines never render outside the axis bounds.
    float cutoff = t - window_seconds;
    while (!pts.empty() && pts.front().x < cutoff)
        pts.erase(pts.begin());

    // Safety cap: prevent unbounded growth at very high CAN data rates.
    while (pts.size() > max_points)
        pts.erase(pts.begin());

    xMin = t - window_seconds;
    xMax = t;
}

void GraphWidget::push_xy(float x, float y) {
    latestY = y;
    if (series.empty()) return;
    auto& pts = series[0].points;
    pts.push_back({x, y});
    if (pts.size() > max_points)
        pts.erase(pts.begin());
}
