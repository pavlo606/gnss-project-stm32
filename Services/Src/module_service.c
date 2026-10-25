/*
 * module_service.c
 *
 *  Created on: 23 жовт. 2026 р.
 *      Author: pavly
 */

#include "module_service.h"
#include "sim7000.h"
#include "command_handler.h"
#include "gnss_parser.h"
#include "display_service.h"

#include <string.h>

typedef enum {
    SVC_IDLE = 0,

    SVC_INFO_SEND_ATI,
    SVC_INFO_WAIT_ATI,
    SVC_INFO_SEND_CGMR,
    SVC_INFO_WAIT_CGMR,
    SVC_INFO_SEND_GSN,
    SVC_INFO_WAIT_GSN,

    SVC_POWER_SEND,
    SVC_POWER_WAIT,

    SVC_FIX_SEND,
    SVC_FIX_WAIT,
    SVC_FIX_PARSE,

    SVC_COMPLETE,
    SVC_FAILED
} ServiceState;

static ServiceState state = SVC_IDLE;
static ModuleRequest active_request;
static ModuleResult result;

static ModuleResultCallback result_callback;

static bool CopyStringSafe(char *dst, const char *src, size_t max_len)
{
	size_t len = strlen(src);
	if (len > max_len)
		return false;

	memcpy(dst, src, len);
	dst[len] = '\0';

	return true;
}

void ModuleService_Init(ModuleResultCallback callback)
{
	result_callback = callback;
}

bool ModuleService_Start(const ModuleRequest *request)
{
    if (request == NULL || state != SVC_IDLE) {
        return false;
    }

    active_request = *request;

    memset(&result, 0, sizeof(result));

    result.cmd = request->cmd;
    result.seq = request->seq;

    switch (request->cmd) {
    case CMD_MODULE_INFO:
        state = SVC_INFO_SEND_ATI;
        break;

    case CMD_GNSS_POWER:
        state = SVC_POWER_SEND;
        break;

    case CMD_GET_FIX:
        state = SVC_FIX_SEND;
        break;

    default:
        return false;
    }

    return true;
}

