#include "motor_manager.h"

MotorBase* MotorManager::motors_[MotorManager::MaxMotorNum] = {nullptr};
int MotorManager::motor_count_ = 0;

void MotorManager::clear()
{
    for (int i = 0; i < MaxMotorNum; ++i) {
        motors_[i] = nullptr;
    }

    motor_count_ = 0;
}

bool MotorManager::addMotor(MotorBase* motor)
{
    if (motor == nullptr) {
        return false;
    }

    if (motor_count_ >= MaxMotorNum) {
        return false;
    }

    motors_[motor_count_++] = motor;
    return true;
}

bool MotorManager::onCanRx(const CanFrame& frame)
{
    for (int i = 0; i < motor_count_; ++i) {
        MotorBase* motor = motors_[i];

        if (motor != nullptr && motor->match(frame)) {
            motor->decode(frame);
            return true;
        }
    }

    return false;
}

int MotorManager::count()
{
    return motor_count_;
}
