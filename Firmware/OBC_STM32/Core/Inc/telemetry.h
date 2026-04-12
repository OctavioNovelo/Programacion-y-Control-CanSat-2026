/*
 * telemetry.h
 *
 *  Created on: Apr 10, 2026
 *      Author: shuten-doji
 */

#ifndef INC_TELEMETRY_H_
#define INC_TELEMETRY_H_

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t  magic;        // 0xCA para identificar nuestros paquetes
    uint8_t  pkt_id;       // Contador de paquetes
    int16_t  temp;         // Temperatura (centígrados)
    uint16_t pressure;     // Presión (Pa / 100)
    uint32_t altitude;     // Altitud calculada
    int16_t  accel_x;      // Aceleración escalada (v * 100)
    int16_t  accel_y;
    int16_t  accel_z;
    int16_t  gyro_x;       // Giroscopio escalado
    int16_t  gyro_y;
    int16_t  gyro_z;
    uint8_t  status;       // Flags de estado (BME_OK, BNO_OK, etc)
    uint8_t  checksum;     // Verificación de integridad
} CanSat_Packet;

void telemetry_init(void);
void telemetry_build(CanSat_Packet *p, int16_t t, uint16_t pr, uint32_t alt);
void telemetry_update_imu(CanSat_Packet *p, float ax, float ay, float az, float gx, float gy, float gz);

#endif /* INC_TELEMETRY_H_ */