void ModuleService_Process(void)
{
    Sim7000_Status at_status;

    switch (state) {
    case SVC_IDLE:
        break;

    case SVC_INFO_SEND_ATI:
        if (Sim7000_StartCommand("ATI", 3000U)
                == SIM7000_STATUS_PENDING) {
            state = SVC_INFO_WAIT_ATI;
        } else {
            state = SVC_FAILED;
        	DisplayService_SetError(DISPLAY_ERR_AT_ERROR);
        }
        break;

    case SVC_INFO_WAIT_ATI:
        at_status = Sim7000_GetStatus();

        if (at_status == SIM7000_STATUS_OK) {
        	if (!CopyStringSafe(
    				result.info.model,
    				Sim7000_GetResponse(),
    				sizeof(result.info.model)))
        	{
        		result.error = CMD_ERROR_BUFFER_OVERFLOW;
        		state = SVC_FAILED;
            	DisplayService_SetError(DISPLAY_ERR_AT_ERROR);
        		return;
        	}

            state = SVC_INFO_SEND_CGMR;
        } else if (at_status != SIM7000_STATUS_PENDING) {
        	result.error = CMD_ERROR_AT;
            state = SVC_FAILED;
        }
        break;

    case SVC_INFO_SEND_CGMR:
        if (Sim7000_StartCommand("AT+CGMR", 3000U)
                == SIM7000_STATUS_PENDING) {
            state = SVC_INFO_WAIT_CGMR;
        } else {
        	result.error = CMD_ERROR_AT;
        	DisplayService_SetError(DISPLAY_ERR_AT_ERROR);
            state = SVC_FAILED;
        }
        break;

    case SVC_INFO_WAIT_CGMR:
        at_status = Sim7000_GetStatus();

        if (at_status == SIM7000_STATUS_OK) {
        	if (!CopyStringSafe(
					result.info.firmware,
					Sim7000_GetResponse(),
					sizeof(result.info.firmware)))
			{
				result.error = CMD_ERROR_BUFFER_OVERFLOW;
				state = SVC_FAILED;
	        	DisplayService_SetError(DISPLAY_ERR_AT_PARSE);
				return;
			}
            state = SVC_INFO_SEND_GSN;
        } else if (at_status != SIM7000_STATUS_PENDING) {
        	result.error = CMD_ERROR_AT;
            state = SVC_FAILED;
        }
        break;

    case SVC_INFO_SEND_GSN:
        if (Sim7000_StartCommand("AT+GSN", 3000U)
                == SIM7000_STATUS_PENDING) {
            state = SVC_INFO_WAIT_GSN;
        } else {
        	result.error = CMD_ERROR_AT;
        	DisplayService_SetError(DISPLAY_ERR_AT_ERROR);
            state = SVC_FAILED;
        }
        break;

    case SVC_INFO_WAIT_GSN:
        at_status = Sim7000_GetStatus();

        if (at_status == SIM7000_STATUS_OK) {
        	if (!CopyStringSafe(
					result.info.imei,
					Sim7000_GetResponse(),
					sizeof(result.info.imei)))
			{
				result.error = CMD_ERROR_BUFFER_OVERFLOW;
				state = SVC_FAILED;
	        	DisplayService_SetError(DISPLAY_ERR_AT_PARSE);
				return;
			}
            state = SVC_COMPLETE;
        } else if (at_status != SIM7000_STATUS_PENDING) {
        	result.error = CMD_ERROR_AT;
            state = SVC_FAILED;
        }
        break;

    case SVC_POWER_SEND:
    	if (active_request.gnss_power == GNSS_POWER_NONE ||
    	    (active_request.gnss_power != GNSS_POWER_ON &&
    		 active_request.gnss_power != GNSS_POWER_OFF))
    	{
    		result.error = CMD_ERROR_INVALID_ARG;
    		state = SVC_FAILED;
        	DisplayService_SetError(DISPLAY_ERR_INVALID_ARG);
    		return;
    	}
        if (Sim7000_StartCommand(
                active_request.gnss_power == GNSS_POWER_ON
                    ? "AT+CGNSPWR=1"
                    : "AT+CGNSPWR=0",
                3000U) == SIM7000_STATUS_PENDING) {
            state = SVC_POWER_WAIT;
        } else {
        	result.error = CMD_ERROR_AT;
            state = SVC_FAILED;
        	DisplayService_SetError(DISPLAY_ERR_AT_ERROR);
        }
        break;

    case SVC_POWER_WAIT:
        at_status = Sim7000_GetStatus();

        if (at_status == SIM7000_STATUS_OK) {
        	result.gnss_power = active_request.gnss_power;
            state = SVC_COMPLETE;
        } else if (at_status != SIM7000_STATUS_PENDING) {
        	result.error = CMD_ERROR_AT;
            state = SVC_FAILED;
        }
        break;

    case SVC_FIX_SEND:
        if (Sim7000_StartCommand("AT+CGNSINF", 3000U)
                == SIM7000_STATUS_PENDING) {
            state = SVC_FIX_WAIT;
        } else {
        	result.error = CMD_ERROR_AT;
            state = SVC_FAILED;
        	DisplayService_SetError(DISPLAY_ERR_AT_ERROR);
        }
        break;

    case SVC_FIX_WAIT:
        at_status = Sim7000_GetStatus();

        if (at_status == SIM7000_STATUS_OK) {
        	const char *response = Sim7000_GetResponse();
//        	const char *response = "+CGNSINF: 1,1,20261024143910.000,49.501946,23.241543,302.200,0.00,116.2,1,,0.8,1.2,0.8,,24,4,7,,27,,";

			if (Sim7000_ParseCgnsinf(response, &result.fix)) {
				state = SVC_COMPLETE;
			} else {
				result.error = CMD_ERROR_PARSE;
				state = SVC_FAILED;
	        	DisplayService_SetError(DISPLAY_ERR_AT_PARSE);
			}
        } else if (at_status != SIM7000_STATUS_PENDING) {
            state = SVC_FAILED;
        }
        break;

    case SVC_COMPLETE:
    	result.status = CMD_STATUS_SUCCESS;
    	result.error = CMD_ERROR_NONE;

    	result_callback(&result);

        state = SVC_IDLE;
        break;

    case SVC_FAILED:
    	result.status = CMD_STATUS_FAILED;

		result_callback(&result);

        state = SVC_IDLE;
        break;

    default:
        state = SVC_FAILED;
        break;
    }
}

bool ModuleService_IsBusy(void)
{
    return state != SVC_IDLE;
}
