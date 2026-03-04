#include "WidgetFactory.h"
#include <type_traits>

static const char* unit_to_string(DataUnit unit) {
    switch (unit) {
        case DataUnit::Temperature: return "\xc2\xb0""C";  // UTF-8 °C
        case DataUnit::Pressure:    return "psi";
        case DataUnit::RPM:         return "RPM";
        case DataUnit::Voltage:     return "V";
        case DataUnit::Current:     return "A";
        case DataUnit::Speed:       return "mph";
        case DataUnit::Torque:      return "Nm";
        case DataUnit::Power:       return "kW";
        case DataUnit::Percent:     return "%";
        default:                    return "";
    }
}

static void fill_thresholds(GaugeThreshold* th, int& count, const DataConfig& d) {
    count = 3;
    th[0] = { (float)d.caution_threshold,  GREEN  };
    th[1] = { (float)d.critical_threshold, YELLOW };
    th[2] = { (float)d.max,                RED    };
}

std::vector<LiveWidget> build_widgets(const DisplayConfig& config) {
    std::vector<LiveWidget> result;

    for (const auto& screen : config.screens) {
        for (const auto& wc : screen.widgets) {
            LiveWidget lw;
            lw.can_id = wc.data.can_id;
            lw.signal = wc.data.signal;

            int gx     = wc.position.x;
            int gy     = wc.position.y;
            int wTiles = wc.position.width;
            int hTiles = wc.position.height;

            switch (wc.type) {
                case WidgetType::Gauge: {
                    GaugeWidget g;
                    g.gx = gx; g.gy = gy;
                    g.wTiles = wTiles; g.hTiles = hTiles;
                    g.minValue = (float)wc.data.min;
                    g.maxValue = (float)wc.data.max;
                    g.units = unit_to_string(wc.data.unit);
                    fill_thresholds(g.thresholds, g.thresholdCount, wc.data);
                    lw.widget = g;
                    break;
                }
                case WidgetType::Bar: {
                    BarGraphWidget b;
                    b.gx = gx; b.gy = gy;
                    b.wTiles = wTiles; b.hTiles = hTiles;
                    b.minValue = (float)wc.data.min;
                    b.maxValue = (float)wc.data.max;
                    b.units = unit_to_string(wc.data.unit);
                    fill_thresholds(b.thresholds, b.thresholdCount, wc.data);
                    lw.widget = b;
                    break;
                }
                case WidgetType::Number: {
                    NumberWidget n;
                    n.gx = gx; n.gy = gy;
                    n.wTiles = wTiles; n.hTiles = hTiles;
                    n.label = wc.data.can_id_label;
                    lw.widget = n;
                    break;
                }
                case WidgetType::Indicator: {
                    IndicatorLight ind;
                    ind.gx = gx; ind.gy = gy;
                    ind.wTiles = wTiles; ind.hTiles = hTiles;
                    ind.label = wc.data.can_id_label;
                    lw.widget = ind;
                    break;
                }
            }

            result.push_back(std::move(lw));
        }
    }

    return result;
}

void LiveWidget::set_value(double v) {
    std::visit([v](auto& w) {
        using T = std::decay_t<decltype(w)>;
        if constexpr (std::is_same_v<T, NumberWidget>) {
            w.value = static_cast<int>(v);
        } else if constexpr (std::is_same_v<T, IndicatorLight>) {
            w.on = (v != 0.0);
        } else {
            w.value = static_cast<float>(v);
        }
    }, widget);
}

void LiveWidget::draw(const Font& font) const {
    std::visit([&font](const auto& w) { w.Draw(font); }, widget);
}
