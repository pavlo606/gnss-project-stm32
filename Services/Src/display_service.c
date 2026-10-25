/*
 * display_service.c
 *
 *  Created on: 25 жовт. 2026 р.
 *      Author: pavly
 */


#include "display_service.h"
#include "lcd_i2c.h"
#include <stdio.h>
#include <string.h>

#define LCD_LINE_LEN 16

static DisplayState display_state = DISPLAY_IDLE;
static DisplayError display_error = DISPLAY_ERR_NONE;

static DisplayGnssData last_data = {0};

static char line1[LCD_LINE_LEN + 1];
static char line2[LCD_LINE_LEN + 1];


static void FormatLine(char *dst, const char *src)
{
    (void)snprintf(dst, LCD_LINE_LEN + 1, "%-16.16s", src ? src : "");
}

static const char *ErrorText(DisplayError error)
{
    switch (error)
    {
        case DISPLAY_ERR_CRC:
            return "CRC ERROR";

        case DISPLAY_ERR_AT_TIMEOUT:
            return "AT TIMEOUT";

        case DISPLAY_ERR_AT_ERROR:
            return "AT ERROR";

        case DISPLAY_ERR_AT_PARSE:
            return "AT PARSE";

        case DISPLAY_ERR_INVALID_ARG:
            return "INVALID ARG";

        case DISPLAY_ERR_UART:
            return "UART ERROR";

        case DISPLAY_ERR_GNSS_OFF:
            return "GNSS OFF";

        case DISPLAY_ERR_INTERNAL:
            return "INTERNAL ERROR";

        default:
            return "UNKNOWN ERROR";
    }
}

void DisplayService_Init(void)
{
    display_state = DISPLAY_IDLE;
    display_error = DISPLAY_ERR_NONE;

    memset(&last_data, 0, sizeof(last_data));

    FormatLine(line1, "PC: waiting...");
    FormatLine(line2, "SIM7000: ready");

    LCD_SetText(0, line1);
    LCD_SetText(1, line2);
}

void DisplayService_SetIdle(void)
{
    display_state = DISPLAY_IDLE;
    display_error = DISPLAY_ERR_NONE;
}

void DisplayService_RequestStarted(void)
{
    display_state = DISPLAY_BUSY;
    display_error = DISPLAY_ERR_NONE;
}

void DisplayService_RequestSucceeded(float latitude, float longitude, bool valid)
{
    last_data.latitude = latitude;
    last_data.longitude = longitude;
    last_data.valid = valid;
    display_state = DISPLAY_SUCCESS;
    display_error = DISPLAY_ERR_NONE;
}

void DisplayService_NoFix(uint8_t satellites, float hdop)
{
    last_data.satellites = satellites;
    last_data.hdop = hdop;

    display_state = DISPLAY_NO_FIX;
    display_error = DISPLAY_ERR_NONE;
}

void DisplayService_SetError(DisplayError error)
{
    display_error = error;
    display_state = DISPLAY_ERROR;
}

void DisplayService_Process(void)
{
    char buf1[LCD_LINE_LEN + 1];
    char buf2[LCD_LINE_LEN + 1];

    switch (display_state)
    {
        case DISPLAY_IDLE:
            FormatLine(buf1, "PC: waiting...");
            FormatLine(buf2, "SIM7000: ready");
            break;

        case DISPLAY_BUSY:
            FormatLine(buf1, "PC: request");
            FormatLine(buf2, "SIM7000: AT...");
            break;

        case DISPLAY_SUCCESS:
            if (last_data.valid)
            {
                (void)snprintf(buf1, sizeof(buf1),
                               "Lat:% .4f",
                               (double)last_data.latitude);

                (void)snprintf(buf2, sizeof(buf2),
                               "Lon:% .4f",
                               (double)last_data.longitude);
            }
            else
            {
                FormatLine(buf1, "GNSS: NO DATA");
                FormatLine(buf2, "Check response");
            }
            break;

        case DISPLAY_NO_FIX:
            FormatLine(buf1, "GNSS: NO FIX");

            if (last_data.satellites > 0)
            {
                (void)snprintf(buf2, sizeof(buf2),
                               "Sat:%u HDOP:%.1f",
                               (unsigned)last_data.satellites,
                               (double)last_data.hdop);
            }
            else
            {
                FormatLine(buf2, "No position");
            }
            break;

        case DISPLAY_ERROR:
            FormatLine(buf1, "ERROR:");
            FormatLine(buf2, ErrorText(display_error));
            break;

        default:
            FormatLine(buf1, "DISPLAY ERROR");
            FormatLine(buf2, "Invalid state");
            break;
    }

    LCD_SetText(0, buf1);
    LCD_SetText(1, buf2);
}
