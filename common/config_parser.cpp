/**
 * config_parser.cpp    Display Config Parser
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 * @author      Justin Busker '26
 *
 * @copyright   Texas A&M University
 */

#include "config_parser.hpp"
#include "config_types.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

static uint32_t parse_can_id(const std::string &s) {
  return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
}

static WidgetType parse_widget_type(std::string_view s) {
  if (s == "gauge")
    return WidgetType::Gauge;
  if (s == "bar")
    return WidgetType::Bar;
  if (s == "number")
    return WidgetType::Number;
  if (s == "indicator")
    return WidgetType::Indicator;
  if (s == "graph")
    return WidgetType::Graph;
  throw std::invalid_argument("unknown widget type: " + std::string(s));
}

static DataUnit parse_data_unit(std::string_view s) {
  if (s == "temperature")
    return DataUnit::Temperature;
  if (s == "pressure")
    return DataUnit::Pressure;
  if (s == "rpm")
    return DataUnit::RPM;
  if (s == "voltage")
    return DataUnit::Voltage;
  if (s == "current")
    return DataUnit::Current;
  if (s == "speed")
    return DataUnit::Speed;
  if (s == "torque")
    return DataUnit::Torque;
  if (s == "power")
    return DataUnit::Power;
  if (s == "percent")
    return DataUnit::Percent;
  throw std::invalid_argument("unknown data unit: " + std::string(s));
}

static GraphMode parse_graph_mode(std::string_view s) {
  if (s == "time_series")
    return GraphMode::TimeSeries;
  if (s == "xy")
    return GraphMode::XY;
  throw std::invalid_argument("unknown graph mode: " + std::string(s));
}

static nlohmann::json parse_json_file(const std::string &path) {
  std::ifstream f(path);
  if (!f.is_open())
    return nullptr;
  return nlohmann::json::parse(f);
}

DisplayConfig load_display_config(const std::string &path) {
  DisplayConfig result;
  auto j = parse_json_file(path);
  if (j.is_null())
    return result;

  if (!j.contains("screens") || !j["screens"].is_array())
    return result;

  for (const auto &screen : j["screens"]) {
    ScreenConfig scr;
    scr.name = screen.value("name", std::string(""));

    if (!screen.contains("widgets") || !screen["widgets"].is_array()) {
      result.screens.emplace_back(scr);
      continue;
    }

    for (const auto &w : screen["widgets"]) {
      WidgetConfig cfg;
      cfg.type = parse_widget_type(w.value("type", std::string("number")));
      cfg.alarm = w.value("alarm", false);

      if (w.contains("position")) {
        const auto &pos = w["position"];
        cfg.position = {
          pos.value("x", 0), pos.value("y", 0),
          pos.value("width", 1), pos.value("height", 1)
        };
      }

      if (w.contains("data")) {
        const auto &d = w["data"];
        cfg.data.can_id = parse_can_id(d.value("can_id", std::string("0x0")));
        cfg.data.can_id_label = d.value("can_id_label", std::string(""));
        cfg.data.signal = d.value("signal", std::string(""));
        cfg.data.unit = d.value("unit", std::string(""));
        cfg.data.min = d.value("min", 0.0);
        cfg.data.max = d.value("max", 100.0);
        cfg.data.caution_threshold = d.value("caution_threshold", 0.0);
        cfg.data.critical_threshold = d.value("critical_threshold", 0.0);
      }

      if (cfg.type == WidgetType::Graph && w.contains("graph")) {
        const auto &g = w["graph"];
        GraphConfig gc;
        gc.mode = parse_graph_mode(g.value("mode", std::string("time_series")));

        gc.max_points = g.value("max_points", 1000u);
        if (gc.max_points < 1)
          gc.max_points = 1;

        if (gc.mode == GraphMode::TimeSeries) {
          gc.window_seconds = g.value("window_seconds", 30.0f);
          if (gc.window_seconds <= 0.0f)
            gc.window_seconds = 1.0f;
        } else {
          gc.x_can_id = parse_can_id(g.value("x_can_id", std::string("0x0")));
          gc.x_signal = g.value("x_signal", std::string(""));
          gc.x_unit = g.value("x_unit", std::string(""));
          gc.x_min = g.value("x_min", 0.0);
          gc.x_max = g.value("x_max", 100.0);
        }
        cfg.graph = gc;
      }

      scr.widgets.emplace_back(cfg);
    }

    result.screens.emplace_back(scr);
  }

  return result;
}
