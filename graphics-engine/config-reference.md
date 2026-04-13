TRACK Display — JSON Config Reference
=====================================

OVERVIEW
--------
The config file describes one or more screens. Each screen contains a list of
widgets to render. The graphics engine loads widgets from the first screen and
draws them on an 800x480 window divided into a 10x6 tile grid (each tile is
80x80 pixels).

Run with a specific config file:
    ./graphics-engine path/to/config.json
Default (no argument):
    ./graphics-engine         # loads data.json from the working directory


TOP-LEVEL STRUCTURE
-------------------
{
    "screens": [
        { ... screen ... },
        { ... screen ... }
    ]
}


SCREEN
------
{
    "name": "screen_main",      -- identifier string (unused at runtime, for your reference)
    "widgets": [
        { ... widget ... },
        { ... widget ... }
    ]
}


WIDGET
------
Every widget has the same outer shape:

{
    "type":     <string>,       -- see Widget Types below
    "alarm":    <bool>,         -- when true, widget flashes a red border once value >= critical_threshold (gauge/bar only)
    "position": { ... },        -- see Position below
    "data":     { ... }         -- see Data below
}


POSITION
--------
Coordinates are in tile-grid units, NOT pixels.
The screen is 10 tiles wide and 6 tiles tall (each tile = 80px).

{
    "x":      <int>,    -- left edge, 0-9
    "y":      <int>,    -- top edge,  0-5
    "width":  <int>,    -- tile columns the widget occupies
    "height": <int>     -- tile rows the widget occupies
}

Minimum sizes per widget type:
    gauge     -- 2x2 tiles recommended, looks best at 3x3
    bar       -- 2x3 tiles (minimum enforced by layout)
    number    -- 1x1 tiles
    indicator -- 1x1 tiles
    graph     -- 5x3 tiles recommended

Widgets do not clip each other — make sure positions do not overlap.

Example placements on the 10x6 grid:
    Gauge  3x3 at (0, 0)  occupies columns 0-2, rows 0-2
    Bar    2x3 at (3, 0)  occupies columns 3-4, rows 0-2
    Number 2x1 at (0, 3)  occupies columns 0-1, rows 3-3


DATA
----
Links the widget to a CAN signal and sets its value range and thresholds.

{
    "can_id":             <string>,  -- CAN frame ID as a hex string, e.g. "0x7F" or "7F"
    "can_id_label":       <string>,  -- display label for number/indicator widgets
    "signal":             <string>,  -- signal name within the CAN frame
    "unit":               <string>,  -- see Units below
    "min":                <number>,  -- minimum expected value
    "max":                <number>,  -- maximum expected value
    "caution_threshold":  <number>,  -- widget turns yellow at or above this value
    "critical_threshold": <number>   -- widget turns red at or above this value
}

Thresholds apply to gauge and bar widgets.
    value < caution_threshold              → green
    caution_threshold <= value < critical  → yellow
    critical_threshold <= value            → red


UNITS
-----
The "unit" field must be one of these exact strings:

    "temperature"   -- displayed as °C
    "pressure"      -- displayed as psi
    "rpm"           -- displayed as RPM

Units are shown inside gauge and bar widgets. The number widget shows the raw
integer value with no units. The indicator widget shows no value, just on/off.


WIDGET TYPES
------------

"gauge"
    Circular ring gauge. Good for continuous values you want to read at a glance
    (speed, RPM, temperature). Animates a colored arc from min to current value.
    Tick marks and labels are drawn around the ring.
    Recommended size: 3x3 tiles or larger.

"bar"
    Vertical bar graph. Good for values where relative fullness matters
    (fuel level, coolant temp). Fills from bottom up. Tick labels on the right.
    Minimum size: 2x3 tiles.

"number"
    Displays a large integer value with a text label above it.
    Good for precise numeric readouts (lap count, gear position).
    The "can_id_label" field sets the label text.
    Minimum size: 1x1 tile, but 2x1 or 2x2 recommended for readability.

