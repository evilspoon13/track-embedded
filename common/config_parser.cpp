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

  for (const auto &screen : j["screens"]) {
    ScreenConfig scr;
    scr.name = screen["name"];

    for (const auto &w : screen["widgets"]) {
      WidgetConfig cfg;
      cfg.type = parse_widget_type(w["type"].get<std::string>());
      cfg.alarm = w["alarm"];

      const auto &pos = w["position"];
      cfg.position = {pos["x"], pos["y"], pos["width"], pos["height"]};

      const auto &d = w["data"];
      cfg.data.can_id = parse_can_id(d["can_id"].get<std::string>());
      cfg.data.can_id_label = d["can_id_label"];
      cfg.data.signal = d["signal"];
      cfg.data.unit = d["unit"].get<std::string>();
      cfg.data.min = d["min"];
      cfg.data.max = d["max"];
      cfg.data.caution_threshold = d["caution_threshold"];
      cfg.data.critical_threshold = d["critical_threshold"];

      if (cfg.type == WidgetType::Graph) {
        const auto &g = w["graph"];
        GraphConfig gc;
        gc.mode = parse_graph_mode(g["mode"].get<std::string>());

        gc.max_points = g.value("max_points", 1000u);
        if (gc.max_points < 1)
          gc.max_points = 1;

        if (gc.mode == GraphMode::TimeSeries) {
          gc.window_seconds = g.value("window_seconds", 30.0f);
          if (gc.window_seconds <= 0.0f)
            gc.window_seconds = 1.0f;
        } else {
          gc.x_can_id = parse_can_id(g["x_can_id"].get<std::string>());
          gc.x_signal = g["x_signal"];
          gc.x_unit = g["x_unit"].get<std::string>();
          gc.x_min = g["x_min"];
          gc.x_max = g["x_max"];
        }
        cfg.graph = gc;
      }

      scr.widgets.emplace_back(cfg);
    }

    result.screens.emplace_back(scr);
  }

  return result;
}
