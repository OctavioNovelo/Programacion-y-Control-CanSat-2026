/*
 * sensors.h
 *
 *  Created on: Apr 10, 2026
 *      Author: shuten-doji
 */

#ifndef INC_SENSORS_H_
#define INC_SENSORS_H_

#include "main.h"
#include "telemetry.h"

// Inicializa el BME280 y el BNO085
void sensors_init(void);

// Lee ambos sensores y llena el paquete de telemetría
void read_sensors(CanSat_Packet *p);

// Esta función debe llamarse constantemente en el while(1) para el BNO085
void sensors_service(void);


#endif /* INC_SENSORS_H_ */
