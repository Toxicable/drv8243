#include "drv8243.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"  // delay(), delayMicroseconds()

namespace esphome {
namespace drv8243 {

static const char *const TAG = "drv8243";

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

  // Always run handshake *once per boot-log* so we can see it.
  ESP_LOGW(TAG, "Running DRV8243 wake/ACK handshake from dump_config()");
  this->do_handshake_();
}

void DRV8243Output::setup() {
  ESP_LOGD(TAG, "Setting up DRV8243 output (setup)");

  // Configure pins only – no handshake here.
  if (nsleep_pin_ != nullptr) {
    nsleep_pin_->setup();
    nsleep_pin_->pin_mode(gpio::FLAG_OUTPUT);
    nsleep_pin_->digital_write(true);  // default: AWAKE
    ESP_LOGD(TAG, "nSLEEP configured as output, driven HIGH (awake)");
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
}

void DRV8243Output::do_handshake_() {
  if (nsleep_pin_ == nullptr) {
    ESP_LOGW(TAG, "Cannot run handshake: nSLEEP pin not set");
    return;
  }

  ESP_LOGD(TAG, "=== DRV8243 wake/ACK handshake start ===");

  // 1) Force SLEEP: nSLEEP LOW 50ms
  nsleep_pin_->digital_write(false);
  ESP_LOGD(TAG, "Handshake: nSLEEP -> LOW (force sleep) 50ms");
  delay(50);

  // 2) Wake: nSLEEP HIGH
  nsleep_pin_->digital_write(true);
  ESP_LOGD(TAG, "Handshake: nSLEEP -> HIGH (wake)");

  // 3) Wait up to 200ms for nFAULT LOW
  bool saw_ready = false;
  if (nfault_pin_ != nullptr) {
    ESP_LOGD(TAG, "Handshake: waiting up to 200ms for nFAULT LOW");
    for (int i = 0; i < 200; i++) {
      if (!nfault_pin_->digital_read()) {
        ESP_LOGD(TAG, "Handshake: nFAULT LOW after %d ms", i);
        saw_ready = true;
        break;
      }
      delay(1);
    }
    if (!saw_ready) {
      ESP_LOGW(TAG, "Handshake: nFAULT never went LOW (timeout), ACKing anyway");
    }
  }

  // 4) ACK pulse: nSLEEP LOW ~10us, then HIGH
  ESP_LOGD(TAG, "Handshake: issuing ACK pulse on nSLEEP (~10us LOW)");
  nsleep_pin_->digital_write(false);
  delayMicroseconds(10);
  nsleep_pin_->digital_write(true);
  ESP_LOGD(TAG, "Handshake: ACK pulse complete; nSLEEP set HIGH");

  // 5) Small settle delay, then re-read pins
  delay(5);
  bool fault_state = nfault_pin_ ? nfault_pin_->digital_read() : true;
  bool sleep_state = nsleep_pin_->digital_read();
  ESP_LOGD(TAG, "Handshake end: nFAULT=%s, nSLEEP=%s",
           fault_state ? "HIGH" : "LOW",
           sleep_state ? "HIGH" : "LOW");

  // 6) Force final awake state just in case
  nsleep_pin_->digital_write(true);
  ESP_LOGD(TAG, "Handshake: final nSLEEP forced HIGH");

  ESP_LOGD(TAG, "=== DRV8243 wake/ACK handshake end ===");
}
void DRV8243Output::write_state(float state) {
  if (raw_output_ == nullptr)
    return;

  ESP_LOGD(TAG, "write_state: requested=%.3f", state);

  if (state <= 0.0005f) {
    ESP_LOGD(TAG, "write_state: OFF (<= 0.0005)");
    raw_output_->set_level(0.0f);
    return;
  }

  float x = state;
  if (x < 0.0f) x = 0.0f;
  if (x > 1.0f) x = 1.0f;

  float y;
  if (exponent_ <= 0.0f) {
    y = min_level_ + (1.0f - min_level_) * x;
  } else {
    y = min_level_ + (1.0f - min_level_) * powf(x, exponent_);
  }

  if (y < 0.0f) y = 0.0f;
  if (y > 1.0f) y = 1.0f;

  ESP_LOGD(TAG, "write_state: mapped to raw_level=%.3f", y);
  raw_output_->set_level(y);
}

}  // namespace drv8243
}  // namespace esphome
