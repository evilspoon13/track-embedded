/**
 * WidgetFactory.h      Widget Factory
 *
 * @author      Justin Busker '26
 * @author      Jack Williams '26
 *
 * @copyright   Texas A&M University
 */

#pragma once
#include "Widgets.h"
#include "config_types.hpp"
#include <string>
#include <variant>
#include <vector>

struct LiveWidget {
    std::variant<NumberWidget, IndicatorLight, GaugeWidget, BarGraphWidget, GraphWidget> widget;
    uint32_t    can_id;
    std::string signal;

    // X-axis channel — only populated for XY-mode GraphWidgets.
    // Defaults to 0/"" so the x-channel check in main.cpp is a no-op for all other widgets.
    uint32_t    x_can_id = 0;
    std::string x_signal = "";

    void set_value(double v);      // y-axis / time-series update
    void set_x_value(double v);    // XY x-axis update (no-op for non-graph widgets)
    void draw(const Font& font) const;
};

struct LiveScreen {
    std::string name;
    std::vector<LiveWidget> widgets;
};

std::vector<LiveScreen> build_screens(const DisplayConfig& config);