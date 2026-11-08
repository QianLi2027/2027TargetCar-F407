#include "dji_motor.h"
#include "can_byte_utils.h"
#include "main.h"

void DjiMotor::decode(const CanFrame& frame)
{
    if (!match(frame) || frame.dlc < 7) {
        return;
    }

    const uint8_t* data = frame.data;

    const uint16_t raw_angle = CanReadUint16BE(&data[0]);
    const int16_t speed_rpm = CanReadInt16BE(&data[2]);
    const int16_t torque_current = CanReadInt16BE(&data[4]);
    const uint8_t temperature = data[6];

    feedback_.velocity = static_cast<float>(speed_rpm);
    feedback_.current = static_cast<float>(torque_current);
    feedback_.temperature = static_cast<float>(temperature);


    updateRoundCount(raw_angle);

    markUpdated(HAL_GetTick());
}

void DjiMotor::updateRoundCount(uint16_t new_angle)
{
    if (!angle_inited_) {
        last_raw_angle_ = new_angle;
        feedback_.round_count = 0;
        feedback_.total_angle = static_cast<int32_t>(new_angle);
        feedback_.position = static_cast<float>(feedback_.total_angle);


        angle_inited_ = true;
        return;
    }

    const int32_t diff = static_cast<int32_t>(new_angle)
                       - static_cast<int32_t>(last_raw_angle_);

    if (diff > 4096) {
        feedback_.round_count--;
    } else if (diff < -4096) {
        feedback_.round_count++;
    }

    feedback_.total_angle = feedback_.round_count * 8192 + static_cast<int32_t>(new_angle);
    feedback_.position = static_cast<float>(feedback_.total_angle);


    last_raw_angle_ = new_angle;
}

DjiMotorGroup::DjiMotorGroup(CanChannel channel,
                             uint32_t tx_id,
                             DjiMotor* m1,
                             DjiMotor* m2,
                             DjiMotor* m3,
                             DjiMotor* m4)
    : channel_(channel), tx_id_(tx_id)
{
    motors_[0] = m1;
    motors_[1] = m2;
    motors_[2] = m3;
    motors_[3] = m4;
}

HAL_StatusTypeDef DjiMotorGroup::sendCurrent()
{
    CanFrame frame{};
    frame.channel = channel_;
    frame.std_id = tx_id_;
    frame.dlc = 8;

    for (int i = 0; i < 4; ++i) {
        const int16_t current = (motors_[i] != nullptr) ? motors_[i]->currentCommand() : 0;
        CanWriteInt16BE(&frame.data[i * 2], current);
    }

    return BspCan::send(frame);
}
