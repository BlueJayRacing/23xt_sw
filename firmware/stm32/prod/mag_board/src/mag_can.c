#include "mag_can.h"

static void put_u16_le(uint8_t *buf, uint16_t v)
{
    buf[0] = (uint8_t)v;
    buf[1] = (uint8_t)(v >> 8);
}

int mag_can_node_valid(uint32_t node)
{
    return node < MAG_CAN_NODE_COUNT;
}

uint32_t mag_can_field_id(uint32_t node)
{
    return MAG_CAN_ID_FIELD_BASE + node;
}

uint32_t mag_can_status_id(uint32_t node)
{
    return mag_can_node_valid(node) ? MAG_CAN_ID_STATUS_BASE + node : MAG_CAN_ID_STATUS_NO_NODE;
}

int16_t mag_can_clamp_i16(int32_t v)
{
    if (v > INT16_MAX)
    {
        return INT16_MAX;
    }
    if (v < INT16_MIN)
    {
        return INT16_MIN;
    }
    return (int16_t)v;
}

void mag_can_pack_field(const mag_field_msg_t *msg, uint8_t out[MAG_CAN_FIELD_DLC])
{
    put_u16_le(&out[0], (uint16_t)msg->bx);
    put_u16_le(&out[2], (uint16_t)msg->by);
    put_u16_le(&out[4], (uint16_t)msg->bz);
    out[6] = msg->counter;
    out[7] = msg->status;
}

void mag_can_pack_status(const mag_status_msg_t *msg, uint8_t out[MAG_CAN_STATUS_DLC])
{
    put_u16_le(&out[0], (uint16_t)msg->startup_fail_mask);
    put_u16_le(&out[2], (uint16_t)(msg->startup_fail_mask >> 16));
    put_u16_le(&out[4], msg->range_mt);
    out[6] = msg->spi_errors;
    out[7] = msg->missed_samples;
}
