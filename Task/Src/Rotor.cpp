//
// ============================================================================
//  靶车主体（GM6020）控制任务
// ============================================================================
//
//  【控制链】只有四步，没有别的开关/阈值：
//
//    目标 → 斜坡 → [前馈 + PID] → 限幅 → 电流
//
//    前馈 = accel_ff × 斜坡斜率 + fric_ff × 方向 + visc_ff × 目标
//    PID  = 修正"前馈没算准"的残差
//
//  【参数位置】（2026-11-07 删掉了在线调参，现在写死）
//    要改就改下面那组常量，然后重新烧录固件。
//    （带调参功能的旧版本备份在 Desktop\ESP-DEMO\backup_tuning_20261107\）
//
//    kp / ki / kd   速度环 PID
//    ramp_ms        0 -> 180rpm 的斜坡时间；0 = 关闭斜坡
//    accel_ff       加速度前馈：电流 / (rpm/s)
//    fric_ff        库仑摩擦前馈：电流（方向取目标方向）
//    visc_ff        粘滞摩擦前馈：电流 / rpm
//
// ============================================================================

#include "Rotor.h"
#include "bsp_can.h"
#include "can_app.h"
#include "EspLink.h"

#include <cmath>
#include "speed_pid.h"
#include "magictools.h"   // clamp()

namespace {

    constexpr float   kGearRpm[5] = {0.0f, 25.0f, 45.0f, 90.0f, 120.0f};   //转速挡位
    constexpr uint8_t kGearMax    = 4;          // 最高档编号


    constexpr float kFullScaleRpm = 180.0f;
    constexpr uint32_t kLinkTimeoutMs = 800;   //掉线信号保留，防止网络波动带来控制不稳定
    constexpr float Rotor_Dt = 0.001f;
    constexpr float kCurrentLimit = 16000.0f;
}

static SpeedPID rotor_speed_pid(20.0f, 0.5f, 0.0f, kCurrentLimit, 3000);

// ── 斜坡 / 前馈参数
constexpr float s_ramp_ms  = 1000.0f;   // 0 -> 180rpm 用 1000ms
constexpr float s_accel_ff = 60.0f;     // 加速度前馈
constexpr float s_fric_ff  = 25.0f;     // 摩擦前馈
constexpr float s_visc_ff  = 20.0f;     // 摩擦前馈

float kDirection = 0;

float target = 0.0f;

void RotorTask::run() {
    rotor_speed_pid.Clear();

    // 斜坡后的实际目标
    float ref = 0.0f;

    for (;;) {
        //网页获取命令
        EspLinkCommand cmd{};
        EspLink_GetCommand(&cmd);

        const uint32_t now = HAL_GetTick();
        const bool link_ok = cmd.valid && ((now - cmd.last_rx_tick) < kLinkTimeoutMs);
        const bool motor_ok = CAN_DjiMotor_IsOnline(2);

        target = 0.0f;

        if (motor_ok && link_ok && cmd.gear >= 1 && cmd.gear <= kGearMax) {

            if (cmd.dir == ESLINK_DIR_FORWARD) {
                kDirection = 1.0f;
            } else if (cmd.dir == ESLINK_DIR_REVERSE) {
                kDirection = -1.0f;
            } else {
                kDirection = 0.0f;          // STOP / 未知方向
            }

            target = kGearRpm[cmd.gear] * kDirection;
        }

        float ramp_rate = 0.0f;

        const float max_rate = kFullScaleRpm / (s_ramp_ms * 0.001f);  // rpm/s
        const float step     = max_rate * Rotor_Dt;

        if (target > ref + step) {
            ref += step;
            ramp_rate = max_rate;
        } else if (target < ref - step) {
            ref -= step;
            ramp_rate = -max_rate;
        } else {
            ref = target;
            ramp_rate = 0.0f;
        }//限制电流变化速度，避免一下拉高伤害电机

        //清pid条件判断
        static bool was_link_ok = false;
        const bool linked = (link_ok != was_link_ok);
        was_link_ok = link_ok;
        //是否还在线

        static float last_target = 0.0f;
        const bool target_changed = (std::fabs(target - last_target) > 1.0f);
        last_target = target;
        //挡位是否改变

        const float speed_raw = CAN_DjiMotor_GetVelocity(2);

        const bool fully_stopped = (ref == 0.0f && std::fabs(speed_raw) < 2.0f);
        //是否停下来了

        if (target_changed || linked || fully_stopped) {
            rotor_speed_pid.Clear();
        }

        //控制部分
        static float s_speed_f = 0.0f;
        s_speed_f += (speed_raw - s_speed_f) * 0.05f;   // 1ms × 5% ≈ 20ms 时间常数
        const float speed = s_speed_f;

        const float dir_sign = (ref > 0.1f) ? 1.0f : ((ref < -0.1f) ? -1.0f : 0.0f);

        // 摩擦前馈
        const bool need_friction_ff = (std::fabs(speed) <= std::fabs(ref) + 2.0f);
        const float visc_term = need_friction_ff ? (s_visc_ff * ref) : 0.0f;

        const bool  stop_braking = (ref == 0.0f && std::fabs(speed_raw) > 3.0f);
        const float brake_term   = stop_braking ? (speed_raw > 0.0f ? -1.0f : 1.0f) * s_accel_ff * kFullScaleRpm: 0.0f;

        //加快启动
        const bool  catching_up  = (ref != 0.0f && !stop_braking && std::fabs(speed_raw) < std::fabs(ref) - 5.0f);
        const float catch_term   = catching_up ? (ref > 0.0f ? 1.0f : -1.0f) * s_accel_ff * kFullScaleRpm: 0.0f;

        const float ff_target = s_accel_ff * ramp_rate   // 加速度前馈
                              + s_fric_ff  * dir_sign    // 摩擦前馈
                              + visc_term                // 转动阻力前馈
                              + brake_term               // 停止刹车
                              + catch_term;              // 起步/追赶

        //平滑处理
        static float s_ff_s = 0.0f;
        s_ff_s += (ff_target - s_ff_s) * 0.03f;          // 1ms × 3% ≈ 30ms 时间常数
        const float ff = s_ff_s;

        float out = ff + rotor_speed_pid.Calculate(ref, speed, Rotor_Dt);

        //电流限幅
        constexpr float kSlewPerMs = 400.0f;
        static float s_out_prev = 0.0f;
        out = clamp(out, s_out_prev - kSlewPerMs, s_out_prev + kSlewPerMs);

        out = clamp(out, -kCurrentLimit, kCurrentLimit);

        s_out_prev = out;

        const int16_t current = static_cast<int16_t>(out);

        CAN_DjiMotor_SetCurrent(2, current);
        CAN_DjiGroup_SendCurrent(1);

        osDelay(1);
    }
}

extern "C" {
    static RotorTask rotor_task;

    void RotorTask_Init() {
        rotor_task.start((char*)"RotorTask", 512, osPriorityAboveNormal);
    }
}
