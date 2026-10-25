/*
 * sim7000.h
 *
 *  Created on: 22 жовт. 2026 р.
 *      Author: pavly
 */

#ifndef SIM7000_H_
#define SIM7000_H_

// +CGNSINF: 1,1,20261024143910.000,49.501946,23.241543,302.200,0.00,116.2,1,,0.8,1.2,0.8,,24,4,7,,27,,


#include "stm32f1xx_hal.h"
#include <stdbool.h>

typedef enum {
    SIM7000_STATUS_IDLE = 0,
    SIM7000_STATUS_PENDING,
    SIM7000_STATUS_OK,
    SIM7000_STATUS_ERROR,
    SIM7000_STATUS_TIMEOUT,
    SIM7000_STATUS_BUSY,
    SIM7000_STATUS_INVALID_ARG,
    SIM7000_STATUS_OVERFLOW,
    SIM7000_STATUS_TX_ERROR
} Sim7000_Status;

typedef void (*Sim7000_UrcCallback)(const char *line);

void Sim7000_Init(UART_HandleTypeDef *modem);
void Sim7000_Process(void);

Sim7000_Status Sim7000_StartCommand(
    const char *command,
    uint32_t timeout_ms
);

Sim7000_Status Sim7000_GetStatus(void);
bool Sim7000_IsBusy(void);

const char *Sim7000_GetResponse(void);
size_t Sim7000_GetResponseLength(void);
const char *Sim7000_GetErrorLine(void);

void Sim7000_SetUrcCallback(Sim7000_UrcCallback callback);


#endif /* SIM7000_H_ */
