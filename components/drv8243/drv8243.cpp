#include "drv8243.h"

#include <cmath>

#include "esphome/core/log.h"
#include "esphome/core/hal.h"  // delay(), delayMicroseconds(), micros()

namespace esphome {
namespace drv8243 {

static const char *const TAG = "drv8243";

// Handshake timings
static constexpr uint32_t SLEEP_FORCE_MS = 2;
static constexpr uint32_t READY_WAIT_TIMEOUT_US = 5000;
static constexpr uint32_t ACK_WAIT_TIMEOUT_US = 5000;
static constexpr uint32_t POLL_STEP_US = 10;

// ACK pulse: keep close to lower edge to reduce risk of stretching above ~40us
static constexpr uint32_t ACK_PULSE_US = 22;

// Fault polling (simple + light)
static constexpr uint32_t FAULT_POLL_MS = 100;

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
  ESP_LOGCONFIG(TAG, "  OUT1 (PWM): configured");
  ESP_LOGCONFIG(TAG, "  nSLEEP pin: %s", nsleep_pin_ ? nsleep_pin_->dump_summary().c_str() : "NOT SET");
  ESP_LOGCONFIG(TAG, "  nFAULT pin: %s", nfault_pin_ ? nfault_pin_->dump_summary().c_str() : "NOT SET");

  if (out2_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  OUT2 pin: %s", out2_pin_->dump_summary().c_str());
    ESP_LOGCONFIG(TAG, "  Flip polarity: %s (OUT2=%s)",
                  flip_polarity_ ? "true" : "false",
                  flip_polarity_ ? "HIGH" : "LOW");
  } else {
    ESP_LOGCONFIG(TAG, "  OUT2 pin: NOT SET");
  }

  ESP_LOGCONFIG(TAG, "  Handshake: %s", handshake_result_str_(handshake_result_));
  ESP_LOGCONFIG(TAG, "  Tip: if LED doesn't light, toggle 'flip_polarity'.");
}

void DRV8243Output::setup() {
  // Keep setup very light (no delays/pulses here)
  if (nsleep_pin_) {
    nsleep_pin_->setup();
    nsleep_pin_->pin_mode(gpio::FLAG_OUTPUT);
    nsleep_pin_->digital_write(true);  // default awake
  }

  if (nfault_pin_) {
    nfault_pin_->setup();
    nfault_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  }

  if (out2_pin_) {
    out2_pin_->setup();
    out2_pin_->pin_mode(gpio::FLAG_OUTPUT);
    out2_pin_->digital_write(flip_polarity_);  // default false => LOW
  }

  #ifdef USE_BINARY_SENSOR
    if (fault_sensor_ && nfault_pin_) {
    // If fault sensor exists, publish initial state once (won't spam)
      bool fault_active = !nfault_pin_->digital_read();  // active-low
      last_fault_ = fault_active;
      fault_sensor_->publish_state(fault_active);
    }
  #endif

}

void DRV8243Output::loop() {
  #ifdef USE_BINARY_SENSOR
    // Optional fault reporting
    if (!fault_sensor_ || !nfault_pin_)
      return;

    const uint32_t now = millis();
    if ((now - last_fault_check_ms_) < FAULT_POLL_MS)
      return;
    last_fault_check_ms_ = now;

    bool fault_active = !nfault_pin_->digital_read();
    if (fault_active != last_fault_) {
      last_fault_ = fault_active;
      fault_sensor_->publish_state(fault_active);
    }
  #endif

}

DRV8243Output::HandshakeResult DRV8243Output::do_handshake_() {
  if (!nsleep_pin_)
    return HandshakeResult::VERIFIED_FAIL;

  // Force sleep then wake
  nsleep_pin_->digital_write(false);
  delay(SLEEP_FORCE_MS);
  nsleep_pin_->digital_write(true);

  // Wait for nFAULT LOW if available (device-ready indication)
  bool saw_ready_low = false;
  if (nfault_pin_) {
    uint32_t start = micros();
    while ((micros() - start) < READY_WAIT_TIMEOUT_US) {
      if (!nfault_pin_->digital_read()) {  // LOW
        saw_ready_low = true;
        break;
      }
      delayMicroseconds(POLL_STEP_US);
    }
  }

  // ACK pulse (no InterruptLock on ESP32-C3)
  nsleep_pin_->digital_write(false);
  delayMicroseconds(ACK_PULSE_US);
  nsleep_pin_->digital_write(true);

  if (!nfault_pin_)
    return HandshakeResult::UNVERIFIED;

  if (!saw_ready_low)
    return HandshakeResult::UNVERIFIED;

  // Confirm nFAULT HIGH after ACK
  uint32_t start = micros();
  while ((micros() - start) < ACK_WAIT_TIMEOUT_US) {
    if (nfault_pin_->digital_read())  // HIGH
      return HandshakeResult::VERIFIED_OK;
    delayMicroseconds(POLL_STEP_US);
  }

  return HandshakeResult::VERIFIED_FAIL;
}

void DRV8243Output::write_state(float state) {
  if (!out1_output_)
    return;

  // Run handshake once, first time we're asked to turn on (so users see logs)
  if (!handshake_ran_ && state > 0.0005f) {
    ESP_LOGI(TAG, "Starting DRV8243 (flip_polarity=%s)", flip_polarity_ ? "true" : "false");
    handshake_result_ = do_handshake_();
    handshake_ran_ = true;

    if (handshake_result_ == HandshakeResult::VERIFIED_OK) {
      ESP_LOGI(TAG, "DRV8243 ready (verified via nFAULT)");
    } else if (handshake_result_ == HandshakeResult::UNVERIFIED) {
      ESP_LOGW(TAG, "DRV8243 started (nFAULT not verified)");
    } else {
      ESP_LOGE(TAG, "DRV8243 failed to start (check wiring / pull-ups / nSLEEP)");
    }
  }

  if (state <= 0.0005f) {
    out1_output_->set_level(0.0f);
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

  out1_output_->set_level(y);
}

}  // namespace drv8243
}  // namespace esphome
