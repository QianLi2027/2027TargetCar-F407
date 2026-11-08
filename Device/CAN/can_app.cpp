#include "can_app.h"

#include "can_device_config.h"

namespace
{
    DjiMotor* GetDjiMotor(uint8_t motor_index)
    {
        switch (motor_index) {
            case 1: return &dji_motor_1;
            case 2: return &dji_motor_2;
            case 3: return &dji_motor_3;
            case 4: return &dji_motor_4;
            default: return nullptr;
        }
    }

    DjiMotorGroup* GetDjiGroup(uint8_t group_index)
    {
        switch (group_index) {
            case 1: return &dji_group_1_to_4;
            default: return nullptr;
        }
    }
}

extern "C" {

HAL_StatusTypeDef CAN_Device_Init(void)
{
    return CanDeviceConfig::init();
}

void CAN_DjiMotor_SetCurrent(uint8_t motor_index, int16_t current)
{
    DjiMotor* motor = GetDjiMotor(motor_index);

    if (motor != nullptr) {
        motor->setCurrent(current);
    }
}

HAL_StatusTypeDef CAN_DjiGroup_SendCurrent(uint8_t group_index)
{
    DjiMotorGroup* group = GetDjiGroup(group_index);

    if (group == nullptr) {
        return HAL_ERROR;
    }

    return group->sendCurrent();
}

float CAN_DjiMotor_GetPosition(uint8_t motor_index)
{
    DjiMotor* motor = GetDjiMotor(motor_index);

    if (motor == nullptr) {
        return 0.0f;
    }

    return motor->feedback().position;
}

float CAN_DjiMotor_GetVelocity(uint8_t motor_index)
{
    DjiMotor* motor = GetDjiMotor(motor_index);

    if (motor == nullptr) {
        return 0.0f;
    }

    return motor->feedback().velocity;
}

float CAN_DjiMotor_GetCurrent(uint8_t motor_index)
{
    DjiMotor* motor = GetDjiMotor(motor_index);

    if (motor == nullptr) {
        return 0.0f;
    }

    return motor->feedback().current;
}

float CAN_DjiMotor_GetTemperature(uint8_t motor_index)
{
    DjiMotor* motor = GetDjiMotor(motor_index);

    if (motor == nullptr) {
        return 0.0f;
    }

    return motor->feedback().temperature;
}

int32_t CAN_DjiMotor_GetTotalAngle(uint8_t motor_index)
{
    DjiMotor* motor = GetDjiMotor(motor_index);

    if (motor == nullptr) {
        return 0;
    }

    return motor->feedback().total_angle;
}

bool CAN_DjiMotor_IsOnline(uint8_t motor_index)
{
    DjiMotor* motor = GetDjiMotor(motor_index);

    if (motor == nullptr) {
        return false;
    }

    // 时间戳在这一层取，调用方直接 if (CAN_DjiMotor_IsOnline(2)) 就行
    return motor->isOnline(HAL_GetTick());
}
} // extern "C"
