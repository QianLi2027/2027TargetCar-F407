#ifndef MOTOR_COMMON_H
#define MOTOR_COMMON_H

#pragma once

#include "can_config.h"
#include <cstdint>

struct MotorFeedback
{
    // 统一视角下的反馈值（本工程只驱动 DJI 电机）。
    //   position   : 机械角度
    //   velocity   : 转速 rpm
    //   current    : 实际电流（原始值）
    //   temperature: 电机温度 ℃
    //   round_count / total_angle: 编码器累计圈数 / 累计计数（跨圈不归零）
    float position = 0.0f;
    float velocity = 0.0f;
    float current = 0.0f;
    float temperature = 0.0f;

    int32_t round_count = 0;
    int32_t total_angle = 0;
};

class MotorBase
{
public:
    MotorBase(CanChannel channel, uint32_t feedback_id)
        : channel_(channel), feedback_id_(feedback_id)
    {
    }

    virtual ~MotorBase() = default;

    virtual bool match(const CanFrame& frame) const
    {
        return frame.channel == channel_ && frame.std_id == feedback_id_;
    }

    virtual void decode(const CanFrame& frame) = 0;

    bool isOnline(uint32_t now_ms) const
    {
        return now_ms - last_update_time_ < offline_timeout_ms_;
    }

    void setOfflineTimeout(uint32_t timeout_ms)
    {
        offline_timeout_ms_ = timeout_ms;
    }

    const MotorFeedback& feedback() const
    {
        return feedback_;
    }

    uint32_t feedbackId() const
    {
        return feedback_id_;
    }

    CanChannel channel() const
    {
        return channel_;
    }

protected:
    void markUpdated(uint32_t now_ms)
    {
        last_update_time_ = now_ms;
    }

protected:
    CanChannel channel_;
    uint32_t feedback_id_ = 0;

    uint32_t last_update_time_ = 0;
    uint32_t offline_timeout_ms_ = 100;

    MotorFeedback feedback_{};
};

#endif // MOTOR_COMMON_H
