//
// Created by 20852 on 2026/7/8.
//

#include "can_dispatcher.h"

#include "bsp_can.h"
#include "motor_manager.h"

void CanDispatcher::init()
{
    BspCan::setRxCallback(CanDispatcher::onRxFrame);
}

void CanDispatcher::onRxFrame(const CanFrame& frame)
{
    if (MotorManager::onCanRx(frame)) {
        return;
    }
    // 未识别的 CAN 帧可以在这里添加调试计数。
}
