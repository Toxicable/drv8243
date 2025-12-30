#include "drv8243.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h" 

#ifdef USE_ESP_IDF
#include "esp_rom_sys.h"
#endif

namespace esphome {
namespace drv8243 {

bool DRV8243Output::global_initialized_ = false;


void DRV8243Output::setup() {
  if (nsleep_pin_ != nullptr) {
    nsleep_pin_->setup();
    nsleep_pin_->pin_mode(gpio::FLAG_OUTPUT);
    nsleep_pin_->digital_write(false);  // start in SLEEP
  }

  if (nfault_pin_ != nullptr) {
    nfault_pin_->setup();
    nfault_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  }

  if (direction_pin_ != nullptr) {
    direction_pin_->setup();
    direction_pin_->pin_mode(gpio::FLAG_OUTPUT);
    direction_pin_->digital_write(direction_high_);
  }

  // Only first instance does the handshake
  if (global_initialized_)
    return;

  // Let supply rails settle
  delay(300);

  // 1) Force SLEEP, then wake
  if (nsleep_pin_ != nullptr) {
    nsleep_pin_->digital_write(false);
    delay(100);
    nsleep_pin_->digital_write(true);
  }

  // 2) Wait for nFAULT LOW (device ready) up to ~1500 ms
  bool ready = false;
  if (nfault_pin_ != nullptr) {
    for (int i = 0; i < 1500; i++) {
      if (!nfault_pin_->digital_read()) {
        ready = true;
        break;
      }
      delay(1);  // 1 ms step
    }
  } else {
    // No nFAULT pin provided – assume ready
    ready = true;
  }

  // 3) ACK pulse: nSLEEP LOW for ~15 µs (<40 µs), then HIGH
  if (ready && nsleep_pin_ != nullptr) {
    nsleep_pin_->digital_write(false);
    #ifdef USE_ESP_IDF
      esp_rom_delay_us(15);
    #else
      delayMicroseconds(15);
    #endif
    nsleep_pin_->digital_write(true);
  }

  global_initialized_ = true;
}

void DRV8243Output::write_state(float state) {
  if (raw_output_ == nullptr)
    return;

  // Fully off
  if (state <= 0.0005f) {
    raw_output_->set_level(0.0f);
    return;
  }

  float x = state;
  if (x < 0.0f) x = 0.0f;
  if (x > 1.0f) x = 1.0f;

  float y;
  if (exponent_ <= 0.0f) {
    // Linear clamp
    y = min_level_ + (1.0f - min_level_) * x;
  } else {
    // Shaped curve
    y = min_level_ + (1.0f - min_level_) * powf(x, exponent_);
  }

  if (y < 0.0f) y = 0.0f;
  if (y > 1.0f) y = 1.0f;

  raw_output_->set_level(y);
}

}  // namespace drv8243
}  // namespace esphome
