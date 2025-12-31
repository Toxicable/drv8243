#include "drv8243.h"

#include <cmath>

#include "esphome/core/log.h"
#include "esphome/core/hal.h"        // delay(), delayMicroseconds(), micros()
#include "esphome/core/helpers.h"    // InterruptLock

namespace esphome {
namespace drv8243 {

static const char *const TAG = "drv8243";

// Handshake tuneables
static constexpr uint32_t SLEEP_FORCE_MS = 2;             // force SLEEP long enough to be unambiguous
static constexpr uint32_t READY_WAIT_TIMEOUT_US = 5000;   // wait for nFAULT LOW up to 5ms
static constexpr uint32_t ACK_WAIT_TIMEOUT_US = 5000;     // wait for nFAULT HIGH up to 5ms
static constexpr uint32_t POLL_STEP_US = 10;              // polling granularity
static constexpr uint32_t ACK_PULSE_US = 22;              // target in ~20–40us window
static constexpr uint32_t DEFER_HANDSHAKE_MS = 1000;      // delay handshake so firmware can boot + logs show

float DRV8243Output::get_setup_priority() const {
  // Run late to ensure logger and networking are up before we start toggling hardware.
  return setup_priority::LATE;
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
    ESP_LOGCONFIG(TAG, "  nFAULT pin: NOT SET");
  }

  if (direction_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Direction (PH) pin: %s (initial=%s)",
                  direction_pin_->dump_summary().c_str(),
                  direction_high_ ? "HIGH" : "LOW");
  } else {
    ESP_LOGCONFIG(TAG, "  Direction (PH) pin: NOT SET");
  }

  ESP_LOGCONFIG(TAG, "  Handshake attempts: %u", (unsigned) handshake_attempts_);
  ESP_LOGCONFIG(TAG, "  Handshake done: %s", handshaked_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Handshake ok:   %s", handshake_ok_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "    Saw nFAULT LOW (ready): %s", saw_nfault_low_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "    Saw nFAULT HIGH after ACK: %s", saw_nfault_high_after_ack_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "    Wait LOW time:  %u us", (unsigned) t_wait_low_us_);
  ESP_LOGCONFIG(TAG, "    Wait HIGH time: %u us", (unsigned) t_wait_high_us_);
}

void DRV8243Output::setup() {
  ESP_LOGI(TAG, "setup: begin");

//   pin setup only — no sleeps, no pulses
  if (nsleep_pin_) { nsleep_pin_->setup(); nsleep_pin_->pin_mode(gpio::FLAG_OUTPUT); nsleep_pin_->digital_write(true); }
  if (nfault_pin_) { nfault_pin_->setup(); nfault_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP); }
  if (direction_pin_) { direction_pin_->setup(); direction_pin_->pin_mode(gpio::FLAG_OUTPUT); direction_pin_->digital_write(direction_high_); }

  ESP_LOGI(TAG, "setup: pins configured, deferring handshake 3s");
  this->set_timeout("drv8243_hs", 3000, [this]() {
    ESP_LOGI(TAG, "handshake: starting (deferred)");
    this->do_handshake_("deferred");
    ESP_LOGI(TAG, "handshake: done");
  });

  ESP_LOGI(TAG, "setup: end");
}

void DRV8243Output::run_handshake(const char *reason) {
  ESP_LOGW(TAG, "run_handshake(): requested (reason=%s)", reason);

  // (Optional) make sure PWM is off while we do the handshake to avoid flicker
  if (raw_output_ != nullptr) {
    ESP_LOGI(TAG, "run_handshake(): forcing raw_output OFF (0.0) before handshake");
    raw_output_->set_level(0.0f);
  }

  bool ok = this->do_handshake_(reason);
  this->handshaked_ = true;
  this->handshake_ok_ = ok;

  ESP_LOGW(TAG, "run_handshake(): done ok=%s", ok ? "true" : "false");
}

void DRV8243Output::pulse_nsleep_ack_() {
  if (nsleep_pin_ == nullptr) return;

  ESP_LOGV(TAG, "pulse: ACK pulse start (target LOW %u us)", (unsigned) ACK_PULSE_US);

  nsleep_pin_->digital_write(false);
  const uint32_t t0 = micros();
  delayMicroseconds(ACK_PULSE_US);
  const uint32_t low_us = micros() - t0;
  nsleep_pin_->digital_write(true);

  ESP_LOGV(TAG, "pulse: ACK pulse end (measured LOW ~%u us)", (unsigned) low_us);

  if (low_us >= 40) {
    ESP_LOGW(TAG, "pulse: measured LOW >= 40us (%u us) -> likely crossed into SLEEP/undefined window", (unsigned) low_us);
  }
}

