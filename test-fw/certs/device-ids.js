let UUID = require('uuid-1345');

module.exports = {
    "manufacturer-uuid": UUID.v5({
        namespace: UUID.namespace.url,
        name: "arm.com"
    }),
    "device-class-uuid": UUID.v5({
        namespace: UUID.namespace.url,
        name: "awesome-lora-sensor"
    })
};
