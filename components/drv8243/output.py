import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID
from esphome import pins

from . import DRV8243Output  # from __init__.py

CONF_RAW_OUTPUT = "raw_output"
CONF_NSLEEP_PIN = "nsleep_pin"
CONF_NFAULT_PIN = "nfault_pin"
CONF_DIRECTION_PIN = "direction_pin"
CONF_DIRECTION_HIGH = "direction_high"
CONF_MIN_LEVEL = "min_level"
CONF_EXPONENT = "exponent"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DRV8243Output),

        cv.Required(CONF_RAW_OUTPUT): cv.use_id(output.FloatOutput),

        cv.Required(CONF_NSLEEP_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_NFAULT_PIN): pins.gpio_input_pin_schema,
        cv.Optional(CONF_DIRECTION_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_DIRECTION_HIGH, default=True): cv.boolean,

        cv.Optional(CONF_MIN_LEVEL, default=0.014): cv.percentage,
        cv.Optional(CONF_EXPONENT, default=1.8): cv.float_range(min=0.1, max=5.0),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    raw = await cg.get_variable(config[CONF_RAW_OUTPUT])
    cg.add(var.set_raw_output(raw))

    nsleep = await cg.gpio_pin_expression(config[CONF_NSLEEP_PIN])
    cg.add(var.set_nsleep_pin(nsleep))

    if CONF_NFAULT_PIN in config:
        nfault = await cg.gpio_pin_expression(config[CONF_NFAULT_PIN])
        cg.add(var.set_nfault_pin(nfault))

    if CONF_DIRECTION_PIN in config:
        direction = await cg.gpio_pin_expression(config[CONF_DIRECTION_PIN])
        cg.add(var.set_direction_pin(direction))
        cg.add(var.set_direction_high(config[CONF_DIRECTION_HIGH]))

    cg.add(var.set_min_level(config[CONF_MIN_LEVEL]))
    cg.add(var.set_exponent(config[CONF_EXPONENT]))
