//
// Created by Lenovo on 2026/10/6.
//

#ifndef TARGET_ROTOR_H
#define TARGET_ROTOR_H

#pragma once
#include "TaskBase.h"
#ifdef __cplusplus

class RotorTask: public TaskBase{
public:
    void run() override;

};

#endif

#ifdef __cplusplus
extern "C" {
#endif

    void RotorTask_Init();

#ifdef __cplusplus
}
#endif

#endif //TARGET_ROTOR-H