/**
 * graph_config_test.cpp    Config parser test for graph widget
 *
 * @copyright   Texas A&M University
 */

#include "config_parser.hpp"
#include "config_types.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>

static void write_file(const char* path, const char* content) {
    std::ofstream f(path);
    f << content;
}

static void test_time_series() {
    write_file("/tmp/graph_ts_test.json", R"({
        "screens": [{
            "name": "test",
            "widgets": [{
                "type": "graph",
                "alarm": false,
                "position": {"x": 0, "y": 0, "width": 5, "height": 3},
                "data": {
                    "can_id": "0x100",
                    "can_id_label": "",
                    "signal": "motor_temp",
                    "unit": "temperature",
                    "min": 0,
                    "max": 150,
                    "caution_threshold": 0,
                    "critical_threshold": 0
                },
                "graph": {
                    "mode": "time_series",
                    "window_seconds": 30,
                    "max_points": 500
                }
            }]
        }]
    })");

    DisplayConfig cfg = load_display_config("/tmp/graph_ts_test.json");
    assert(cfg.screens.size() == 1);
    assert(cfg.screens[0].widgets.size() == 1);

    const auto& w = cfg.screens[0].widgets[0];
    assert(w.type == WidgetType::Graph);
    assert(w.graph.has_value());
    assert(w.graph->mode == GraphMode::TimeSeries);
    assert(w.graph->window_seconds == 30.0f);
    assert(w.graph->max_points == 500u);
    assert(w.data.can_id == 0x100u);
    assert(w.data.signal == "motor_temp");
    assert(w.data.min == 0.0);
    assert(w.data.max == 150.0);

    printf("PASS: test_time_series\n");
}

static void test_xy() {
    write_file("/tmp/graph_xy_test.json", R"({
        "screens": [{
            "name": "test",
            "widgets": [{
                "type": "graph",
                "alarm": false,
                "position": {"x": 0, "y": 0, "width": 5, "height": 3},
                "data": {
                    "can_id": "0x200",
                    "can_id_label": "",
                    "signal": "motor_rpm",
                    "unit": "rpm",
                    "min": 0,
                    "max": 8000,
                    "caution_threshold": 0,
                    "critical_threshold": 0
                },
                "graph": {
                    "mode": "xy",
                    "max_points": 300,
                    "x_can_id": "0x100",
                    "x_signal": "throttle_pct",
                    "x_unit": "percent",
                    "x_min": 0,
                    "x_max": 100
                }
            }]
        }]
    })");

    DisplayConfig cfg = load_display_config("/tmp/graph_xy_test.json");
    const auto& w = cfg.screens[0].widgets[0];
    assert(w.type == WidgetType::Graph);
    assert(w.graph.has_value());
    assert(w.graph->mode == GraphMode::XY);
    assert(w.graph->max_points == 300u);
    assert(w.graph->x_can_id == 0x100u);
    assert(w.graph->x_signal == "throttle_pct");
    assert(w.graph->x_unit == DataUnit::Percent);
    assert(w.graph->x_min == 0.0);
    assert(w.graph->x_max == 100.0);

    printf("PASS: test_xy\n");
}

static void test_non_graph_widgets_unaffected() {
    write_file("/tmp/graph_nongraph_test.json", R"({
        "screens": [{
            "name": "test",
            "widgets": [{
                "type": "gauge",
                "alarm": false,
                "position": {"x": 0, "y": 0, "width": 2, "height": 2},
                "data": {
                    "can_id": "0x300",
                    "can_id_label": "",
                    "signal": "voltage",
                    "unit": "voltage",
                    "min": 0,
                    "max": 100,
                    "caution_threshold": 0,
                    "critical_threshold": 0
                }
            }]
        }]
    })");

    DisplayConfig cfg = load_display_config("/tmp/graph_nongraph_test.json");
    const auto& w = cfg.screens[0].widgets[0];
    assert(w.type == WidgetType::Gauge);
    assert(!w.graph.has_value());

    printf("PASS: test_non_graph_widgets_unaffected\n");
}

int main() {
    test_time_series();
    test_xy();
    test_non_graph_widgets_unaffected();
    printf("All tests passed.\n");
    return 0;
}
