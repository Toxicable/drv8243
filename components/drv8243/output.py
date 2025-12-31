import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output, binary_sensor
from esphome.const import CONF_ID
from esphome import pins

from . import DRV8243Output

CONF_OUT1 = "out1"
CONF_NSLEEP_PIN = "nsleep_pin"
CONF_NFAULT_PIN = "nfault_pin"
CONF_OUT2_PIN = "out2_pin"
CONF_FLIP_POLARITY = "flip_polarity"
CONF_FAULT_SENSOR = "fault_sensor"
CONF_MIN_LEVEL = "min_level"
CONF_EXPONENT = "exponent"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DRV8243Output),

        cv.Required(CONF_OUT1): cv.use_id(output.FloatOutput),

        cv.Required(CONF_NSLEEP_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_NFAULT_PIN): pins.gpio_input_pin_schema,

        cv.Optional(CONF_OUT2_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_FLIP_POLARITY, default=False): cv.boolean,

        # Optional: expose nFAULT as a binary_sensor ("problem" true when fault is active)
        cv.Optional(CONF_FAULT_SENSOR): binary_sensor.binary_sensor_schema(
            device_class="problem"
        ),

        cv.Optional(CONF_MIN_LEVEL, default=0.014): cv.percentage,
        cv.Optional(CONF_EXPONENT, default=1.8): cv.float_range(min=0.1, max=5.0),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    out1 = await cg.get_variable(config[CONF_OUT1])
    cg.add(var.set_out1_output(out1))

    nsleep = await cg.gpio_pin_expression(config[CONF_NSLEEP_PIN])
    cg.add(var.set_nsleep_pin(nsleep))

    if CONF_NFAULT_PIN in config:
        nfault = await cg.gpio_pin_expression(config[CONF_NFAULT_PIN])
        cg.add(var.set_nfault_pin(nfault))

    if CONF_OUT2_PIN in config:
        out2 = await cg.gpio_pin_expression(config[CONF_OUT2_PIN])
        cg.add(var.set_out2_pin(out2))

    cg.add(var.set_flip_polarity(config[CONF_FLIP_POLARITY]))

    if CONF_FAULT_SENSOR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_FAULT_SENSOR])
        cg.add(var.set_fault_sensor(sens))

    cg.add(var.set_min_level(config[CONF_MIN_LEVEL]))
    cg.add(var.set_exponent(config[CONF_EXPONENT]))
