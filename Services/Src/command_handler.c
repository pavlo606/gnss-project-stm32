/*
 * command_handler.c
 *
 *  Created on: 23 жовт. 2026 р.
 *      Author: pavly
 */


#include "command_handler.h"
#include "module_service.h"
#include "display_service.h"

#include <string.h>

static void SendStatus(
    uint8_t cmd,
    uint8_t seq,
    CommandStatus status,
    CommandError error)
{
	ProtocolFrame frame = {0};

	frame.cmd = cmd;
	frame.seq = seq;

	frame.data[0] = (uint8_t)status;
	frame.data[1] = (uint8_t)error;

	frame.data_len = 2;

    frame.flags = PROTOCOL_FLAG_RESPONSE;

    if (status == CMD_STATUS_FAILED ||
        status == CMD_STATUS_INVALID ||
        status == CMD_STATUS_BUSY) {
    	frame.flags |= PROTOCOL_FLAG_ERROR;
    }

    Protocol_SendResponse(&frame);
}

static bool TLV_AddString(
		uint8_t *dst,
		size_t max_len,
		size_t *result_len,
		const char *line,
		PayloadType type)
{
	size_t value_len = strlen(line);
	if (*result_len > max_len || (uint16_t)(max_len - *result_len) < value_len + 2U)
		return false;

	dst[(*result_len)++] = type;
	dst[(*result_len)++] = value_len;

	memcpy(&dst[*result_len], line, value_len);
	*result_len += value_len;
	return true;
}

static bool TLV_AddFloat(
		uint8_t *dst,
		size_t max_len,
		size_t *result_len,
		const float value,
		PayloadType type)
{
	if (*result_len > max_len || (uint16_t)(max_len - *result_len) < 6U)
	        return false;

	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));

	dst[(*result_len)++] = (uint8_t)type;
	dst[(*result_len)++] = 4U;
	dst[(*result_len)++] = (uint8_t)(bits);
	dst[(*result_len)++] = (uint8_t)(bits >> 8);
	dst[(*result_len)++] = (uint8_t)(bits >> 16);
	dst[(*result_len)++] = (uint8_t)(bits >> 24);

	return true;
}

static bool TLV_AddByte(
		uint8_t *dst,
		size_t max_len,
		size_t *result_len,
		const uint8_t value,
		PayloadType type)
{
	if (*result_len > max_len || (uint16_t)(max_len - *result_len) < 3U)
	        return false;

	dst[(*result_len)++] = (uint8_t)type;
	dst[(*result_len)++] = 1U;
	dst[(*result_len)++] = value;

	return true;
}

static bool Encode_TLV(
		uint8_t *dst,
		size_t max_len,
		size_t *final_len,
		const ModuleResult *result)
{
	size_t result_len = 0;

	if (result->info.firmware[0] != '\0')
	{
		if (!TLV_AddString(dst, max_len, &result_len, result->info.firmware, TYPE_MODULE_FIRMWARE))
			return false;
	}
	if (result->info.imei[0] != '\0')
	{
		if (!TLV_AddString(dst, max_len, &result_len, result->info.imei, TYPE_MODULE_IMEI))
			return false;
	}
	if (result->info.model[0] != '\0')
	{
		if (!TLV_AddString(dst, max_len, &result_len, result->info.model, TYPE_MODULE_MODEL))
			return false;
	}
	if (result->gnss_power != GNSS_POWER_NONE)
	{
		if (!TLV_AddByte(dst, max_len, &result_len, (uint8_t)result->gnss_power, TYPE_GNSS_RUN_STATUS))
			return false;
	}
	if (result->fix.valid_fields & GNSS_FIELD_STATUS)
	{
		if (!TLV_AddByte(dst, max_len, &result_len, (uint8_t)result->fix.status, TYPE_GNSS_FIXSTATUS))
			return false;
	}
	if (result->fix.valid_fields & GNSS_FIELD_UTC)
	{
		if(!TLV_AddString(dst, max_len, &result_len, result->fix.utc, TYPE_GNSS_UTC))
			return false;
	}
	if (result->fix.valid_fields & GNSS_FIELD_LATITUDE)
	{
		if(!TLV_AddFloat(dst, max_len, &result_len, result->fix.latitude, TYPE_GNSS_LAT))
			return false;
	}
	if (result->fix.valid_fields & GNSS_FIELD_LONGITUDE)
	{
		if(!TLV_AddFloat(dst, max_len, &result_len, result->fix.longitude, TYPE_GNSS_LON))
			return false;
	}
	if (result->fix.valid_fields & GNSS_FIELD_ALTITUDE)
	{
		if(!TLV_AddFloat(dst, max_len, &result_len, result->fix.altitude_m, TYPE_GNSS_ALT))
			return false;
	}
	if (result->fix.valid_fields & GNSS_FIELD_SPEED)
	{
		if(!TLV_AddFloat(dst, max_len, &result_len, result->fix.speed_kmh, TYPE_GNSS_SPEED))
			return false;
	}
	if (result->fix.valid_fields & GNSS_FIELD_COURSE)
	{
		if(!TLV_AddFloat(dst, max_len, &result_len, result->fix.course_deg, TYPE_GNSS_COURSE))
			return false;
	}
	if (result->fix.valid_fields & GNSS_FIELD_SATELLITES_VISIBLE)
	{
		if(!TLV_AddByte(dst, max_len, &result_len, result->fix.satellites_visible, TYPE_GNSS_SAT_VISIBLE))
			return false;
	}
	if (result->fix.valid_fields & GNSS_FIELD_SATELLITES_USED)
	{
		if(!TLV_AddByte(dst, max_len, &result_len, result->fix.satellites_used, TYPE_GNSS_SAT_USED))
			return false;
	}
	if (result->fix.valid_fields & GNSS_FIELD_HDOP)
	{
		if(!TLV_AddFloat(dst, max_len, &result_len, result->fix.hdop, TYPE_GNSS_HDOP))
			return false;
	}

	*final_len = result_len;
	return true;
}

