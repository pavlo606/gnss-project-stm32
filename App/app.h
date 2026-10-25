/*
 * app.h
 *
 *  Created on: 21 жовт. 2026 р.
 *      Author: pavly
 */

#ifndef APP_H_
#define APP_H_

#include "stm32f1xx_hal.h"

void App_Init(UART_HandleTypeDef *pc, UART_HandleTypeDef *modem, I2C_HandleTypeDef *lcd);
void App_Process(void);

#endif /* APP_H_ */
