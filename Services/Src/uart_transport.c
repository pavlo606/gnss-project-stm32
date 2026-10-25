/*
 * uart_transport.c
 *
 *  Created on: 21 жовт. 2026 р.
 *      Author: pavly
 */


#include "uart_transport.h"
#include "display_service.h"
#include <string.h>

#define RX_DMA_SIZE    128
#define RX_QUEUE_SIZE  512
#define TX_QUEUE_SIZE  512
#define TX_DMA_SIZE     64

typedef struct
{
    UART_HandleTypeDef *huart;

    uint8_t rx_dma[RX_DMA_SIZE];
    uint16_t rx_last_pos;

    uint8_t rx_queue[RX_QUEUE_SIZE];
    volatile uint16_t rx_head;
    volatile uint16_t rx_tail;

    uint8_t tx_queue[TX_QUEUE_SIZE];
    uint16_t tx_head;
    uint16_t tx_tail;

    uint8_t tx_dma[TX_DMA_SIZE];
    volatile uint8_t tx_busy;

	volatile uint32_t rx_bytes;
	volatile uint32_t rx_overflows;
	volatile uint32_t rx_dropped_bytes;
	volatile uint32_t rx_loss_events;

	volatile uint32_t tx_bytes;
	volatile uint32_t tx_overflows;
	volatile uint32_t tx_dma_errors;

	volatile uint32_t uart_errors;
	volatile uint32_t last_error;

	volatile uint8_t rx_restart_required;
	volatile uint8_t recovery_required;
	volatile uint8_t data_loss;

} UartTransport;

static UartTransport pc;
static UartTransport modem;

static UartTransport *FindTransport(UART_HandleTypeDef *huart)
{
    if (huart == pc.huart) return &pc;
    if (huart == modem.huart) return &modem;

    return NULL;
}

static uint16_t RxQueue_Free(UartTransport *t)
{
    uint16_t head = t->rx_head;
    uint16_t tail = t->rx_tail;

    return (tail + RX_QUEUE_SIZE - head - 1)
           % RX_QUEUE_SIZE;
}

static bool RxQueue_Write(
    UartTransport *t,
    const uint8_t *data,
    uint16_t len)
{
	if (len > RxQueue_Free(t))
	{
		t->rx_overflows++;
		t->rx_dropped_bytes += len;
		t->rx_loss_events++;
		t->data_loss = 1;
		return false;
	}
    uint16_t head = t->rx_head;

    for (uint16_t i = 0; i < len; i++)
    {
        t->rx_queue[head] = data[i];

        if (++head >= RX_QUEUE_SIZE)
            head = 0;
    }

    t->rx_head = head;
    t->rx_bytes += len;

    return true;
}

void UartTransport_Init(
    UART_HandleTypeDef *pc_uart,
    UART_HandleTypeDef *modem_uart)
{
    memset(&pc, 0, sizeof(pc));
    memset(&modem, 0, sizeof(modem));

    pc.huart = pc_uart;
    modem.huart = modem_uart;

    HAL_UARTEx_ReceiveToIdle_DMA(pc.huart, pc.rx_dma, RX_DMA_SIZE);

    HAL_UARTEx_ReceiveToIdle_DMA(modem.huart, modem.rx_dma, RX_DMA_SIZE);
}

uint16_t UartTransport_Read(
    UART_HandleTypeDef *huart,
    uint8_t *dst,
    uint16_t max_len)
{
    UartTransport *t = FindTransport(huart);

    if (t == NULL || dst == NULL || max_len == 0)
        return 0;

    uint16_t tail = t->rx_tail;
    uint16_t head = t->rx_head;
    uint16_t count = 0;

    while (tail != head && count < max_len)
    {
        dst[count++] = t->rx_queue[tail];

        if (++tail >= RX_QUEUE_SIZE)
            tail = 0;
    }

    t->rx_tail = tail;

    return count;
}

bool UartTransport_Send(
    UART_HandleTypeDef *huart,
    const uint8_t *data,
    uint16_t len)
{
    UartTransport *t = FindTransport(huart);

    if (t == NULL || data == NULL || len == 0)
        return false;

    uint16_t head = t->tx_head;
    uint16_t tail = t->tx_tail;

    uint16_t free_space =
        (tail + TX_QUEUE_SIZE - head - 1)
        % TX_QUEUE_SIZE;

    if (len > free_space)
    {
        t->tx_overflows++;
        return false;
    }

    for (uint16_t i = 0; i < len; i++)
    {
        t->tx_queue[head] = data[i];

        if (++head >= TX_QUEUE_SIZE)
            head = 0;
    }

    t->tx_head = head;
    return true;
}

