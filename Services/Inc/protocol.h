/*
 * protocol.h
 *
 *  Created on: 22 жовт. 2026 р.
 *      Author: pavly
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_


#include "stm32f1xx_hal.h"
#include <stdbool.h>

#define PROTOCOL_MAX_DATA 256U
#define PROTOCOL_FLAG_RESPONSE  (1U << 0)
#define PROTOCOL_FLAG_ERROR     (1U << 1)

/* 						UART Protocol
 * 	---------------------------------------------------------
 *	|  SOF	|  LEN	| FLAGS	|  CMD	|  SEQ	| DATA	|  CRC	|
 * 	|  2B	|  1B	|  1B	|  1B	|  1B	|0-256B	|  2B	|
 *	| A5 5A	|		|		|		|		|		|		|
 *	---------------------------------------------------------
 */

/* 				Data
 * 	---------------------------------
 *	| STATUS | ERROR | Payload(TLV) |
 *	---------------------------------
 */


typedef struct {
	uint8_t flags;
	uint8_t cmd;
	uint8_t seq;
	uint8_t data[PROTOCOL_MAX_DATA];
	uint8_t data_len;
} ProtocolFrame;

typedef struct
{
    uint32_t frames_ok;
    uint32_t crc_errors;
    uint32_t length_errors;
    uint32_t frame_timeouts;
} ProtocolStats;

void Protocol_GetStats(ProtocolStats *stats);
void Protocol_ResetStats(void);

typedef void (*ProtocolFrameCallback)(
		const ProtocolFrame *frame
);

void Protocol_Init(ProtocolFrameCallback callback, UART_HandleTypeDef *pc);

void Protocol_Process(void);

void Protocol_Feed(const uint8_t *data, size_t len);

bool Protocol_Encode(
    const ProtocolFrame *frame,
    uint8_t *output,
    size_t capacity,
    size_t *output_len
);

void Protocol_SendResponse(const ProtocolFrame *frame);

#endif /* PROTOCOL_H_ */
