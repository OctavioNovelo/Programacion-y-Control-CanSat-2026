/*
 * telemetry.h
 *
 *  Created on: Mar 13, 2026
 *      Author: shuten-doji
 */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>


// Paquete de Telemetria
#pragma pack(push, 1)
typedef struct
{
    uint16_t header;
    uint16_t pkt;
    uint16_t temperatura;
	uint16_t presion;
	uint16_t humidity;
    uint16_t altitud;
	uint16_t accel_y;
    uint16_t trigger;
} TelemetryPacketLoRa;
#pragma pack(pop)w

// Funcion de contruccion de telemetria
void telemetry_build_LoRa(TelemetryPacketLoRa *protocolLoRa, uint16_t header, uint16_t temperatura, uint16_t presion, uint16_t humidity,  uint16_t altitud, uint16_t accel_y, uint16_t trigger);

#endif