void CommandHandler_OnFrame(const ProtocolFrame *frame)
{
	uint8_t flags = frame->flags;
	uint8_t cmd = frame->cmd;
	uint8_t seq = frame->seq;
	const uint8_t *data = frame->data;
	uint8_t data_len = frame->data_len;

	ModuleRequest request = { 0 };

	if ((flags & PROTOCOL_FLAG_RESPONSE) != 0U) {
		return;
	}

	switch (cmd)
	{
		case CMD_PING:
			if (data_len != 0U) {
				SendStatus(cmd, seq, CMD_STATUS_INVALID,
						   CMD_ERROR_INTERNAL);
				return;
			}

			SendStatus(cmd, seq, CMD_STATUS_SUCCESS,
					   CMD_ERROR_NONE);
			break;

		case CMD_MODULE_INFO:
			request.cmd = cmd;
			request.seq = seq;

			if (!ModuleService_Start(&request)) {
				SendStatus(cmd, seq, CMD_STATUS_BUSY,
						   CMD_ERROR_NONE);
				return;
			}

			SendStatus(cmd, seq, CMD_STATUS_ACCEPTED,
					   CMD_ERROR_NONE);
			break;

		case CMD_GNSS_POWER:
			request.cmd = cmd;
			request.seq = seq;
			request.gnss_power = (GnssPower)data[0];

			if (!ModuleService_Start(&request)) {
				SendStatus(cmd, seq, CMD_STATUS_BUSY,
						   CMD_ERROR_NONE);
				return;
			}

			SendStatus(cmd, seq, CMD_STATUS_ACCEPTED,
					   CMD_ERROR_NONE);
			break;

		case CMD_GET_FIX:
			request.cmd = cmd;
			request.seq = seq;

			if (!ModuleService_Start(&request)) {
				SendStatus(cmd, seq, CMD_STATUS_BUSY,
						   CMD_ERROR_NONE);
				return;
			}

			SendStatus(cmd, seq, CMD_STATUS_ACCEPTED,
					   CMD_ERROR_NONE);
			DisplayService_RequestStarted();
			break;

		default:
			SendStatus(cmd, seq, CMD_STATUS_INVALID,
					   CMD_ERROR_NONE);
			break;
	}
}

void CommandHandler_OnOperationComplete(const ModuleResult *result)
{
	ProtocolFrame frame = {0};

	frame.cmd = result->cmd;
	frame.seq = result->seq;

	frame.data[0] = (uint8_t)result->status;
	frame.data[1] = (uint8_t)result->error;

	frame.flags = PROTOCOL_FLAG_RESPONSE;

	frame.data_len = 2U;

	size_t len;

	if (result->error == CMD_ERROR_NONE)
	{
		if (Encode_TLV(&frame.data[2], sizeof(frame.data) - 2U, &len, result))
		{
			frame.data_len += len;
		} else
		{
			frame.data[0] = CMD_STATUS_FAILED;
			frame.data[1] = CMD_ERROR_BUFFER_OVERFLOW;
			frame.flags |= PROTOCOL_FLAG_ERROR;
		}
	} else
	{
		frame.flags |= PROTOCOL_FLAG_ERROR;
	}

	Protocol_SendResponse(&frame);

	if (frame.cmd != CMD_GET_FIX)
		return;

	if (result->fix.status == GNSS_STATUS_DISABLED)
	{
		DisplayService_SetError(DISPLAY_ERR_GNSS_OFF);
	} else if (result->fix.status == GNSS_STATUS_NO_FIX)
	{
		DisplayService_NoFix(result->fix.satellites_visible, result->fix.hdop);
	} else
	{
		DisplayService_RequestSucceeded(
				result->fix.latitude,
				result->fix.longitude,
				((result->fix.valid_fields & GNSS_FIELD_LATITUDE) &&
						(result->fix.valid_fields & GNSS_FIELD_LONGITUDE)));
	}
}