"indicator"
    A labeled circle that lights up (red) when the signal value is non-zero,
    and is dim gray when zero. Good for binary on/off states (oil pressure
    warning, fan active, etc.).
    The "can_id_label" field sets the label text.
    Minimum size: 1x1 tile.

"graph"
    Time-series or X-Y line graph.

    Time-series mode (mode: "time_series"):
        Plots one CAN signal over a rolling time window. The x-axis
        is wall-clock time (seconds since process start). The y-axis is
        the signal value. The "data" block configures the y-axis signal,
        range, and unit. The "graph" block sets window_seconds (how many
        seconds of history to show) and max_points (ring buffer cap).

    X-Y mode (mode: "xy"):
        Plots one CAN signal against another. The "data" block configures
        the y-axis signal. The "graph" block configures the x-axis signal
        (x_can_id, x_signal, x_unit, x_min, x_max) and max_points.
        x_unit is required in JSON for XY mode.

    The "data" block always carries the y-axis signal (consistent with
    all other widget types).

    Points accumulate using a last-known-value strategy: a new point is
    plotted whenever either axis receives a CAN update, using the most
    recent value of the other axis.

    Recommended size: 5x3 tiles or larger.
    Graph history is cleared on SIGHUP hot-reload.


FULL EXAMPLE
------------
{
    "screens": [
        {
            "name": "screen_main",
            "widgets": [
                {
                    "type": "gauge",
                    "alarm": false,
                    "position": { "x": 0, "y": 0, "width": 3, "height": 3 },
                    "data": {
                        "can_id": "0x64",
                        "can_id_label": "RPM",
                        "signal": "engine_rpm",
                        "unit": "rpm",
                        "min": 0,
                        "max": 8000,
                        "caution_threshold": 6000,
                        "critical_threshold": 7000
                    }
                },
                {
                    "type": "bar",
                    "alarm": false,
                    "position": { "x": 3, "y": 0, "width": 2, "height": 3 },
                    "data": {
                        "can_id": "0x65",
                        "can_id_label": "Coolant",
                        "signal": "coolant_temp",
                        "unit": "temperature",
                        "min": 0,
                        "max": 120,
                        "caution_threshold": 90,
                        "critical_threshold": 105
                    }
                },
                {
                    "type": "number",
                    "alarm": false,
                    "position": { "x": 5, "y": 0, "width": 2, "height": 2 },
                    "data": {
                        "can_id": "0x66",
                        "can_id_label": "GEAR",
                        "signal": "gear_pos",
                        "unit": "rpm",
                        "min": 0,
                        "max": 6,
                        "caution_threshold": 0,
                        "critical_threshold": 0
                    }
                },
                {
                    "type": "indicator",
                    "alarm": true,
                    "position": { "x": 7, "y": 0, "width": 1, "height": 1 },
                    "data": {
                        "can_id": "0x67",
                        "can_id_label": "OIL",
                        "signal": "oil_pressure_warn",
                        "unit": "pressure",
                        "min": 0,
                        "max": 1,
                        "caution_threshold": 0,
                        "critical_threshold": 1
                    }
                }
            ]
        }
    ]
}


GRAPH WIDGET EXAMPLES
---------------------

Time-series (30-second rolling window):

{
    "type": "graph",
    "alarm": false,
    "position": { "x": 0, "y": 3, "width": 5, "height": 3 },
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
        "max_points": 1000
    }
}

X-Y (throttle percentage vs motor RPM):

{
    "type": "graph",
    "alarm": false,
    "position": { "x": 5, "y": 3, "width": 5, "height": 3 },
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
        "max_points": 500,
        "x_can_id": "0x100",
        "x_signal": "throttle_pct",
        "x_unit": "percent",
        "x_min": 0,
        "x_max": 100
    }
}
