/*
 * lcd.c
 *
 *  Created on: 25 жовт. 2026 р.
 *      Author: pavly
 */


#include "lcd_i2c.h"
#include <string.h>

#define LCD_ADDR       (0x27U << 1)  // HAL expects shifted address
#define LCD_COLS       16U
#define LCD_ROWS       2U
#define LCD_SIZE       (LCD_COLS * LCD_ROWS)

#define LCD_RS         0x01U
#define LCD_ENABLE     0x04U
#define LCD_BACKLIGHT  0x08U

#define LCD_RETRY_MS   10U
#define LCD_TX_TIMEOUT_MS 20U

typedef enum
{
    LCD_STATE_OFF,
    LCD_STATE_INIT_WAIT,
    LCD_STATE_INIT_TX,
    LCD_STATE_READY,
    LCD_STATE_SET_ADDRESS,
    LCD_STATE_WRITE_CHAR,
    LCD_STATE_ERROR
} LCD_State;

typedef enum
{
    TX_ACTION_NONE,
    TX_ACTION_INIT_STEP,
    TX_ACTION_SET_ADDRESS,
    TX_ACTION_WRITE_CHAR
} LCD_TxAction;

typedef struct
{
    uint8_t value;
    uint8_t nibble_only;
    uint16_t delay_ms;
} LCD_InitStep;

static const LCD_InitStep init_sequence[] =
{
    {0x30, 1, 5}, // Init sequence of 3 nibbles
    {0x30, 1, 1},
    {0x30, 1, 1},
    {0x20, 1, 1}, // Enable 4-bit mode
    {0x28, 0, 1}, // Function Set
    {0x08, 0, 1}, // Display Off
    {0x01, 0, 2}, // Clear Display
    {0x06, 0, 1}, // Entry Mode Set
    {0x0C, 0, 1}  // Display On
};

#define INIT_STEP_COUNT (sizeof(init_sequence) / sizeof(init_sequence[0]))

static I2C_HandleTypeDef *lcd_i2c;

static volatile bool tx_busy;
static volatile bool tx_done;
static volatile bool tx_error;

static uint8_t tx_buffer[4];
static uint16_t tx_length;

static LCD_State state = LCD_STATE_OFF;
static LCD_TxAction tx_action = TX_ACTION_NONE;

static uint8_t init_step;
static uint8_t cursor;
static uint8_t current_pos;

static uint32_t deadline;
static uint32_t tx_started_at;

static char desired[LCD_SIZE];
static char displayed[LCD_SIZE];

static uint8_t current_char;


static bool TimeReached(uint32_t now, uint32_t target)
{
    return (int32_t)(now - target) >= 0;
}

static bool StartTransfer(uint16_t length, LCD_TxAction action)
{
    if (tx_busy || length == 0U || length > sizeof(tx_buffer))
        return false;

    tx_length = length;
    tx_done = false;
    tx_error = false;
    tx_action = action;
    tx_started_at = HAL_GetTick();

    tx_busy = true;

    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit_IT(
        lcd_i2c,
        LCD_ADDR,
        tx_buffer,
        tx_length
    );

    if (status != HAL_OK)
    {
        tx_busy = false;
        tx_action = TX_ACTION_NONE;
        tx_error = true;
        return false;
    }

    return true;
}

static bool StartNibble(uint8_t nibble, LCD_TxAction action)
{
    uint8_t base = (nibble & 0xF0U) | LCD_BACKLIGHT;

    tx_buffer[0] = base | LCD_ENABLE;
    tx_buffer[1] = base;

    return StartTransfer(2U, action);
}

static bool StartByte(uint8_t value,
                      uint8_t rs,
                      LCD_TxAction action)
{
    uint8_t base = (value & 0xF0U) | LCD_BACKLIGHT | rs;

    tx_buffer[0] = base | LCD_ENABLE;
    tx_buffer[1] = base;

    base = ((value << 4) & 0xF0U) | LCD_BACKLIGHT | rs;

    tx_buffer[2] = base | LCD_ENABLE;
    tx_buffer[3] = base;

    return StartTransfer(4U, action);
}

void LCD_InitStart(I2C_HandleTypeDef *hi2c)
{
    lcd_i2c = hi2c;

    tx_busy = false;
    tx_done = false;
    tx_error = false;
    tx_action = TX_ACTION_NONE;

    init_step = 0U;
    cursor = 0U;

    memset(desired, ' ', sizeof(desired));
    memset(displayed, 0xFF, sizeof(displayed));

    deadline = HAL_GetTick() + 50U;
    state = LCD_STATE_INIT_WAIT;
}

