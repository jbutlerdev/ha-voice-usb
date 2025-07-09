import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

# No dependencies needed - uses USB Serial/JTAG directly
DEPENDENCIES = []

usb_communication_ns = cg.esphome_ns.namespace("usb_communication")
USBCommunicationComponent = usb_communication_ns.class_(
    "USBCommunicationComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(USBCommunicationComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)