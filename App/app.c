/*
 * app.c
 *
 *  Created on: 21 жовт. 2026 р.
 *      Author: pavly
 */

#include "app.h"
#include "uart_transport.h"
#include "protocol.h"
#include "sim7000.h"
#include "command_handler.h"
#include "module_service.h"
#include "lcd_i2c.h"
#include "display_service.h"

static UART_HandleTypeDef *pc_uart;
static UART_HandleTypeDef *modem_uart;
static I2C_HandleTypeDef *lcd_i2c;

uint8_t rx_buffer[128];


void App_Init(UART_HandleTypeDef *pc, UART_HandleTypeDef *modem, I2C_HandleTypeDef *lcd)
{
	pc_uart = pc;
	modem_uart = modem;
	lcd_i2c = lcd;

	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, 1);

	UartTransport_Init(pc_uart, modem_uart);
	Protocol_Init(CommandHandler_OnFrame, pc_uart);
	Sim7000_Init(modem_uart);
	ModuleService_Init(CommandHandler_OnOperationComplete);

	LCD_InitStart(lcd_i2c);
	DisplayService_Init();
}

void App_Process(void)
{
	uint16_t len;

	len = UartTransport_Read(pc_uart, rx_buffer, sizeof(rx_buffer));
	if (len > 0)
	{
		Protocol_Feed(rx_buffer, len);
	}

	UartTransport_Process();
	Protocol_Process();
	Sim7000_Process();
	ModuleService_Process();

	DisplayService_Process();
	LCD_Process();

	if (UartTransport_TakeDataLoss(modem_uart))
	{
	  // Data loss from modem
	  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
	}

	if (UartTransport_TakeDataLoss(pc_uart))
	{
	  // Data loss from pc
	  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
	}
}
