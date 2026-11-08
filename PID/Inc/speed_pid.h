//
// Created by Lenovo on 2025/12/21.
//

#ifndef F407_SPEED_PID_H
#define F407_SPEED_PID_H

#pragma once
#include "PIDBase.h"

#ifdef __cplusplus
class SpeedPID : public PidBase {
public:
    using PidBase::PidBase;
};
#endif

#ifdef __cplusplus
extern "C" {
#endif
    void Speed_PID_Clear();
    int16_t  speed_pid_calculate(float target, float current, float dt);
#ifdef __cplusplus
}
#endif

#endif //F407_SPEED_PID_H