/*
 * module_types.h
 *
 *  Created on: 24 жовт. 2026 р.
 *      Author: pavly
 */

#ifndef MODULE_TYPES_H_
#define MODULE_TYPES_H_


#include "stm32f1xx_hal.h"

#define MODULE_MODEL_MAX_LEN     64U
#define MODULE_FW_MAX_LEN        46U
#define MODULE_IMEI_MAX_LEN      24U
#define MODULE_UTC_MAX_LEN       24U

typedef struct {
    char model[MODULE_MODEL_MAX_LEN];
    char firmware[MODULE_FW_MAX_LEN];
    char imei[MODULE_IMEI_MAX_LEN];
} ModuleInfo;

typedef enum {
    GNSS_STATUS_DISABLED = 0,
    GNSS_STATUS_NO_FIX,
    GNSS_STATUS_VALID_FIX
} GnssStatus;

typedef enum {
    GNSS_FIELD_STATUS             = (1U << 0),
    GNSS_FIELD_UTC                = (1U << 1),
    GNSS_FIELD_LATITUDE           = (1U << 2),
    GNSS_FIELD_LONGITUDE          = (1U << 3),
    GNSS_FIELD_ALTITUDE           = (1U << 4),
    GNSS_FIELD_SPEED              = (1U << 5),
    GNSS_FIELD_COURSE             = (1U << 6),
    GNSS_FIELD_HDOP               = (1U << 7),
    GNSS_FIELD_SATELLITES_VISIBLE = (1U << 8),
    GNSS_FIELD_SATELLITES_USED    = (1U << 9)
} GnssField;

typedef struct {
    GnssStatus status;
    char utc[MODULE_UTC_MAX_LEN];

    float latitude;
    float longitude;
    float altitude_m;
    float speed_kmh;
    float course_deg;
    float hdop;

    uint8_t satellites_visible;
    uint8_t satellites_used;

    uint16_t valid_fields;
} GnssFix;

typedef enum {
	GNSS_POWER_NONE = 0U,
    GNSS_POWER_OFF = 1U,
    GNSS_POWER_ON = 2U
} GnssPower;

typedef struct {
    uint8_t cmd;
    uint8_t seq;

    uint8_t status;
    uint8_t error;

    ModuleInfo info;
    GnssFix fix;
    GnssPower gnss_power;
} ModuleResult;

typedef struct {
    uint8_t cmd;
    uint8_t seq;
    GnssPower gnss_power;
} ModuleRequest;


#endif /* MODULE_TYPES_H_ */
