/*
 * lcd.h
 *
 *  Created on: 25 жовт. 2026 р.
 *      Author: pavly
 */

#ifndef INC_LCD_I2C_H_
#define INC_LCD_I2C_H_


#include "stm32f1xx_hal.h"
#include <stdbool.h>

void LCD_InitStart(I2C_HandleTypeDef *hi2c);
void LCD_Process(void);
void LCD_SetText(uint8_t row, const char *text);
bool LCD_IsReady(void);


#endif /* INC_LCD_I2C_H_ */