void LCD_SetText(uint8_t row, const char *text)
{
    if (row >= LCD_ROWS || text == NULL)
        return;

    uint8_t offset = row * LCD_COLS;
    uint8_t i = 0U;

    while (i < LCD_COLS && text[i] != '\0')
    {
        desired[offset + i] = text[i];
        i++;
    }

    while (i < LCD_COLS)
    {
        desired[offset + i] = ' ';
        i++;
    }
}

bool LCD_IsReady(void)
{
    return state == LCD_STATE_READY ||
           state == LCD_STATE_SET_ADDRESS ||
           state == LCD_STATE_WRITE_CHAR;
}

void LCD_Process(void)
{
    if (lcd_i2c == NULL || state == LCD_STATE_OFF || state == LCD_STATE_ERROR)
    {
        return;
    }

    uint32_t now = HAL_GetTick();

    if (tx_busy && (uint32_t)(now - tx_started_at) >= LCD_TX_TIMEOUT_MS)
    {
        tx_busy = false;
        tx_done = false;
        tx_error = true;
        tx_action = TX_ACTION_NONE;

        (void)HAL_I2C_Master_Abort_IT(lcd_i2c, LCD_ADDR);
    }

    if (tx_error)
    {
        tx_error = false;
        tx_done = false;

        tx_action = TX_ACTION_NONE;
        init_step = 0U;
        deadline = now + LCD_RETRY_MS;
        state = LCD_STATE_INIT_WAIT;
        return;
    }

    if (tx_done)
    {
        tx_done = false;

        switch (tx_action)
        {
            case TX_ACTION_INIT_STEP:
                deadline = now +
                    init_sequence[init_step].delay_ms;
                init_step++;
                state = LCD_STATE_INIT_WAIT;
                break;

            case TX_ACTION_SET_ADDRESS:
                state = LCD_STATE_WRITE_CHAR;
                deadline = now + 1U;
                break;

            case TX_ACTION_WRITE_CHAR:
                displayed[current_pos] = (char)current_char;
                cursor = current_pos + 1U;
                state = LCD_STATE_READY;
                deadline = now + 1U;
                break;

            default:
                break;
        }

        tx_action = TX_ACTION_NONE;
        return;
    }

    if (tx_busy || !TimeReached(now, deadline))
        return;

    if (state == LCD_STATE_INIT_WAIT)
    {
        if (init_step >= INIT_STEP_COUNT)
        {
            state = LCD_STATE_READY;
            cursor = 0U;
            return;
        }

        const LCD_InitStep *step = &init_sequence[init_step];

        bool started;

        if (step->nibble_only)
        {
            started = StartNibble(step->value, TX_ACTION_INIT_STEP);
        }
        else
        {
            started = StartByte(step->value, 0U, TX_ACTION_INIT_STEP);
        }

        if (!started && !tx_busy)
        {
            deadline = now + LCD_RETRY_MS;
        }

        return;
    }

    if (state == LCD_STATE_READY)
    {
        uint8_t pos = cursor;

        while (pos < LCD_SIZE && desired[pos] == displayed[pos])
        {
            pos++;
        }

        if (pos >= LCD_SIZE)
        {
            cursor = 0U;
            return;
        }

        current_pos = pos;
        state = LCD_STATE_SET_ADDRESS;
    }

    if (state == LCD_STATE_SET_ADDRESS)
    {
        uint8_t row = current_pos / LCD_COLS;
        uint8_t col = current_pos % LCD_COLS;
        uint8_t address = (row == 0U)
                        ? col
                        : (uint8_t)(0x40U + col);

        if (!StartByte((uint8_t)(0x80U | address), 0U, TX_ACTION_SET_ADDRESS))
        {
            if (!tx_busy)
                deadline = now + LCD_RETRY_MS;
        }

        return;
    }

    if (state == LCD_STATE_WRITE_CHAR)
    {
    	current_char = (uint8_t)desired[current_pos];

        if (!StartByte((uint8_t)desired[current_pos], LCD_RS, TX_ACTION_WRITE_CHAR))
        {
            if (!tx_busy)
                deadline = now + LCD_RETRY_MS;
        }

        return;
    }
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == lcd_i2c)
    {
        tx_busy = false;
        tx_done = true;
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c == lcd_i2c)
    {
        tx_busy = false;
        tx_error = true;
    }
}
