//
// Created by 20852 on 2026/3/26.
//

#ifndef F407_STANDARDTEMPLATE_BSP_CAN_H
#define F407_STANDARDTEMPLATE_BSP_CAN_H


#pragma once

#include "can.h"
#include "../can_config.h"

class BspCan
{
public:
    static HAL_StatusTypeDef init();

    static HAL_StatusTypeDef send(const CanFrame& frame);

    static void setRxCallback(CanRxCallback callback);

    static void rxFifo0Callback(CAN_HandleTypeDef* hcan);

private:
    static HAL_StatusTypeDef filterConfig();

    static CAN_HandleTypeDef* getHandle(CanChannel channel);

    static CanRxCallback rx_callback_;
};

#endif //F407_STANDARDTEMPLATE_BSP_CAN_H