#pragma once

#include "esphome.h"

namespace esphome {
namespace drv8243 {

class DRV8243Output : public Component, public output::FloatOutput {
 public:
  void set_nsleep_pin(GPIOPin *pin) { nsleep_pin_ = pin; }
  void set_nfault_pin(GPIOPin *pin) { nfault_pin_ = pin; }
  void set_raw_output(output::FloatOutput *out) { raw_output_ = out; }

  void set_min_level(float v) { min_level_ = v; }
  void set_exponent(float e) { exponent_ = e; }

  void setup() override;
  void write_state(float state) override;

 protected:
  GPIOPin *nsleep_pin_{nullptr};
  GPIOPin *nfault_pin_{nullptr};
  output::FloatOutput *raw_output_{nullptr};

  float min_level_{0.014f};  // PWM floor (0..1) – tuned to ~4.6% OUT1 duty
  float exponent_{1.8f};     // perceptual shaping

  // Shared between instances so we only do the handshake once
  static bool global_initialized_;
};

} 
}