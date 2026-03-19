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
    std::variant<NumberWidget, IndicatorLight, GaugeWidget, BarGraphWidget> widget;
    uint32_t can_id;
    std::string signal;

    void set_value(double v);
    void draw(const Font& font) const;
};

struct LiveScreen {
    std::string name;
    std::vector<LiveWidget> widgets;
};

std::vector<LiveScreen> build_screens(const DisplayConfig& config);