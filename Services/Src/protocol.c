/*
 * protocol.c
 *
 *  Created on: 22 жовт. 2026 р.
 *      Author: pavly
 */

#include "protocol.h"
#include "uart_transport.h"
#include "display_service.h"
#include <string.h>

#define SOF_1       0xA5U
#define SOF_2       0x5AU
#define CRC_SIZE    2U
#define HEADER_SIZE 5U
#define MIN_LEN     3U
#define MAX_LEN     (MIN_LEN + PROTOCOL_MAX_DATA)
#define PROTOCOL_MAX_FRAME (2U + 1U + MAX_LEN + 2U)
#define FRAME_TIMEOUT_MS 100U

static UART_HandleTypeDef *pc_uart;

static uint8_t rx_frame[PROTOCOL_MAX_FRAME];
static size_t rx_pos;
static size_t rx_expected;

static ProtocolFrameCallback frame_callback;

static ProtocolStats stats = {0};

static uint32_t frame_started_at;

static uint16_t crc16(const uint8_t* pData, size_t length)
{
    uint8_t i;
    uint16_t wCrc = 0xffff;
    while (length--) {
        wCrc ^= *(unsigned char *)pData++ << 8;
        for (i=0; i < 8; i++)
            wCrc = wCrc & 0x8000 ? (wCrc << 1) ^ 0x1021 : wCrc << 1;
    }
    return wCrc & 0xffff;
}

void Protocol_GetStats(ProtocolStats *out)
{
    if (out != NULL)
    {
        *out = stats;
    }
}

void Protocol_ResetStats(void)
{
    memset(&stats, 0, sizeof(stats));
}

static void Protocol_ResetParser(void)
{
    rx_pos = 0U;
    rx_expected = 0U;
}

void Protocol_Init(ProtocolFrameCallback callback, UART_HandleTypeDef *pc)
{
	pc_uart = pc;

    frame_callback = callback;
    Protocol_ResetParser();
}


static void Protocol_Decode(void)
{
    uint8_t len = rx_frame[2];

    uint16_t received_crc =
        (uint16_t)rx_frame[3U + len] |
        ((uint16_t)rx_frame[4U + len] << 8);

    uint16_t calculated_crc =
        crc16(&rx_frame[2], 1U + len);

    if (received_crc != calculated_crc)
    {
    	stats.crc_errors++;
    	DisplayService_SetError(DISPLAY_ERR_CRC);
        return;
    }

    ProtocolFrame frame = {0};

    frame.flags = rx_frame[3];
    frame.cmd = rx_frame[4];
    frame.seq = rx_frame[5];
    frame.data_len = (uint8_t)(len - MIN_LEN);

    if (frame.data_len > 0U)
    {
        memcpy(
            frame.data,
            &rx_frame[6],
            frame.data_len
        );
    }

    if (frame_callback != NULL)
    {
        frame_callback(&frame);
    }

    stats.frames_ok++;
}

static void Protocol_FeedByte(uint8_t byte)
{
    if (rx_pos == 0U)
    {
        if (byte == SOF_1)
        {
            rx_frame[0] = byte;
            rx_pos = 1U;
            frame_started_at = HAL_GetTick();
        }
        return;
    }

    if (rx_pos == 1U)
    {
        if (byte == SOF_2)
        {
            rx_frame[1] = byte;
            rx_pos = 2U;
        }
        else if (byte != SOF_1)
        {
            Protocol_ResetParser();
        }

        return;
    }

    if (rx_pos >= sizeof(rx_frame))
    {
    	stats.length_errors++;
        Protocol_ResetParser();
        return;
    }

    rx_frame[rx_pos++] = byte;

    if (rx_pos == 3U)
    {
        uint8_t len = rx_frame[2];

        if (len < MIN_LEN || len > MAX_LEN)
        {
        	stats.length_errors++;
            Protocol_ResetParser();
            return;
        }

        rx_expected = 2U + 1U + len + CRC_SIZE;
    }

    if (rx_expected > 0U && rx_pos == rx_expected)
    {
    	Protocol_Decode();
        Protocol_ResetParser();
    }
}

void Protocol_Feed(const uint8_t *data, size_t len)
{
    if (data == NULL)
        return;

    for (size_t i = 0; i < len; i++)
    {
        Protocol_FeedByte(data[i]);
    }
}

bool Protocol_Encode(
    const ProtocolFrame *frame,
    uint8_t *output,
    size_t capacity,
    size_t *output_len)
{
    if (frame == NULL || output == NULL ||
        output_len == NULL ||
        frame->data_len > PROTOCOL_MAX_DATA)
    {
        return false;
    }

    const uint8_t len =
        (uint8_t)(MIN_LEN + frame->data_len);

    const size_t total =
        2U + 1U + len + CRC_SIZE;

    if (capacity < total)
        return false;

    output[0] = SOF_1;
    output[1] = SOF_2;
    output[2] = len;
    output[3] = frame->flags;
    output[4] = frame->cmd;
    output[5] = frame->seq;

    if (frame->data_len > 0U)
    {
        memcpy(&output[6], frame->data, frame->data_len);
    }

    uint16_t crc = crc16(
        &output[2],
        1U + len
    );

    output[3U + len] = (uint8_t)(crc & 0xFFU);
    output[4U + len] = (uint8_t)(crc >> 8);

    *output_len = total;
    return true;
}

void Protocol_Process(void)
{
	if (rx_pos == 0U)
		return;

	uint32_t now = HAL_GetTick();

	if ((uint32_t)(now - frame_started_at) >=
		FRAME_TIMEOUT_MS)
	{
		stats.frame_timeouts++;
		Protocol_ResetParser();
	}
}

void Protocol_SendResponse(const ProtocolFrame *frame)
{
	uint8_t tx_buffer[PROTOCOL_MAX_FRAME];
	size_t tx_len = 0;

	if (Protocol_Encode(
			frame,
			tx_buffer,
			sizeof(tx_buffer),
			&tx_len))
	{
		UartTransport_Send(pc_uart, tx_buffer, tx_len);
	}
}
