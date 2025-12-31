#include "drv8243.h"

#include <cmath>

#include "esphome/core/log.h"
#include "esphome/core/hal.h"  // delay(), delayMicroseconds(), micros()

namespace esphome {
namespace drv8243 {

static const char *const TAG = "drv8243";

// Handshake timings
static constexpr uint32_t SLEEP_FORCE_MS = 2;             // ensure real SLEEP
static constexpr uint32_t READY_WAIT_TIMEOUT_US = 5000;   // wait for nFAULT LOW (ready)
static constexpr uint32_t ACK_WAIT_TIMEOUT_US = 5000;     // wait for nFAULT HIGH after ACK
static constexpr uint32_t POLL_STEP_US = 10;

// ACK pulse target:
// Keep close to the lower edge so occasional interrupt stretch is less likely to exceed ~40us.
static constexpr uint32_t ACK_PULSE_US = 22;

const char *DRV8243Output::handshake_result_str_(HandshakeResult r) const {
  switch (r) {
    case HandshakeResult::NOT_RUN:
      return "not_run";
    case HandshakeResult::VERIFIED_OK:
      return "verified_ok";
    case HandshakeResult::VERIFIED_FAIL:
      return "verified_fail";
    case HandshakeResult::UNVERIFIED:
      return "unverified";
    default:
      return "unknown";
  }
}

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
    ESP_LOGCONFIG(TAG, "  nFAULT pin: NOT SET (handshake will be unverified)");
  }

  if (direction_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  PH/direction pin: %s (direction_high=%s)",
                  direction_pin_->dump_summary().c_str(),
                  direction_high_ ? "true" : "false");
  } else {
    ESP_LOGCONFIG(TAG, "  PH/direction pin: NOT SET");
  }

  ESP_LOGCONFIG(TAG, "  Handshake: %s", handshake_result_str_(handshake_result_));
  ESP_LOGCONFIG(TAG, "  Polarity tip: if LED doesn't light, flip 'direction_high'.");
}

void DRV8243Output::setup() {
  // Keep setup lightweight — this runs before most people see logs over API.
  if (nsleep_pin_ != nullptr) {
    nsleep_pin_->setup();
    nsleep_pin_->pin_mode(gpio::FLAG_OUTPUT);
    nsleep_pin_->digital_write(true);  // default awake
  }

  if (nfault_pin_ != nullptr) {
    nfault_pin_->setup();
    nfault_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);  // internal pull-up
  }

  if (direction_pin_ != nullptr) {
    direction_pin_->setup();
    direction_pin_->pin_mode(gpio::FLAG_OUTPUT);
    direction_pin_->digital_write(direction_high_);
  }
}

DRV8243Output::HandshakeResult DRV8243Output::do_handshake_() {
  if (nsleep_pin_ == nullptr) {
    return HandshakeResult::VERIFIED_FAIL;
  }

  // Force sleep
  nsleep_pin_->digital_write(false);
  delay(SLEEP_FORCE_MS);

  // Wake
  nsleep_pin_->digital_write(true);

  // If we can observe nFAULT, try to verify the handshake.
  bool saw_ready_low = false;
  if (nfault_pin_ != nullptr) {
    uint32_t start = micros();
    while ((micros() - start) < READY_WAIT_TIMEOUT_US) {
      if (!nfault_pin_->digital_read()) {  // nFAULT LOW
        saw_ready_low = true;
        break;
      }
      delayMicroseconds(POLL_STEP_US);
    }
  }

  // ACK pulse (must stay short; no InterruptLock on ESP32-C3)
  nsleep_pin_->digital_write(false);
  delayMicroseconds(ACK_PULSE_US);
  nsleep_pin_->digital_write(true);

  if (nfault_pin_ == nullptr) {
    // Can't confirm; treat as unverified.
    return HandshakeResult::UNVERIFIED;
  }

  if (!saw_ready_low) {
    // We didn't see nFAULT assert LOW; still proceed, but we can't verify.
    return HandshakeResult::UNVERIFIED;
  }

  // Confirm nFAULT goes HIGH after ACK
  uint32_t start = micros();
  while ((micros() - start) < ACK_WAIT_TIMEOUT_US) {
    if (nfault_pin_->digital_read()) {  // nFAULT HIGH
      return HandshakeResult::VERIFIED_OK;
    }
    delayMicroseconds(POLL_STEP_US);
  }

  return HandshakeResult::VERIFIED_FAIL;
}

void DRV8243Output::write_state(float state) {
  if (raw_output_ == nullptr)
    return;

  // Run handshake the first time we’re asked to turn on (so logs are visible to typical users).
  if (!handshake_ran_ && state > 0.0005f) {
    this->run_handshake("first_on");
  }

  if (state <= 0.0005f) {
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

  raw_output_->set_level(y);
}

}  // namespace drv8243
}  // namespace esphome
