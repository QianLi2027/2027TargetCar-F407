//
// Created by 20852 on 2026/7/8.
//

#ifndef F407_STANDARDTEMPLATE_CAN_CONFIG_H
#define F407_STANDARDTEMPLATE_CAN_CONFIG_H

#pragma once

#include <cstdint>

enum class CanChannel
{
    CAN_1,
    CAN_2
};

struct CanFrame
{
    CanChannel channel;
    uint32_t std_id;
    uint8_t data[8];
    uint8_t dlc = 8;
};

using CanRxCallback = void (*)(const CanFrame& frame);

#endif //F407_STANDARDTEMPLATE_CAN_CONFIG_H
