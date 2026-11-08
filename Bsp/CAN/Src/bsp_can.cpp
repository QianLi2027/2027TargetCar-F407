//
// Created by 20852 on 2026/3/26.
//

#include "../Inc/bsp_can.h"
#include <cstring>

CanRxCallback BspCan::rx_callback_ = nullptr;

HAL_StatusTypeDef BspCan::init()
{
    HAL_StatusTypeDef status = filterConfig();
    if (status != HAL_OK) {
        return status;
    }

    status = HAL_CAN_Start(&hcan1);
    if (status != HAL_OK) {
        return status;
    }

    status = HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
    if (status != HAL_OK) {
        return status;
    }

    return HAL_OK;
}

HAL_StatusTypeDef BspCan::filterConfig()
{
    CAN_FilterTypeDef filter{};

    filter.FilterActivation = ENABLE;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;

    filter.FilterIdHigh = 0x0000;
    filter.FilterIdLow = 0x0000;
    filter.FilterMaskIdHigh = 0x0000;
    filter.FilterMaskIdLow = 0x0000;

    // 只使用 CAN1，过滤器组 0
    filter.FilterBank = 0;
    filter.SlaveStartFilterBank = 14;

    HAL_StatusTypeDef status = HAL_CAN_ConfigFilter(&hcan1, &filter);
    if (status != HAL_OK) {
        return status;
    }

    return HAL_OK;
}

HAL_StatusTypeDef BspCan::send(const CanFrame& frame)
{
    if (frame.dlc > 8) {
        return HAL_ERROR;
    }

    CAN_HandleTypeDef* hcan = getHandle(frame.channel);
    if (hcan == nullptr) {
        return HAL_ERROR;
    }

    CAN_TxHeaderTypeDef tx_header{};
    uint32_t tx_mailbox = 0;
    uint8_t tx_data[8] = {0};

    tx_header.StdId = frame.std_id;
    tx_header.IDE = CAN_ID_STD;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = frame.dlc;

    std::memcpy(tx_data, frame.data, frame.dlc);

    return HAL_CAN_AddTxMessage(hcan, &tx_header, tx_data, &tx_mailbox);
}

void BspCan::setRxCallback(CanRxCallback callback)
{
    rx_callback_ = callback;
}

CAN_HandleTypeDef* BspCan::getHandle(CanChannel channel)
{
    switch (channel) {
        case CanChannel::CAN_1:
            return &hcan1;

        default:
            return nullptr;
    }
}

void BspCan::rxFifo0Callback(CAN_HandleTypeDef* hcan)
{
    if (hcan == nullptr || hcan->Instance != CAN1) {
        return;
    }

    CAN_RxHeaderTypeDef rx_header{};
    uint8_t rx_data[8] = {0};

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK) {
        return;
    }

    CanFrame frame{};
    frame.channel = CanChannel::CAN_1;

    frame.std_id = rx_header.StdId;
    frame.dlc = rx_header.DLC > 8 ? 8 : rx_header.DLC;

    std::memcpy(frame.data, rx_data, frame.dlc);

    if (rx_callback_ != nullptr) {
        rx_callback_(frame);
    }
}

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan)
{
    BspCan::rxFifo0Callback(hcan);
}
