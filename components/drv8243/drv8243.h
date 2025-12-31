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

  void setup() override;
  void dump_config() override;
  void write_state(float state) override;

 protected:
  enum class HandshakeResult : uint8_t {
    NOT_RUN = 0,
    VERIFIED_OK,
    VERIFIED_FAIL,
    UNVERIFIED,
  };

  HandshakeResult do_handshake_();

  const char *handshake_result_str_(HandshakeResult r) const;

  GPIOPin *nsleep_pin_{nullptr};
  GPIOPin *nfault_pin_{nullptr};
  GPIOPin *direction_pin_{nullptr};
  output::FloatOutput *raw_output_{nullptr};

  float min_level_{0.014f};
  float exponent_{1.8f};
  bool direction_high_{true};

  HandshakeResult handshake_result_{HandshakeResult::NOT_RUN};
  bool handshake_ran_{false};
};

}  // namespace drv8243
}  // namespace esphome
