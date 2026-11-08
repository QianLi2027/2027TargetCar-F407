#ifndef MOTOR_MANAGER_H
#define MOTOR_MANAGER_H

#pragma once

#include "motor_common.h"

class MotorManager
{
public:
    static constexpr int MaxMotorNum = 24;

    static void clear();

    static bool addMotor(MotorBase* motor);

    static bool onCanRx(const CanFrame& frame);

    static int count();

private:
    static MotorBase* motors_[MaxMotorNum];
    static int motor_count_;
};

#endif // MOTOR_MANAGER_H
