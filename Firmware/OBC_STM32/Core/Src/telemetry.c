/*
 * telemetry.c
 *
 *  Created on: Apr 10, 2026
 *      Author: shuten-doji
 */
#include "telemetry.h"

static uint8_t global_pkt_id = 0;

static uint8_t calculate_checksum(uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) crc ^= data[i];
    return crc;
}

void telemetry_build(CanSat_Packet *p, int16_t t, uint16_t pr, uint32_t alt) {
    p->magic = 0xCA;
    p->pkt_id = global_pkt_id++;
    p->temp = t;
    p->pressure = pr;
    p->altitude = alt;
}

void telemetry_update_imu(CanSat_Packet *p, float ax, float ay, float az, float gx, float gy, float gz) {
    // Escalamos floats a int16 para reducir el tamaño del paquete
    p->accel_x = (int16_t)(ax * 100);
    p->accel_y = (int16_t)(ay * 100);
    p->accel_z = (int16_t)(az * 100);
    p->gyro_x  = (int16_t)(gx * 100);
    p->gyro_y  = (int16_t)(gy * 100);
    p->gyro_z  = (int16_t)(gz * 100);

    // El checksum se calcula al final de llenar todo, antes de enviar
    p->checksum = calculate_checksum((uint8_t*)p, sizeof(CanSat_Packet) - 1);
}
