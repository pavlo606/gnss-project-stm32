/*
 * module_service.h
 *
 *  Created on: 23 жовт. 2026 р.
 *      Author: pavly
 */

#ifndef MODULE_SERVICE_H_
#define MODULE_SERVICE_H_


#include "stm32f1xx_hal.h"
#include "module_types.h"
#include <stdbool.h>

typedef void (*ModuleResultCallback)(
		const ModuleResult *result
);

bool ModuleService_Start(const ModuleRequest *request);

void ModuleService_Init(ModuleResultCallback callback);
void ModuleService_Process(void);
bool ModuleService_IsBusy(void);


#endif /* MODULE_SERVICE_H_ */
