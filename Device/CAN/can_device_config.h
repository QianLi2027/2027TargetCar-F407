#ifndef CAN_DEVICE_CONFIG_H
#define CAN_DEVICE_CONFIG_H

#pragma once

#include "../../Bsp/CAN/Inc/bsp_can.h"
#include "dji_motor.h"

// 本工程只驱动一个 GM6020（靶车主体），CAN ID = 2 → 反馈 0x206。
//   1~4 号是 DJI 电机的反馈 ID 段，这里只实例化实际存在的 1、2 号：
//   1 号占位（0x201，无此设备），2 号是真正的 6020。
extern DjiMotor dji_motor_1;
extern DjiMotor dji_motor_2;
extern DjiMotor dji_motor_3;
extern DjiMotor dji_motor_4;

extern DjiMotorGroup dji_group_1_to_4;

class CanDeviceConfig
{
public:
    static HAL_StatusTypeDef init();
};

#endif // CAN_DEVICE_CONFIG_H
