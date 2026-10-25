/*
 * command_handler.h
 *
 *  Created on: 23 жовт. 2026 р.
 *      Author: pavly
 */

#ifndef COMMAND_HANDLER_H_
#define COMMAND_HANDLER_H_


#include "stm32f1xx_hal.h"
#include "protocol.h"
#include "module_service.h"

typedef enum {
    CMD_PING        = 0x01,
    CMD_MODULE_INFO = 0x10,
    CMD_GNSS_POWER  = 0x11,
    CMD_GET_FIX     = 0x12
} CommandId;

typedef enum {
    CMD_STATUS_ACCEPTED = 0x00,
    CMD_STATUS_SUCCESS  = 0x01,
    CMD_STATUS_FAILED   = 0x02,
    CMD_STATUS_BUSY     = 0x03,
    CMD_STATUS_INVALID  = 0x04
} CommandStatus;

typedef enum {
    CMD_ERROR_NONE 			  = 0x00,
    CMD_ERROR_AT 			  = 0x01,
	CMD_ERROR_INVALID_ARG	  = 0x02,
    CMD_ERROR_TIMEOUT 		  = 0x03,
    CMD_ERROR_PARSE 		  = 0x04,
    CMD_ERROR_GNSS_OFF 		  = 0x05,
    CMD_ERROR_NO_FIX 		  = 0x06,
    CMD_ERROR_BUFFER_OVERFLOW = 0x07,
    CMD_ERROR_INTERNAL 		  = 0x08
} CommandError;

typedef enum {
    TYPE_MODULE_IMEI 	  = 0x01,
	TYPE_MODULE_FIRMWARE  = 0x02,
	TYPE_MODULE_MODEL 	  = 0x03,
	TYPE_GNSS_RUN_STATUS  = 0x10,
	TYPE_GNSS_FIXSTATUS   = 0x11,
	TYPE_GNSS_UTC 		  = 0x12,
	TYPE_GNSS_LAT 		  = 0x13,
	TYPE_GNSS_LON 		  = 0x14,
	TYPE_GNSS_ALT 		  = 0x15,
	TYPE_GNSS_SPEED 	  = 0x16,
	TYPE_GNSS_COURSE   	  = 0x17,
	TYPE_GNSS_SAT_VISIBLE = 0x18,
	TYPE_GNSS_SAT_USED 	  = 0x19,
	TYPE_GNSS_HDOP		  = 0x1a
} PayloadType;


void CommandHandler_Process(void);

void CommandHandler_OnFrame(const ProtocolFrame *frame);
void CommandHandler_OnOperationComplete(const ModuleResult *result);

#endif /* COMMAND_HANDLER_H_ */
