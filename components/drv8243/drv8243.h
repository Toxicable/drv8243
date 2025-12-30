#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace drv8243 {

class DRV8243Output : public Component, public output::FloatOutput {
 public:
  void set_nsleep_pin(GPIOPin *pin) { nsleep_pin_ = pin; }
  void set_nfault_pin(GPIOPin *pin) { nfault_pin_ = pin; }
  void set_direction_pin(GPIOPin *pin) { direction_pin_ = pin; }
  void set_direction_high(bool v) { direction_high_ = v; }

  void set_raw_output(output::FloatOutput *out) { raw_output_ = out; }

  void set_min_level(float v) { min_level_ = v; }
  void set_exponent(float e) { exponent_ = e; }

  float get_setup_priority() const override;  // run late so logs/wifi are up
  void setup() override;
  void dump_config() override;
  void write_state(float state) override;
  void run_handshake(const char *reason = "yaml");

 protected:
  bool do_handshake_(const char *reason);  // wake + ACK pulse; returns "observed-good" if nfault exists
  void pulse_nsleep_ack_();               // ~30us low pulse (jitter-minimized)

  GPIOPin *nsleep_pin_{nullptr};
  GPIOPin *nfault_pin_{nullptr};
  GPIOPin *direction_pin_{nullptr};
  output::FloatOutput *raw_output_{nullptr};

  float min_level_{0.014f};
  float exponent_{1.8f};
  bool direction_high_{true};

  // Debug/state
  bool handshaked_{false};
  bool handshake_ok_{false};
  bool handshake_in_progress_{false};

  bool saw_nfault_low_{false};
  bool saw_nfault_high_after_ack_{false};
  uint32_t t_wait_low_us_{0};
  uint32_t t_wait_high_us_{0};

  uint32_t handshake_attempts_{0};
};

}  // namespace drv8243
}  // namespace esphome
