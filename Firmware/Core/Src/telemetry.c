/*
 * telemetry.c
 *
 *  Created on: Mar 13, 2026
 *      Author: shuten-doji
 */
#include "telemetry.h"
#include "main.h"
#include "sh2.h"
#include <stdint.h>

static uint16_t pkt = 0;


// Aqui contruimos el paquete de telemetria LoRa
void telemetry_build_LoRa(TelemetryPacketLoRa *protocolLoRa, uint16_t header, uint16_t temperatura, uint16_t presion, uint16_t humidity, uint16_t altitud, uint16_t accel_y, uint16_t trigger)
{
	protocolLoRa->header = header;
	protocolLoRa->pkt = pkt++;
	protocolLoRa->temperatura = temperatura;
	protocolLoRa->presion = presion;
	protocolLoRa->humidity = humidity;
	protocolLoRa->altitud = altitud;
	protocolLoRa->accel_y = accel_y;
	protocolLoRa->trigger = trigger;
}




