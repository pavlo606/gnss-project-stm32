/*
 * gnss_parser.h
 *
 *  Created on: 24 жовт. 2026 р.
 *      Author: pavly
 */

#ifndef GNSS_PARSER_H_
#define GNSS_PARSER_H_


#include "stm32f1xx_hal.h"
#include "module_types.h"
#include <stdbool.h>

bool Sim7000_ParseCgnsinf(const char *response, GnssFix *out);


#endif /* GNSS_PARSER_H_ */
