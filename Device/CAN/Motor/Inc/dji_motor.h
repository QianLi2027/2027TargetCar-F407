#ifndef DJI_MOTOR_H
#define DJI_MOTOR_H

#pragma once

#include "motor_common.h"
#include "bsp_can.h"

class DjiMotor : public MotorBase
{
public:
    DjiMotor(CanChannel channel, uint32_t feedback_id)
        : MotorBase(channel, feedback_id)
    {
    }

    void decode(const CanFrame& frame) override;

    void setCurrent(int16_t current)
    {
        current_cmd_ = current;
    }

    int16_t currentCommand() const
    {
        return current_cmd_;
    }

private:
    void updateRoundCount(uint16_t new_angle);

private:
    int16_t current_cmd_ = 0;

    uint16_t last_raw_angle_ = 0;
    bool angle_inited_ = false;
};

class DjiMotorGroup
{
public:
    static constexpr uint32_t TxId_1_To_4 = 0x1FE;
    static constexpr uint32_t TxId_5_To_8 = 0x1FE;
    static constexpr uint32_t TxId_9_To_12 = 0x2FF;

    DjiMotorGroup(CanChannel channel,
                  uint32_t tx_id,
                  DjiMotor* m1,
                  DjiMotor* m2,
                  DjiMotor* m3,
                  DjiMotor* m4);

    HAL_StatusTypeDef sendCurrent();

private:
    CanChannel channel_;
    uint32_t tx_id_ = 0;
    DjiMotor* motors_[4] = {nullptr};
};

#endif // DJI_MOTOR_H
