#include "drv8243.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"  // for delayMicroseconds()

#ifdef USE_ESP_IDF
#include "esp_rom_sys.h"
#endif

namespace esphome {
namespace drv8243 {

bool DRV8243Output::global_initialized_ = false;

// Simple ms delay helper – only used once at boot
static inline void drv_delay_ms(uint32_t ms) {
#ifdef USE_ESP_IDF
  // Busy-wait; fine for one-time startup
  esp_rom_delay_us(ms * 1000);
#else
  delay(ms);
#endif
}

void DRV8243Output::setup() {
  // Configure pins
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

  // ---- HANDSHAKE: mirror your working lambda ----

  // 1) Force SLEEP (already low), hold ~50 ms
  drv_delay_ms(5000);

  // 2) Wake: nSLEEP HIGH
  if (nsleep_pin_ != nullptr) {
    nsleep_pin_->digital_write(true);
  }

  // 3) Wait up to 200 ms for nFAULT LOW (device ready)
  bool ready = false;
  if (nfault_pin_ != nullptr) {
    for (int i = 0; i < 200; i++) {     // 200 x 1 ms = 200 ms
      if (!nfault_pin_->digital_read()) {
        ready = true;
        break;
      }
      drv_delay_ms(1);
    }
  } else {
    // No nFAULT pin wired in – assume ready
    ready = true;
  }

  // 4) ACK pulse: nSLEEP LOW for ~10 µs (<40 µs), then HIGH.
  // Do it even if we didn't see nFAULT low, like your original code.
  if (nsleep_pin_ != nullptr) {
    nsleep_pin_->digital_write(false);
    esphome::delayMicroseconds(10);
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
    // Linear clamp only
    y = min_level_ + (1.0f - min_level_) * x;
  } else {
    // Perceptual shaping
    y = min_level_ + (1.0f - min_level_) * powf(x, exponent_);
  }

  if (y < 0.0f) y = 0.0f;
  if (y > 1.0f) y = 1.0f;

  raw_output_->set_level(y);
}

}  // namespace drv8243
}  // namespace esphome
