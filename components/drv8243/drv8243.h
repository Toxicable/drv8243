#pragma once

#include "esphome/core/component.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/gpio.h"

namespace esphome {
namespace drv8243 {

class DRV8243Output : public Component, public output::FloatOutput {
 public:
  void set_nsleep_pin(GPIOPin *pin) { nsleep_pin_ = pin; }
  void set_nfault_pin(GPIOPin *pin) { nfault_pin_ = pin; }

  void set_out2_pin(GPIOPin *pin) { out2_pin_ = pin; }
  void set_flip_polarity(bool v) { flip_polarity_ = v; }

  void set_out1_output(output::FloatOutput *out) { out1_output_ = out; }

  void set_fault_sensor(binary_sensor::BinarySensor *s) { fault_sensor_ = s; }

  void set_min_level(float v) { min_level_ = v; }
  void set_exponent(float e) { exponent_ = e; }

  void setup() override;
  void dump_config() override;
  void loop() override;
  void write_state(float state) override;

 protected:
  enum class HandshakeResult : uint8_t { NOT_RUN = 0, VERIFIED_OK, VERIFIED_FAIL, UNVERIFIED };

  HandshakeResult do_handshake_();
  const char *handshake_result_str_(HandshakeResult r) const;

  GPIOPin *nsleep_pin_{nullptr};
  GPIOPin *nfault_pin_{nullptr};
  GPIOPin *out2_pin_{nullptr};  // DRV OUT2 / PH mapping

  output::FloatOutput *out1_output_{nullptr};  // DRV OUT1 PWM mapping
  binary_sensor::BinarySensor *fault_sensor_{nullptr};

  float min_level_{0.014f};
  float exponent_{1.8f};
  bool flip_polarity_{false};

  // State
  bool handshake_ran_{false};
  HandshakeResult handshake_result_{HandshakeResult::NOT_RUN};

  // fault reporting
  bool last_fault_{false};
  uint32_t last_fault_check_ms_{0};
};

}  // namespace drv8243
}  // namespace esphome