static void TxProcess(UartTransport *t)
{
    if (t->tx_busy)
        return;

    uint16_t tail = t->tx_tail;
    uint16_t head = t->tx_head;

    if (tail == head)
        return;

    uint16_t len;

    if (head > tail)
        len = head - tail;
    else
        len = TX_QUEUE_SIZE - tail;

    if (len > TX_DMA_SIZE)
        len = TX_DMA_SIZE;

    memcpy(t->tx_dma, &t->tx_queue[tail], len);

    t->tx_busy = 1;

    if (HAL_UART_Transmit_DMA(t->huart, t->tx_dma, len) == HAL_OK)
    {
        t->tx_tail = (tail + len) % TX_QUEUE_SIZE;
    }
    else
    {
        t->tx_busy = 0;
    }
}

static void RecoverTransport(UartTransport *t)
{
	if (!t->recovery_required && !t->rx_restart_required)
		return;

	if (t->recovery_required)
	{
		t->recovery_required = 0;

		HAL_UART_AbortReceive(t->huart);

		t->rx_last_pos = 0;
		t->data_loss = 1;
	}

    HAL_StatusTypeDef status =
        HAL_UARTEx_ReceiveToIdle_DMA(
            t->huart,
            t->rx_dma,
            RX_DMA_SIZE);

	t->rx_restart_required = (status != HAL_OK);
}

void UartTransport_Process(void)
{
	RecoverTransport(&pc);
	RecoverTransport(&modem);

    TxProcess(&pc);
    TxProcess(&modem);
}

bool UartTransport_TakeDataLoss(
    UART_HandleTypeDef *huart)
{
    UartTransport *t = FindTransport(huart);

    if (t == NULL)
        return false;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    bool lost = t->data_loss != 0;
    t->data_loss = 0;

    __set_PRIMASK(primask);

    return lost;
}

bool UartTransport_GetStats(
    UART_HandleTypeDef *huart,
    UartTransport_Stats *out)
{
    UartTransport *t = FindTransport(huart);

    if (t == NULL || out == NULL)
        return false;

    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    out->rx_bytes = t->rx_bytes;
    out->rx_overflows = t->rx_overflows;
    out->rx_dropped_bytes = t->rx_dropped_bytes;
    out->rx_loss_events = t->rx_loss_events;
    out->tx_bytes = t->tx_bytes;
    out->tx_overflows = t->tx_overflows;
    out->tx_dma_errors = t->tx_dma_errors;
    out->uart_errors = t->uart_errors;
    out->last_error = t->last_error;

    __set_PRIMASK(primask);

    return true;
}

void HAL_UARTEx_RxEventCallback(
    UART_HandleTypeDef *huart,
    uint16_t Size)
{
    UartTransport *t = FindTransport(huart);

    if (t == NULL)
        return;

    uint16_t old_pos = t->rx_last_pos;
    uint16_t first_len;
    uint16_t second_len = 0;

    if (Size > RX_DMA_SIZE || t->rx_last_pos > RX_DMA_SIZE)
    {
    	t->uart_errors++;
    	t->data_loss = 1;
        return;
    }

    if (Size >= old_pos)
	{
		first_len = Size - old_pos;
	}
	else
	{
		first_len = RX_DMA_SIZE - old_pos;
		second_len = Size;
	}

    uint16_t total = first_len + second_len;

    if (total > RxQueue_Free(t))
	{
		t->rx_overflows++;
		t->rx_dropped_bytes += total;
		t->rx_loss_events++;
		t->data_loss = 1;

		t->rx_last_pos = Size;
		return;
	}

    if (first_len > 0)
		RxQueue_Write(t, &t->rx_dma[old_pos], first_len);

	if (second_len > 0)
		RxQueue_Write(t, &t->rx_dma[0], second_len);

    t->rx_last_pos = Size;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    UartTransport *t = FindTransport(huart);

    if (t != NULL)
        t->tx_busy = 0;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    UartTransport *t = FindTransport(huart);

    if (t == NULL)
        return;

    t->uart_errors++;
    t->last_error = huart->ErrorCode;
    t->recovery_required = 1;
    t->data_loss = 1;

    if (t->tx_busy)
    {
        t->tx_dma_errors++;
        t->tx_busy = 0;
    }
    DisplayService_SetError(DISPLAY_ERR_UART);
}
