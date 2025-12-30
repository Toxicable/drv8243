#include "drv8243.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"  // delay(), delayMicroseconds()

namespace esphome {
namespace drv8243 {

static const char *const TAG = "drv8243";

bool DRV8243Output::global_initialized_ = false;

void DRV8243Output::dump_config() {
  ESP_LOGCONFIG(TAG, "DRV8243 Output");
  ESP_LOGCONFIG(TAG, "  Min level: %.4f", this->min_level_);
  ESP_LOGCONFIG(TAG, "  Exponent: %.2f", this->exponent_);

  if (nsleep_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  nSLEEP pin: %s", nsleep_pin_->dump_summary().c_str());
  } else {
    ESP_LOGCONFIG(TAG, "  nSLEEP pin: NOT SET");
  }

  if (nfault_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  nFAULT pin: %s", nfault_pin_->dump_summary().c_str());
  } else {
    ESP_LOGCONFIG(TAG, "  nFAULT pin: NOT SET");
  }

  if (direction_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Direction (PH) pin: %s (initial=%s)",
                  direction_pin_->dump_summary().c_str(),
                  direction_high_ ? "HIGH" : "LOW");
  } else {
    ESP_LOGCONFIG(TAG, "  Direction (PH) pin: NOT SET");
  }

  ESP_LOGCONFIG(TAG, "  Handshake done: %s",
                handshake_done_ ? "YES" : "NO");

  // Safety net: if for some reason setup() ran before logger and we
  // couldn't see it, run handshake once here if it hasn't been done.
  if (!handshake_done_) {
    ESP_LOGW(TAG, "Handshake not yet done at dump_config(); running now");
    this->do_handshake_();
  }
}

void DRV8243Output::setup() {
  ESP_LOGD(TAG, "Setting up DRV8243 output (setup)");

  // Configure pins
  if (nsleep_pin_ != nullptr) {
    nsleep_pin_->setup();
    nsleep_pin_->pin_mode(gpio::FLAG_OUTPUT);
    nsleep_pin_->digital_write(false);  // start in SLEEP
    ESP_LOGD(TAG, "nSLEEP configured as output, driven LOW (SLEEP)");
  } else {
    ESP_LOGW(TAG, "nSLEEP pin not configured! DRV8243 will not wake.");
  }

  if (nfault_pin_ != nullptr) {
    nfault_pin_->setup();
    nfault_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
    ESP_LOGD(TAG, "nFAULT configured as input with pull-up");
  } else {
    ESP_LOGW(TAG, "nFAULT pin not configured; assuming device is always ready");
  }

  if (direction_pin_ != nullptr) {
    direction_pin_->setup();
    direction_pin_->pin_mode(gpio::FLAG_OUTPUT);
    direction_pin_->digital_write(direction_high_);
    ESP_LOGD(TAG, "PH/direction pin set to %s",
             direction_high_ ? "HIGH" : "LOW");
  }

  // Only first instance actually needs the handshake, but we track a per-instance flag too.
  if (global_initialized_) {
    ESP_LOGD(TAG, "DRV8243 already globally initialized; skipping handshake");
    handshake_done_ = true;
    return;
  }

  this->do_handshake_();
}

void DRV8243Output::do_handshake_() {
  if (handshake_done_) {
    ESP_LOGD(TAG, "Handshake already done; skipping");
    return;
  }

  if (nsleep_pin_ == nullptr) {
    ESP_LOGW(TAG, "Cannot run handshake: nSLEEP pin not set");
    return;
  }

  // ---- HANDSHAKE: mirror your known-good lambda ----

  // 1) Force SLEEP for 50 ms (nSLEEP already LOW)
  ESP_LOGD(TAG, "Handshake: forcing SLEEP for 50ms");
  delay(50);

  // 2) Wake: nSLEEP HIGH
  ESP_LOGD(TAG, "Handshake: driving nSLEEP HIGH to wake DRV8243");
  nsleep_pin_->digital_write(true);

  // 3) Wait up to 200 ms for nFAULT LOW (device ready)
  bool ready = false;
  if (nfault_pin_ != nullptr) {
    ESP_LOGD(TAG, "Handshake: waiting up to 200ms for nFAULT LOW (ready)");
    for (int i = 0; i < 200; i++) {  // 200 x 1ms
      bool fault_level = nfault_pin_->digital_read();
      if (!fault_level) {
        ESP_LOGD(TAG, "Handshake: nFAULT went LOW after %d ms – device ready", i);
        ready = true;
        break;
      }
      delay(1);
    }
    if (!ready) {
      ESP_LOGW(TAG, "Handshake: timeout waiting for nFAULT LOW; continuing anyway");
    }
  } else {
    ready = true;
  }

  // 4) ACK pulse: nSLEEP LOW for ~10 µs (<40 µs), then HIGH
  ESP_LOGD(TAG, "Handshake: issuing ACK pulse on nSLEEP (~10us LOW)");
  nsleep_pin_->digital_write(false);
  delayMicroseconds(10);
  nsleep_pin_->digital_write(true);
  ESP_LOGD(TAG, "Handshake: ACK pulse complete; nSLEEP held HIGH");

  handshake_done_ = true;
  global_initialized_ = true;
  ESP_LOGD(TAG, "Handshake: done (handshake_done_=true, global_initialized_=true)");
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
