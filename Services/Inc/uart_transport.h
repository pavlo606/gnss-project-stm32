/*
 * uart_transport.h
 *
 *  Created on: 21 жовт. 2026 р.
 *      Author: pavly
 */

#ifndef UART_TRANSPORT_H_
#define UART_TRANSPORT_H_


#include "stm32f1xx_hal.h"
#include <stdbool.h>

typedef struct
{
    uint32_t rx_bytes;
    uint32_t rx_overflows;
    uint32_t rx_dropped_bytes;
    uint32_t rx_loss_events;
    uint32_t tx_bytes;
    uint32_t tx_overflows;
    uint32_t tx_dma_errors;
    uint32_t uart_errors;
    uint32_t last_error;
} UartTransport_Stats;

void UartTransport_Init(
    UART_HandleTypeDef *pc_uart,
    UART_HandleTypeDef *modem_uart
);

uint16_t UartTransport_Read(
    UART_HandleTypeDef *huart,
    uint8_t *dst,
    uint16_t max_len
);

bool UartTransport_Send(
    UART_HandleTypeDef *huart,
    const uint8_t *data,
    uint16_t len
);

void UartTransport_Process(void);

bool UartTransport_TakeDataLoss(UART_HandleTypeDef *huart);

bool UartTransport_GetStats(UART_HandleTypeDef *huart, UartTransport_Stats *out);


#endif /* UART_TRANSPORT_H_ */
