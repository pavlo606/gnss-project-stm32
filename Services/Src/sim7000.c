/*
 * sim7000.c
 *
 *  Created on: 22 жовт. 2026 р.
 *      Author: pavly
 */


#include "sim7000.h"
#include "uart_transport.h"
#include "display_service.h"
#include <string.h>

#define SIM7000_COMMAND_MAX_LEN   128U
#define SIM7000_LINE_MAX_LEN      160U
#define SIM7000_RESPONSE_MAX_LEN  512U

static UART_HandleTypeDef *modem_uart;

static Sim7000_Status status = SIM7000_STATUS_IDLE;

static char active_command[SIM7000_COMMAND_MAX_LEN];

static char line_buffer[SIM7000_LINE_MAX_LEN];
static size_t line_length = 0U;
static bool line_overflow = false;

static char response_buffer[SIM7000_RESPONSE_MAX_LEN];
static size_t response_length = 0U;

static char error_line[SIM7000_LINE_MAX_LEN];

static uint32_t command_start_tick = 0U;
static uint32_t command_timeout_ms = 0U;

static Sim7000_UrcCallback urc_callback = NULL;


static void Sim7000_ResetLine(void)
{
    line_length = 0U;
    line_overflow = false;
    line_buffer[0] = '\0';
}

static void Sim7000_ResetResponse(void)
{
    response_length = 0U;
    response_buffer[0] = '\0';
    error_line[0] = '\0';
}

static bool Sim7000_IsTerminalLine(const char *line)
{
    return (strcmp(line, "OK") == 0) ||
           (strcmp(line, "ERROR") == 0) ||
           (strncmp(line, "+CME ERROR:", 11U) == 0) ||
           (strncmp(line, "+CMS ERROR:", 11U) == 0);
}

static bool Sim7000_IsErrorLine(const char *line)
{
    return (strcmp(line, "ERROR") == 0) ||
           (strncmp(line, "+CME ERROR:", 11U) == 0) ||
           (strncmp(line, "+CMS ERROR:", 11U) == 0);
}

static bool Sim7000_IsUrcLine(const char *line)
{
    if (strcmp(line, "RING") == 0 ||
    	strncmp(line, "+CEREG:", 7U) == 0 ||
        strncmp(line, "+CREG:", 6U) == 0 ||
        strncmp(line, "+CGREG:", 7U) == 0 ||
        strncmp(line, "+CMTI:", 6U) == 0 ||
        strncmp(line, "+QIURC:", 7U) == 0 ||
        strncmp(line, "+PDP: DEACT", 11U) == 0) {
        return true;
    }

    return false;
}

static bool Sim7000_AppendResponse(const char *line)
{
    const size_t length = strlen(line);
    const size_t separator_length =
        (response_length > 0U) ? 2U : 0U;

    if ((response_length + separator_length + length)
        >= sizeof(response_buffer)) {
        return false;
    }

    if (separator_length > 0U) {
        response_buffer[response_length++] = '\r';
        response_buffer[response_length++] = '\n';
    }

    memcpy(&response_buffer[response_length], line, length);
    response_length += length;
    response_buffer[response_length] = '\0';

    return true;
}

static void Sim7000_ProcessLine(const char *line)
{
    if (line[0] == '\0') {
        return;
    }

    if (active_command[0] != '\0' &&
        strcmp(line, active_command) == 0) {
        return;
    }

    if (Sim7000_IsUrcLine(line)) {
        if (urc_callback != NULL) {
            urc_callback(line);
        }
        return;
    }

    if (status != SIM7000_STATUS_PENDING) {
        return;
    }

    if (Sim7000_IsTerminalLine(line)) {
        if (Sim7000_IsErrorLine(line)) {
            size_t len = strlen(line);

            if (len >= sizeof(error_line)) {
                len = sizeof(error_line) - 1U;
            }

            memcpy(error_line, line, len);
            error_line[len] = '\0';

            status = SIM7000_STATUS_ERROR;
        } else {
            status = SIM7000_STATUS_OK;
        }

        return;
    }

    if (!Sim7000_AppendResponse(line)) {
        status = SIM7000_STATUS_OVERFLOW;
    }
}

