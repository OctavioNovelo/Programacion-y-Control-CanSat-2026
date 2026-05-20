/*
 * telemetry.c
 *
 *  Created on: Mar 13, 2026
 *      Author: shuten-doji
 */
#include "telemetry.h"
#include "main.h"

static uint8_t pkt = 0;

static uint8_t telemetry_checksum(uint8_t *data, uint8_t len)
{
    uint8_t c = 0;

    for(int i = 0; i < len; i++)
        c ^= data[i];

    return c;
}


void telemetry_build_LoRa(TelemetryPacketLoRa *protocolLoRa, int16_t temperatura, uint16_t altitud, uint16_t presion, uint8_t verificacion)
{
	protocolLoRa->magic = 0xCA;
	protocolLoRa->pkt = pkt++;
	protocolLoRa->temperatura = temperatura;
	protocolLoRa->altitud = altitud;
	protocolLoRa->presion = presion;
	protocolLoRa->verificacion = verificacion;
	protocolLoRa->checksum = telemetry_checksum((uint8_t*)protocolLoRa, sizeof(TelemetryPacketLoRa) - 1);
}

// Juntar con telemetry_build_LoRa
void telemetry_LoRa_BNO(PacketLoRaBNO *telemetryBNO, float accel_x, float accel_y, float accel_z, float gyro_x, float gyro_y, float gyro_z)
{
	telemetryBNO->accel_x = accel_x;
	telemetryBNO->accel_y = accel_y;
	telemetryBNO->accel_z = accel_z;
	telemetryBNO->giro_x = gyro_x;
	telemetryBNO->giro_y = gyro_y;
	telemetryBNO->giro_z = gyro_z;
}


