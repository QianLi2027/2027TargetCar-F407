//
// Created by 20852 on 2026/7/8.
//

#ifndef F407_STANDARDTEMPLATE_CAN_DISPATCHER_H
#define F407_STANDARDTEMPLATE_CAN_DISPATCHER_H

#pragma once

#include "can_config.h"

class CanDispatcher
{
public:
    static void init();

    static void onRxFrame(const CanFrame& frame);
};


#endif //F407_STANDARDTEMPLATE_CAN_DISPATCHER_H
