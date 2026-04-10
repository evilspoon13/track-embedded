/**
 * gpio_input.hpp       GPIO Button Input (libgpiod v2)
 *
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef TRACK_GPIO_INPUT_HPP
#define TRACK_GPIO_INPUT_HPP

#include <gpiod.h>
#include <cstdio>
#include <cstdint>

inline constexpr unsigned int GPIO_PIN_SCREEN_UP = 12;
inline constexpr unsigned int GPIO_PIN_SCREEN_DOWN = 13;

inline constexpr int DEBOUNCE_FRAMES = 6; // ~100ms at 60fps

class GpioButtons {
public:
  GpioButtons() {
    chip_ = gpiod_chip_open("/dev/gpiochip0");
    if (!chip_) {
      perror("gpio: failed to open /dev/gpiochip0");
      return;
    }

    settings_ = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings_, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(settings_, GPIOD_LINE_BIAS_PULL_UP);
    gpiod_line_settings_set_active_low(settings_, true);

    config_ = gpiod_line_config_new();
    unsigned int offsets[] = { GPIO_PIN_SCREEN_UP, GPIO_PIN_SCREEN_DOWN };
    gpiod_line_config_add_line_settings(config_, offsets, 2, settings_);

    request_ = gpiod_chip_request_lines(chip_, nullptr, config_);
    if (!request_) {
      perror("gpio: failed to request lines");
    }
  }

  ~GpioButtons() {
    if (request_)
        gpiod_line_request_release(request_);
    if (config_)
        gpiod_line_config_free(config_);
    if (settings_)
        gpiod_line_settings_free(settings_);
    if (chip_)
        gpiod_chip_close(chip_);
  }

  GpioButtons(const GpioButtons&) = delete;
  GpioButtons& operator=(const GpioButtons&) = delete;

  /// call once per frame. reads pin state and detects edges.
  void update() {
    if (!request_) return;

    bool up_now = gpiod_line_request_get_value(request_, GPIO_PIN_SCREEN_UP) == GPIOD_LINE_VALUE_ACTIVE;
    bool down_now = gpiod_line_request_get_value(request_, GPIO_PIN_SCREEN_DOWN) == GPIOD_LINE_VALUE_ACTIVE;

    // rising edge detect with debounce
    up_pressed_ = false;
    down_pressed_ = false;

    if (up_cooldown_ > 0) {
      --up_cooldown_;
    }
    else if (up_now && !up_last_) {
      up_pressed_ = true;
      up_cooldown_ = DEBOUNCE_FRAMES;
    }

    if (down_cooldown_ > 0) {
      --down_cooldown_;
    }
    else if (down_now && !down_last_) {
      down_pressed_ = true;
      down_cooldown_ = DEBOUNCE_FRAMES;
    }

    up_last_ = up_now;
    down_last_ = down_now;
  }

  /// true for one frame after the screen-up button is pressed.
  bool pressed_up() const {
    return up_pressed_;
  }

  /// true for one frame after the screen-down button is pressed.
  bool pressed_down() const {
    return down_pressed_;
  }

  /// true if libgpiod init successfully.
  bool ok() const {
    return request_ != nullptr;
  }

private:
  gpiod_chip* chip_= nullptr;
  gpiod_line_settings* settings_ = nullptr;
  gpiod_line_config* config_= nullptr;
  gpiod_line_request* request_ = nullptr;

  bool up_last_ = false;
  bool down_last_ = false;

  bool up_pressed_ = false;
  bool down_pressed_ = false;

  int up_cooldown_ = 0;
  int down_cooldown_ = 0;
};

#endif
