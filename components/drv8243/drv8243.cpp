#include "drv8243.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"  // delay(), delayMicroseconds()

namespace esphome {
namespace drv8243 {

static const char *const TAG = "drv8243";

bool DRV8243Output::global_initialized_ = false;

void DRV8243Output::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DRV8243 output");

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
    ESP_LOGCONFIG(TAG, "PH/direction pin set to %s",
                  direction_high_ ? "HIGH" : "LOW");
  }

  // Only first instance does the handshake
  if (global_initialized_) {
    ESP_LOGD(TAG, "DRV8243 already initialized; skipping handshake");
    return;
  }

  // ---- HANDSHAKE: mirror your working lambda ----
  ESP_LOGD(TAG, "Forcing SLEEP for 50ms");
  delay(50);  // ms

  // 1) Wake: nSLEEP HIGH
  if (nsleep_pin_ != nullptr) {
    ESP_LOGD(TAG, "Driving nSLEEP HIGH to wake DRV8243");
    nsleep_pin_->digital_write(true);
  }

  // 2) Wait up to 200 ms for nFAULT LOW (device ready)
  bool ready = false;
  if (nfault_pin_ != nullptr) {
    ESP_LOGD(TAG, "Waiting up to 200ms for nFAULT to assert LOW (ready)");
    for (int i = 0; i < 200; i++) {  // 200 x 1ms = 200ms
      bool fault_level = nfault_pin_->digital_read();
      if (!fault_level) {
        ESP_LOGD(TAG, "nFAULT went LOW after %d ms – device ready", i);
        ready = true;
        break;
      }
      delay(1);
    }
    if (!ready) {
      ESP_LOGW(TAG, "Timeout waiting for nFAULT LOW; continuing anyway");
    }
  } else {
    ready = true;
  }

  // 3) ACK pulse: nSLEEP LOW for ~10 µs (<40 µs), then HIGH.
  // Same behaviour as your working Arduino lambda.
  if (nsleep_pin_ != nullptr) {
    ESP_LOGD(TAG, "Issuing ACK pulse on nSLEEP (~10us LOW)");
    nsleep_pin_->digital_write(false);
    delayMicroseconds(10);
    nsleep_pin_->digital_write(true);
    ESP_LOGD(TAG, "ACK pulse complete; nSLEEP held HIGH");
  }

  global_initialized_ = true;
  ESP_LOGCONFIG(TAG, "DRV8243 handshake complete");
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
