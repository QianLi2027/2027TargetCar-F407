#ifndef CAN_APP_H
#define CAN_APP_H

#pragma once

#include "can.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 这一层是专门给 main.c / C 文件调用的 C 接口。
// C 文件不要直接 include dji_motor.h、motor_manager.h、can_device_config.h 这些 C++ 头文件。
//
// 本工程只驱动 DJI 电机（GM6020）。DM / LK / 上下半板 / IMU 的接口原本也在这里，
// 但它们在这个工程里既没有实现、也没有任何调用方，已经删除。

HAL_StatusTypeDef CAN_Device_Init(void);

// DJI Motor，motor_index 从 1 开始编号，对应 dji_motor_1、dji_motor_2...
void CAN_DjiMotor_SetCurrent(uint8_t motor_index, int16_t current);
HAL_StatusTypeDef CAN_DjiGroup_SendCurrent(uint8_t group_index);

float CAN_DjiMotor_GetPosition(uint8_t motor_index);
float CAN_DjiMotor_GetVelocity(uint8_t motor_index);
float CAN_DjiMotor_GetCurrent(uint8_t motor_index);
float CAN_DjiMotor_GetTemperature(uint8_t motor_index);
int32_t CAN_DjiMotor_GetTotalAngle(uint8_t motor_index);
// 电机是否在线（有没有 CAN 反馈）。时间戳由本层自己取，调用方不用传。
bool CAN_DjiMotor_IsOnline(uint8_t motor_index);

#ifdef __cplusplus
}
#endif

#endif // CAN_APP_H
