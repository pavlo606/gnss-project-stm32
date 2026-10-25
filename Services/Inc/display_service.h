/*
 * display_service.h
 *
 *  Created on: 25 жовт. 2026 р.
 *      Author: pavly
 */

#ifndef INC_DISPLAY_SERVICE_H_
#define INC_DISPLAY_SERVICE_H_


#include "stm32f1xx_hal.h"
#include <stdbool.h>

typedef enum
{
    DISPLAY_IDLE,
    DISPLAY_BUSY,
    DISPLAY_SUCCESS,
    DISPLAY_NO_FIX,
    DISPLAY_ERROR
} DisplayState;

typedef enum
{
    DISPLAY_ERR_NONE,
    DISPLAY_ERR_CRC,
    DISPLAY_ERR_AT_TIMEOUT,
    DISPLAY_ERR_AT_ERROR,
	DISPLAY_ERR_AT_PARSE,
	DISPLAY_ERR_INVALID_ARG,
    DISPLAY_ERR_UART,
    DISPLAY_ERR_GNSS_OFF,
    DISPLAY_ERR_INTERNAL
} DisplayError;

typedef struct
{
    float latitude;
    float longitude;
    uint8_t satellites;
    float hdop;
    bool valid;
} DisplayGnssData;

void DisplayService_Init(void);
void DisplayService_Process(void);

void DisplayService_SetIdle(void);
void DisplayService_RequestStarted(void);

void DisplayService_RequestSucceeded(float latitude, float longitude, bool valid);

void DisplayService_NoFix(uint8_t satellites, float hdop);

void DisplayService_SetError(DisplayError error);

#endif /* INC_DISPLAY_SERVICE_H_ */