static void Sim7000_OnRxByte(uint8_t byte)
{
	if (byte == '\r') return;
    if (byte == '\n')
    {
        line_buffer[line_length] = '\0';
        Sim7000_ProcessLine(line_buffer);
        Sim7000_ResetLine();

        return;
    }

    if (line_length < (sizeof(line_buffer) - 1U)) {
        line_buffer[line_length++] = (char)byte;
        line_buffer[line_length] = '\0';
    } else {
    	Sim7000_ResetLine();

		if (status == SIM7000_STATUS_PENDING) {
			status = SIM7000_STATUS_OVERFLOW;
		}

		return;
    }
}


void Sim7000_Init(UART_HandleTypeDef *modem)
{
    status = SIM7000_STATUS_IDLE;

    active_command[0] = '\0';

    modem_uart = modem;

    Sim7000_ResetLine();
    Sim7000_ResetResponse();
}

Sim7000_Status Sim7000_StartCommand(
    const char *command,
    uint32_t timeout_ms)
{
    if (command == NULL ||
        command[0] == '\0' ||
        timeout_ms == 0U) {
        return SIM7000_STATUS_INVALID_ARG;
    }

    if (status == SIM7000_STATUS_PENDING) {
        return SIM7000_STATUS_BUSY;
    }

    const size_t command_length = strlen(command);

    if (command_length == 0U ||
        command_length >= sizeof(active_command) ||
        strchr(command, '\r') != NULL ||
        strchr(command, '\n') != NULL) {
        return SIM7000_STATUS_INVALID_ARG;
    }

    memcpy(active_command, command, command_length);
    active_command[command_length] = '\0';

    Sim7000_ResetLine();
    Sim7000_ResetResponse();

    command_start_tick = HAL_GetTick();
    command_timeout_ms = timeout_ms;
    status = SIM7000_STATUS_PENDING;

    char tx_buffer[SIM7000_COMMAND_MAX_LEN + 2U];

    memcpy(tx_buffer, command, command_length);
    tx_buffer[command_length] = '\r';
    tx_buffer[command_length + 1U] = '\n';

    UartTransport_Send(modem_uart,
                       (uint8_t *)tx_buffer,
                       command_length + 2U);

    return SIM7000_STATUS_PENDING;
}

void Sim7000_Process(void)
{
    uint8_t rx_buffer[64];

    size_t total_read = 0U;
    const size_t max_bytes_per_call = 256U;

    while (total_read < max_bytes_per_call) {
        size_t capacity = sizeof(rx_buffer);

        if (capacity > (max_bytes_per_call - total_read)) {
            capacity = max_bytes_per_call - total_read;
        }

        size_t received =
            UartTransport_Read(modem_uart, rx_buffer, capacity);

        if (received == 0U) {
            break;
        }

        if (received > capacity) {
            received = capacity;
        }

        for (size_t i = 0U; i < received; i++) {
            Sim7000_OnRxByte(rx_buffer[i]);
        }

        total_read += received;
    }

    if (status == SIM7000_STATUS_PENDING &&
        (uint32_t)(HAL_GetTick() - command_start_tick)
            >= command_timeout_ms) {
        status = SIM7000_STATUS_TIMEOUT;
        DisplayService_SetError(DISPLAY_ERR_AT_TIMEOUT);
    }
}

Sim7000_Status Sim7000_GetStatus(void)
{
    return status;
}

bool Sim7000_IsBusy(void)
{
    return status == SIM7000_STATUS_PENDING;
}

const char *Sim7000_GetResponse(void)
{
    return response_buffer;
}

size_t Sim7000_GetResponseLength(void)
{
    return response_length;
}

const char *Sim7000_GetErrorLine(void)
{
    return error_line;
}

void Sim7000_SetUrcCallback(Sim7000_UrcCallback callback)
{
    urc_callback = callback;
}
