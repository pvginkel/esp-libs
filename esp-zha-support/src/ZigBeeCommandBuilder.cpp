#include "ZigBeeCommandBuilder.h"

static constexpr size_t MAX_FRAME_DATA_SIZE = 110;

uint8_t* ZigBeeCommandBuilder::_buffer = new uint8_t[MAX_FRAME_DATA_SIZE];

void ZigBeeCommandBuilder::send() {
    ZigBeeLock lock;

    esp_zb_zcl_custom_cluster_cmd_req(&cmd_req);
}

void ZigBeeCommandBuilder::send(std::function<void(ZigBeeStream&)> buffer_writer) {
    ZigBeeStream stream(_buffer, MAX_FRAME_DATA_SIZE);

    buffer_writer(stream);

    cmd_req.data.type = ESP_ZB_ZCL_ATTR_TYPE_SET;
    cmd_req.data.size = stream.getPosition();
    cmd_req.data.value = stream.getData();

    send();
}