bool DRV8243Output::do_handshake_(const char *reason) {
    // return true;
  if (handshake_in_progress_) {
    ESP_LOGW(TAG, "handshake: already in progress; skipping (reason=%s)", reason);
    return handshake_ok_;
  }
  handshake_in_progress_ = true;
  handshake_attempts_++;

  // Reset debug fields
  saw_nfault_low_ = false;
  saw_nfault_high_after_ack_ = false;
  t_wait_low_us_ = 0;
  t_wait_high_us_ = 0;

  if (nsleep_pin_ == nullptr) {
    ESP_LOGE(TAG, "handshake: cannot run, nSLEEP is null (reason=%s)", reason);
    handshake_in_progress_ = false;
    return false;
  }

  ESP_LOGI(TAG, "handshake: === start === reason=%s attempt=%u", reason, (unsigned) handshake_attempts_);

  // Snapshot initial states
  bool init_nsleep = nsleep_pin_->digital_read();
  bool init_nfault = nfault_pin_ ? nfault_pin_->digital_read() : true;
  ESP_LOGI(TAG, "handshake: initial: nSLEEP=%s nFAULT=%s",
           init_nsleep ? "HIGH" : "LOW",
           init_nfault ? "HIGH" : "LOW");

  // Step 1: Force SLEEP
  ESP_LOGI(TAG, "handshake: step1 force SLEEP: nSLEEP->LOW for %u ms", (unsigned) SLEEP_FORCE_MS);
  nsleep_pin_->digital_write(false);
  delay(SLEEP_FORCE_MS);

  // Step 2: Wake
  ESP_LOGI(TAG, "handshake: step2 wake: nSLEEP->HIGH");
  nsleep_pin_->digital_write(true);

  // Step 3: Wait for nFAULT LOW (ready), if we can observe it
  if (nfault_pin_ != nullptr) {
    ESP_LOGI(TAG, "handshake: step3 wait nFAULT LOW (ready), timeout=%u us", (unsigned) READY_WAIT_TIMEOUT_US);
    uint32_t start = micros();
    while ((micros() - start) < READY_WAIT_TIMEOUT_US) {
      if (!nfault_pin_->digital_read()) {
        saw_nfault_low_ = true;
        t_wait_low_us_ = micros() - start;
        ESP_LOGI(TAG, "handshake: step3 success: nFAULT LOW after %u us", (unsigned) t_wait_low_us_);
        break;
      }
      delayMicroseconds(POLL_STEP_US);
    }
    if (!saw_nfault_low_) {
      t_wait_low_us_ = micros() - start;
      ESP_LOGW(TAG, "handshake: step3 timeout: never saw nFAULT LOW after %u us", (unsigned) t_wait_low_us_);
    }
  } else {
    ESP_LOGW(TAG, "handshake: step3 skipped (no nFAULT). delaying 2ms best-effort");
    delay(2);
  }

  // Step 4: ACK pulse (target ~30us LOW)
  ESP_LOGI(TAG, "handshake: step4 ACK pulse on nSLEEP (LOW ~%u us)", (unsigned) ACK_PULSE_US);
  this->pulse_nsleep_ack_();

  // Step 5: Confirm nFAULT HIGH after ACK (if we saw LOW and have the pin)
  if (nfault_pin_ != nullptr && saw_nfault_low_) {
    ESP_LOGI(TAG, "handshake: step5 wait nFAULT HIGH after ACK, timeout=%u us", (unsigned) ACK_WAIT_TIMEOUT_US);
    uint32_t start = micros();
    while ((micros() - start) < ACK_WAIT_TIMEOUT_US) {
      if (nfault_pin_->digital_read()) {
        saw_nfault_high_after_ack_ = true;
        t_wait_high_us_ = micros() - start;
        ESP_LOGI(TAG, "handshake: step5 success: nFAULT HIGH after %u us", (unsigned) t_wait_high_us_);
        break;
      }
      delayMicroseconds(POLL_STEP_US);
    }
    if (!saw_nfault_high_after_ack_) {
      t_wait_high_us_ = micros() - start;
      ESP_LOGW(TAG, "handshake: step5 timeout: nFAULT did not go HIGH after %u us", (unsigned) t_wait_high_us_);
    }
  } else if (nfault_pin_ != nullptr) {
    bool nf = nfault_pin_->digital_read();
    ESP_LOGW(TAG, "handshake: step5 skipped confirm (never saw LOW). current nFAULT=%s",
             nf ? "HIGH" : "LOW");
  } else {
    ESP_LOGW(TAG, "handshake: step5 cannot confirm (no nFAULT)");
  }
  return true;

  // Final states
  bool end_nsleep = nsleep_pin_->digital_read();
  bool end_nfault = nfault_pin_ ? nfault_pin_->digital_read() : true;
  ESP_LOGI(TAG, "handshake: end: nSLEEP=%s nFAULT=%s",
           end_nsleep ? "HIGH" : "LOW",
           end_nfault ? "HIGH" : "LOW");

  // Always leave awake
  nsleep_pin_->digital_write(true);
  ESP_LOGI(TAG, "handshake: final: nSLEEP forced HIGH");

  // Determine success
  bool ok = true;
  if (nfault_pin_ != nullptr) {
    ok = saw_nfault_low_ && saw_nfault_high_after_ack_;
  } else {
    ok = true;  // best-effort when we can't observe nFAULT
  }

  ESP_LOGI(TAG, "handshake: === end === ok=%s", ok ? "true" : "false");
  handshake_in_progress_ = false;
  return ok;
}

void DRV8243Output::write_state(float state) {
  if (raw_output_ == nullptr) {
    ESP_LOGE(TAG, "write_state: raw_output is null");
    return;
  }

  ESP_LOGD(TAG, "write_state: requested=%.3f handshaked=%s ok=%s",
           state,
           handshaked_ ? "true" : "false",
           handshake_ok_ ? "true" : "false");

  // If the deferred handshake hasn't run yet, do a synchronous one on first use
  // so the first brightness change is deterministic.
  if (!handshaked_ && nsleep_pin_ != nullptr) {
    ESP_LOGW(TAG, "write_state: handshake not run yet (deferred). Running now (sync).");
    raw_output_->set_level(0.0f);
    handshake_ok_ = do_handshake_("first_write_state_sync");
    handshaked_ = true;
    ESP_LOGI(TAG, "write_state: sync handshake done ok=%s", handshake_ok_ ? "true" : "false");
  }

  if (state <= 0.0005f) {
    ESP_LOGD(TAG, "write_state: OFF");
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

  ESP_LOGD(TAG, "write_state: mapped %.3f -> raw_level=%.3f (min=%.4f exp=%.2f)",
           x, y, min_level_, exponent_);
  raw_output_->set_level(y);
}

}  // namespace drv8243
}  // namespace esphome
