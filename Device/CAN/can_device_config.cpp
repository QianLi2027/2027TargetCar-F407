#include "can_device_config.h"

#include "can_dispatcher.h"
#include "motor_manager.h"

// 本工程只使用 CAN1（CAN2 已从 CubeMX 配置中移除）。

DjiMotor dji_motor_1(CanChannel::CAN_1, 0x201);
DjiMotor dji_motor_2(CanChannel::CAN_1, 0x206);
DjiMotor dji_motor_3(CanChannel::CAN_1, 0x203);
DjiMotor dji_motor_4(CanChannel::CAN_1, 0x204);


DjiMotorGroup dji_group_1_to_4(
    CanChannel::CAN_1,
    DjiMotorGroup::TxId_1_To_4,
    &dji_motor_1,
    &dji_motor_2,
    &dji_motor_3,
    &dji_motor_4
);



HAL_StatusTypeDef CanDeviceConfig::init()
{
    MotorManager::clear();

    MotorManager::addMotor(&dji_motor_1);
    MotorManager::addMotor(&dji_motor_2);
    MotorManager::addMotor(&dji_motor_3);
    MotorManager::addMotor(&dji_motor_4);

    CanDispatcher::init();

    return BspCan::init();
}
