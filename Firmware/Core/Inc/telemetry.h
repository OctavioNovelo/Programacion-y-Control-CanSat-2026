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
#pragma pack(push,1)
typedef struct
{
	uint8_t magic;
    uint8_t pkt;
    int16_t temperatura;
    int16_t altitud;
    uint16_t presion;
    uint8_t verificacion;
    uint8_t checksum;

} TelemetryPacketLoRa;
#pragma pack(pop)

// Funcion de contruccion de telemetria
void telemetry_build_LoRa(TelemetryPacketLoRa *protocolLoRa, int16_t temperatura, uint16_t altitud, uint16_t presion, uint8_t verificacion);


// Paquete de IMU
#pragma pack(push,1)
typedef struct
{
	float accel_x;
	float accel_y;
	float accel_z;
	float giro_x;
	float giro_y;
	float giro_z;
} PacketLoRaBNO;
#pragma pack(pop)

// Funcion de construccion para el paquete de IMU
void telemetry_LoRa_BNO(PacketLoRaBNO *telemetryBNO, float accel_x, float accel_y, float accel_z, float giro_x, float giro_y, float giro_z);

#endif
