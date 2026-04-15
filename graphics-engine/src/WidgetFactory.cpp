/**
 * WidgetFactory.cpp    Widget Factory
 *
 * @author      Justin Busker '26
 * @author      Jack Williams '26
 *
 * @copyright   Texas A&M University
 */

#include "WidgetFactory.h"
#include "raylib.h"
#include <type_traits>
#include <utility>

static void fill_thresholds(GaugeThreshold *th, int &count,
                            const DataConfig &d) {
  count = 3;
  th[0] = {(float)d.caution_threshold, GREEN};
  th[1] = {(float)d.critical_threshold, YELLOW};
  th[2] = {(float)d.max, RED};
}

std::vector<LiveScreen> build_screens(const DisplayConfig &config) {
  std::vector<LiveScreen> result;
  result.reserve(config.screens.size());

  for (const auto &screen_cfg : config.screens) {
    LiveScreen screen;
    screen.name = screen_cfg.name;
    screen.widgets.reserve(screen_cfg.widgets.size());

    for (const auto &wc : screen_cfg.widgets) {
      LiveWidget lw;
      lw.can_id = wc.data.can_id;
      lw.signal = wc.data.signal;

      int gx = wc.position.x;
      int gy = wc.position.y;
      int wTiles = wc.position.width;
      int hTiles = wc.position.height;

      switch (wc.type) {
      case WidgetType::Gauge: {
        GaugeWidget g;
        g.gx = gx;
        g.gy = gy;
        g.wTiles = wTiles;
        g.hTiles = hTiles;
        g.minValue = (float)wc.data.min;
        g.maxValue = (float)wc.data.max;
        g.units = wc.data.unit;
        fill_thresholds(g.thresholds, g.thresholdCount, wc.data);
        g.alarm = wc.alarm;
        g.criticalThreshold = (float)wc.data.critical_threshold;
        lw.widget = g;
        break;
      }
      case WidgetType::Bar: {
        BarGraphWidget b;
        b.gx = gx;
        b.gy = gy;
        b.wTiles = wTiles;
        b.hTiles = hTiles;
        b.minValue = (float)wc.data.min;
        b.maxValue = (float)wc.data.max;
        b.units = wc.data.unit;
        fill_thresholds(b.thresholds, b.thresholdCount, wc.data);
        b.alarm = wc.alarm;
        b.criticalThreshold = (float)wc.data.critical_threshold;
        lw.widget = b;
        break;
      }
      case WidgetType::Number: {
        NumberWidget n;
        n.gx = gx;
        n.gy = gy;
        n.wTiles = wTiles;
        n.hTiles = hTiles;
        n.label = wc.data.can_id_label;
        fill_thresholds(n.thresholds, n.thresholdCount, wc.data);
        n.alarm = wc.alarm;
        n.criticalThreshold = (float)wc.data.critical_threshold;
        lw.widget = n;
        break;
      }
      case WidgetType::Indicator: {
        IndicatorLight ind;
        ind.gx = gx;
        ind.gy = gy;
        ind.wTiles = wTiles;
        ind.hTiles = hTiles;
        ind.label = wc.data.can_id_label;
        ind.alarm = wc.alarm;
        lw.widget = ind;
        break;
      }
      case WidgetType::Graph: {
        GraphWidget g;
        g.gx = gx;
        g.gy = gy;
        g.wTiles = wTiles;
        g.hTiles = hTiles;
        g.yMin = (float)wc.data.min;
        g.yMax = (float)wc.data.max;
        g.yUnits = wc.data.unit;

        GraphConfig gc = wc.graph.value_or(GraphConfig{});
        g.mode = gc.mode;
        g.max_points = gc.max_points > 0 ? gc.max_points : 1000;

        if (gc.mode == GraphMode::TimeSeries) {
          g.window_seconds = gc.window_seconds > 0.0f ? gc.window_seconds : 30.0f;
          g.xMin = 0.0f;
          g.xMax = g.window_seconds;
          g.xUnits = "s";
        } else {
          g.xMin = (float)gc.x_min;
          g.xMax = (float)gc.x_max;
          g.xUnits = gc.x_unit;
          lw.x_can_id = gc.x_can_id;
          lw.x_signal = gc.x_signal;
        }

        fill_thresholds(g.thresholds, g.thresholdCount, wc.data);
        g.alarm = wc.alarm;
        g.criticalThreshold = (float)wc.data.critical_threshold;

        // series[0] must exist before push_y/push_xy are called.
        g.series.emplace_back(GraphSeries{});

        lw.widget = g;
        break;
      }
      }

      screen.widgets.push_back(std::move(lw));
    }

    result.push_back(std::move(screen));
  }

  return result;
}

void LiveWidget::set_value(double v) {
  std::visit(
      [v](auto &w) {
        using T = std::decay_t<decltype(w)>;
        if constexpr (std::is_same_v<T, NumberWidget>) {
          w.value = static_cast<int>(v);
        } else if constexpr (std::is_same_v<T, IndicatorLight>) {
          w.on = (v != 0.0);
        } else if constexpr (std::is_same_v<T, GraphWidget>) {
          if (w.mode == GraphMode::TimeSeries) {
            w.push_y(GetTime(), static_cast<float>(v));
          } else {
            w.pending_y = static_cast<float>(v);
            w.has_pending_y = true;
            if (w.has_pending_x)
              w.push_xy(w.pending_x, w.pending_y);
          }
        } else {
          w.value = static_cast<float>(v);
        }
      },
      widget);
}

void LiveWidget::set_x_value(double v) {
  std::visit(
      [v](auto &w) {
        if constexpr (std::is_same_v<std::decay_t<decltype(w)>, GraphWidget>) {
          w.pending_x = static_cast<float>(v);
          w.has_pending_x = true;
          if (w.has_pending_y)
            w.push_xy(w.pending_x, w.pending_y);
        }
      },
      widget);
}

void LiveWidget::draw(const Font &font) const {
  std::visit([&font](const auto &w) { w.Draw(font); }, widget);
}
