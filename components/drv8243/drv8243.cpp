// drv8243.cpp
#include "drv8243.h"

#include <cmath>

#include "esphome/core/log.h"
#include "esphome/core/hal.h"       // delay(), delayMicroseconds(), micros()
#include "esphome/core/helpers.h"   // InterruptLock

namespace esphome {
namespace drv8243 {

static const char *const TAG = "drv8243";

// Tuneables for handshake behavior
static constexpr uint32_t SLEEP_FORCE_MS = 2;          // force SLEEP long enough to be unambiguous
static constexpr uint32_t READY_WAIT_TIMEOUT_US = 5000; // wait for nFAULT LOW (ready) up to 5ms
static constexpr uint32_t ACK_WAIT_TIMEOUT_US = 5000;   // wait for nFAULT HIGH after ACK up to 5ms
static constexpr uint32_t POLL_STEP_US = 10;            // polling granularity
static constexpr uint32_t ACK_PULSE_US = 30;            // ACK pulse target (aim mid-window ~20–40us)

void DRV8243Output::dump_config() {
  ESP_LOGCONFIG(TAG, "DRV8243 Output Config");
//   ESP_LOGCONFIG(TAG, "  Min level: %.4f", this->min_level_);
//   ESP_LOGCONFIG(TAG, "  Exponent: %.2f", this->exponent_);

//   if (nsleep_pin_ != nullptr) {
//     ESP_LOGCONFIG(TAG, "  nSLEEP pin: %s", nsleep_pin_->dump_summary().c_str());
//   } else {
//     ESP_LOGCONFIG(TAG, "  nSLEEP pin: NOT SET");
//   }

//   if (nfault_pin_ != nullptr) {
//     ESP_LOGCONFIG(TAG, "  nFAULT pin: %s", nfault_pin_->dump_summary().c_str());
//   } else {
//     ESP_LOGCONFIG(TAG, "  nFAULT pin: NOT SET");
//   }

//   if (direction_pin_ != nullptr) {
//     ESP_LOGCONFIG(TAG, "  Direction (PH) pin: %s (initial=%s)",
//                   direction_pin_->dump_summary().c_str(),
//                   direction_high_ ? "HIGH" : "LOW");
//   } else {
//     ESP_LOGCONFIG(TAG, "  Direction (PH) pin: NOT SET");
//   }

//   ESP_LOGCONFIG(TAG, "  Handshake done: %s", handshaked_ ? "YES" : "NO");
//   ESP_LOGCONFIG(TAG, "  Handshake ok:   %s", handshake_ok_ ? "YES" : "NO");
//   ESP_LOGCONFIG(TAG, "    Saw nFAULT LOW (ready): %s", saw_nfault_low_ ? "YES" : "NO");
//   ESP_LOGCONFIG(TAG, "    Saw nFAULT HIGH after ACK: %s", saw_nfault_high_after_ack_ ? "YES" : "NO");
//   ESP_LOGCONFIG(TAG, "    Wait LOW time:  %u us", (unsigned) t_wait_low_us_);
//   ESP_LOGCONFIG(TAG, "    Wait HIGH time: %u us", (unsigned) t_wait_high_us_);
}

void DRV8243Output::setup() {
  ESP_LOGD(TAG, "Setting up DRV8243 output (setup)");

//   // Configure pins
//   if (nsleep_pin_ != nullptr) {
//     nsleep_pin_->setup();
//     nsleep_pin_->pin_mode(gpio::FLAG_OUTPUT);
//     // We'll drive it explicitly during handshake
//     ESP_LOGD(TAG, "nSLEEP configured as output");
//   } else {
//     ESP_LOGW(TAG, "nSLEEP pin not configured! Driver wake/ACK cannot run.");
//   }

//   if (nfault_pin_ != nullptr) {
//     nfault_pin_->setup();
//     nfault_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
//     ESP_LOGD(TAG, "nFAULT configured as input with pull-up");
//   } else {
//     ESP_LOGW(TAG, "nFAULT pin not configured; handshake will be timing-based (less reliable).");
//   }

//   if (direction_pin_ != nullptr) {
//     direction_pin_->setup();
//     direction_pin_->pin_mode(gpio::FLAG_OUTPUT);
//     direction_pin_->digital_write(direction_high_);
//     ESP_LOGD(TAG, "PH/direction pin set to %s", direction_high_ ? "HIGH" : "LOW");
//   }

//   // Ensure bridge is off before messing with nSLEEP
//   if (raw_output_ != nullptr) {
//     ESP_LOGD(TAG, "setup: forcing raw_output to 0.0 before handshake");
//     raw_output_->set_level(0.0f);
//   } else {
//     ESP_LOGW(TAG, "setup: raw_output not set yet");
//   }

//   ESP_LOGI(TAG, "setup: running DRV8243 wake/ACK handshake");
// //   handshake_ok_ = this->do_handshake_();
//   handshaked_ = true;

//   ESP_LOGI(TAG, "setup: handshake complete. ok=%s saw_ready_low=%s saw_high_after_ack=%s",
//            handshake_ok_ ? "true" : "false",
//            saw_nfault_low_ ? "true" : "false",
//            saw_nfault_high_after_ack_ ? "true" : "false");
}

// void DRV8243Output::pulse_nsleep_ack_() {
//   // nSLEEP ACK pulse: LOW for ~30us then HIGH.
//   // We use InterruptLock to reduce jitter so the pulse stays in the correct window.
//   ESP_LOGV(TAG, "pulse: entering critical section for ACK pulse");
//   InterruptLock lock;

//   nsleep_pin_->digital_write(false);
//   delayMicroseconds(ACK_PULSE_US);
//   nsleep_pin_->digital_write(true);

//   ESP_LOGV(TAG, "pulse: ACK pulse done (LOW %u us)", (unsigned) ACK_PULSE_US);
// }

// bool DRV8243Output::do_handshake_() {
//   // Reset debug fields
//   saw_nfault_low_ = false;
//   saw_nfault_high_after_ack_ = false;
//   t_wait_low_us_ = 0;
//   t_wait_high_us_ = 0;

//   if (nsleep_pin_ == nullptr) {
//     ESP_LOGW(TAG, "handshake: cannot run (nSLEEP pin not set)");
//     return false;
//   }

//   ESP_LOGD(TAG, "handshake: === start ===");

//   // Snapshot initial pin states
//   bool init_nsleep = nsleep_pin_->digital_read();
//   bool init_nfault = nfault_pin_ ? nfault_pin_->digital_read() : true;
//   ESP_LOGD(TAG, "handshake: initial states: nSLEEP=%s nFAULT=%s",
//            init_nsleep ? "HIGH" : "LOW",
//            init_nfault ? "HIGH" : "LOW");

//   // 1) Force SLEEP (unambiguous)
//   ESP_LOGD(TAG, "handshake: step1 force SLEEP: nSLEEP -> LOW for %u ms", (unsigned) SLEEP_FORCE_MS);
//   nsleep_pin_->digital_write(false);
//   delay(SLEEP_FORCE_MS);

//   // 2) Wake
//   ESP_LOGD(TAG, "handshake: step2 wake: nSLEEP -> HIGH");
//   nsleep_pin_->digital_write(true);

//   // 3) Wait for nFAULT LOW (device ready for ACK), if available
//   if (nfault_pin_ != nullptr) {
//     ESP_LOGD(TAG, "handshake: step3 waiting for nFAULT LOW (ready), timeout=%u us", (unsigned) READY_WAIT_TIMEOUT_US);
//     uint32_t start = micros();
//     while ((micros() - start) < READY_WAIT_TIMEOUT_US) {
//       if (!nfault_pin_->digital_read()) {
//         saw_nfault_low_ = true;
//         t_wait_low_us_ = micros() - start;
//         ESP_LOGI(TAG, "handshake: nFAULT went LOW after %u us (ready)", (unsigned) t_wait_low_us_);
//         break;
//       }
//       delayMicroseconds(POLL_STEP_US);
//     }
//     if (!saw_nfault_low_) {
//       t_wait_low_us_ = micros() - start;
//       ESP_LOGW(TAG, "handshake: timeout waiting for nFAULT LOW after %u us; continuing anyway",
//                (unsigned) t_wait_low_us_);
//     }
//   } else {
//     // No nFAULT observed: use time-based wait (best-effort)
//     ESP_LOGW(TAG, "handshake: step3 no nFAULT pin; waiting 2ms before ACK (timing-based)");
//     delay(2);
//     saw_nfault_low_ = false;
//     t_wait_low_us_ = 2000;
//   }

//   // 4) ACK pulse (must be ~20–40us LOW, we aim ~30us)
//   ESP_LOGD(TAG, "handshake: step4 issuing ACK pulse on nSLEEP (LOW ~%u us)", (unsigned) ACK_PULSE_US);
//   this->pulse_nsleep_ack_();

//   // 5) Confirm nFAULT HIGH after ACK (device de-asserts nFAULT as ACK), if possible
//   if (nfault_pin_ != nullptr && saw_nfault_low_) {
//     ESP_LOGD(TAG, "handshake: step5 waiting for nFAULT HIGH after ACK, timeout=%u us", (unsigned) ACK_WAIT_TIMEOUT_US);
//     uint32_t start = micros();
//     while ((micros() - start) < ACK_WAIT_TIMEOUT_US) {
//       if (nfault_pin_->digital_read()) {
//         saw_nfault_high_after_ack_ = true;
//         t_wait_high_us_ = micros() - start;
//         ESP_LOGI(TAG, "handshake: nFAULT went HIGH after %u us (ACK complete)", (unsigned) t_wait_high_us_);
//         break;
//       }
//       delayMicroseconds(POLL_STEP_US);
//     }
//     if (!saw_nfault_high_after_ack_) {
//       t_wait_high_us_ = micros() - start;
//       ESP_LOGW(TAG, "handshake: timeout waiting for nFAULT HIGH after ACK (%u us)",
//                (unsigned) t_wait_high_us_);
//     }
//   } else if (nfault_pin_ != nullptr) {
//     // We never saw LOW; still log current state.
//     bool nf = nfault_pin_->digital_read();
//     ESP_LOGW(TAG, "handshake: step5 skipped confirm (didn't see nFAULT LOW). current nFAULT=%s",
//              nf ? "HIGH" : "LOW");
//   } else {
//     ESP_LOGD(TAG, "handshake: step5 no nFAULT pin; cannot confirm ACK");
//   }

//   // Final snapshot
//   bool end_nsleep = nsleep_pin_->digital_read();
//   bool end_nfault = nfault_pin_ ? nfault_pin_->digital_read() : true;
//   ESP_LOGD(TAG, "handshake: end states: nSLEEP=%s nFAULT=%s",
//            end_nsleep ? "HIGH" : "LOW",
//            end_nfault ? "HIGH" : "LOW");

//   // Always leave awake
//   nsleep_pin_->digital_write(true);
//   ESP_LOGD(TAG, "handshake: final: nSLEEP forced HIGH");

//   bool ok = true;
//   if (nfault_pin_ != nullptr) {
//     // If we can observe nFAULT, we consider it "ok" only if it behaves like the expected handshake
//     // (saw LOW then HIGH after ACK). If we timed out at either stage, mark not ok.
//     ok = saw_nfault_low_ && saw_nfault_high_after_ack_;
//   }

//   ESP_LOGD(TAG, "handshake: === end === ok=%s", ok ? "true" : "false");
//   return ok;
// }

void DRV8243Output::write_state(float state) {
  if (raw_output_ == nullptr) {
    ESP_LOGW(TAG, "write_state: raw_output is null");
    return;
  }

  ESP_LOGD(TAG, "write_state: requested=%.3f handshaked=%s ok=%s",
           state, handshaked_ ? "true" : "false", handshake_ok_ ? "true" : "false");

  // If something tries to drive before handshake ran (or if it failed), attempt one retry.
//   if (!handshaked_ || !handshake_ok_) {
//     ESP_LOGW(TAG, "write_state: handshake not done/ok; forcing output OFF and retrying handshake once");
//     raw_output_->set_level(0.0f);

//     // bool ok = this->do_handshake_();
//     handshaked_ = true;
//     handshake_ok_ = ok;

//     ESP_LOGI(TAG, "write_state: handshake retry complete. ok=%s", ok ? "true" : "false");
//   }

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

  ESP_LOGD(TAG, "write_state: mapped state=%.3f -> raw_level=%.3f (min=%.4f exp=%.2f)",
           x, y, min_level_, exponent_);
  raw_output_->set_level(y);
}

}  // namespace drv8243
}  // namespace esphome
