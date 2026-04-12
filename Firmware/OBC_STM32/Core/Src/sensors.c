/*
* sensors.c
*
* Created on: Apr 10, 2026
* Author: shuten-doji
*/

#include "sensors.h"
#include "sh2.h"
#include "sh2_SensorValue.h"
#include "sh2_err.h"
#include "telemetry.h"
#include <math.h>

extern I2C_HandleTypeDef hi2c1;

// --- Direcciones I2C ---

#define BME280_ADDR (0x76 << 1)

// --- Memoria de Calibración BME280 ---

static uint16_t dig_T1, dig_P1;
static int16_t dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static int32_t t_fine;

// --- Variables de Trabajo (Escalares) ---
static float temperature, pressure, altitude;

// --- Datos del BNO085 ---

static float last_accel[3], last_gyro[3];
static sh2_SensorValue_t sensorValue;

// --- Handler de Eventos BNO085 ---
static void event_handler(void * cookie, sh2_AsyncEvent_t *event) {

    // Aplicamos el cast (const sh2_SensorEvent_t *) para que coincida el tipo
    if (sh2_decodeSensorEvent(&sensorValue, (const sh2_SensorEvent_t *)&event->shtpEvent) != SH2_OK) {
        return;
    }



    if (sensorValue.sensorId == SH2_ACCELEROMETER) {
        last_accel[0] = sensorValue.un.accelerometer.x;
        last_accel[1] = sensorValue.un.accelerometer.y;
        last_accel[2] = sensorValue.un.accelerometer.z;
    }

    if (sensorValue.sensorId == SH2_GYROSCOPE_CALIBRATED) {
        last_gyro[0] = sensorValue.un.gyroscope.x;
        last_gyro[1] = sensorValue.un.gyroscope.y;
        last_gyro[2] = sensorValue.un.gyroscope.z;
    }

}



// --- Funciones de Compensación BME280 ---

static int32_t compensate_T(int32_t adc_T) {
    int32_t var1, var2, T;
    var1 = ((((adc_T>>3) - ((int32_t)dig_T1<<1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T>>4) - ((int32_t)dig_T1)) * ((adc_T>>4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T;
}



static uint32_t compensate_P(int32_t adc_P) {
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    return (uint32_t)p;
}



void sensors_init(void) {

    // 1. Reset físico del BNO085 (Vital para liberar el bus I2C)
    HAL_GPIO_WritePin(BNO_RST_GPIO_Port, BNO_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(BNO_RST_GPIO_Port, BNO_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(50); // Tiempo para que el sensor despierte


    // 2. Inicializar BNO085 y capturar el código de error
    // Usamos la variable global de main.c para ver qué pasa en Live Expressions

    extern int g_sh2_open_rc;
    g_sh2_open_rc = sh2_open(NULL, event_handler, NULL);


    sh2_SensorConfig_t config = {.reportInterval_us = 20000}; // 50Hz
    sh2_setSensorConfig(SH2_ACCELEROMETER, &config);
    sh2_setSensorConfig(SH2_GYROSCOPE_CALIBRATED, &config);

    /*
    if (g_sh2_open_rc == SH2_OK) {
    sh2_SensorConfig_t config = {.reportInterval_us = 20000}; // 50Hz
    sh2_setSensorConfig(SH2_ACCELEROMETER, &config);
    sh2_setSensorConfig(SH2_GYROSCOPE_CALIBRATED, &config);
    }*/


    // 2. Inicializar BME280 (Lectura de calibración)
    uint8_t calib[24];
    HAL_I2C_Mem_Read(&hi2c1, BME280_ADDR, 0x88, 1, calib, 24, 100);
    dig_T1 = (calib[1] << 8) | calib[0];
    dig_T2 = (calib[3] << 8) | calib[2];
    dig_T3 = (calib[5] << 8) | calib[4];
    dig_P1 = (calib[7] << 8) | calib[6];
    dig_P2 = (calib[9] << 8) | calib[8];
    dig_P3 = (calib[11] << 8) | calib[10];
    dig_P4 = (calib[13] << 8) | calib[12];
    dig_P5 = (calib[15] << 8) | calib[14];
    dig_P6 = (calib[17] << 8) | calib[16];
    dig_P7 = (calib[19] << 8) | calib[18];
    dig_P8 = (calib[21] << 8) | calib[20];
    dig_P9 = (calib[23] << 8) | calib[22];

    // Configuración básica BME (OverSampling x1)
    uint8_t ctrl = 0x27;
    HAL_I2C_Mem_Write(&hi2c1, BME280_ADDR, 0xF4, 1, &ctrl, 1, 100);
}



void sensors_service(void) {
    sh2_service(); // Procesa BNO085
    // Lectura de datos crudos BME280
    uint8_t data[6];

    if (HAL_I2C_Mem_Read(&hi2c1, BME280_ADDR, 0xF7, 1, data, 6, 10) == HAL_OK) {
        int32_t adc_P = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
        int32_t adc_T = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
        int32_t t = compensate_T(adc_T);
        uint32_t p = compensate_P(adc_P);
        temperature = t / 100.0f;
        pressure = p / 256.0f;
        // Cálculo de altitud simple (p0 = 101325 Pa)
        altitude = 44330.0f * (1.0f - powf(pressure / 101325.0f, 0.1903f));
    }
}



void read_sensors(CanSat_Packet *p) {
    // 1. Usamos tu función de telemetry.c para poner Magic, ID y datos ambientales
    // Convertimos los floats a los tipos que espera la función (int16, uint16, etc)
    telemetry_build(p, (int16_t)(temperature * 100), (uint16_t)pressure, (uint32_t)(altitude * 100));

    // 2. Actualizamos el IMU y calculamos el Checksum automáticamente
    // Esto llamará a tu función calculate_checksum que ya tienes en telemetry.c
    telemetry_update_imu(p, last_accel[0], last_accel[1], last_accel[2],
    last_gyro[0], last_gyro[1], last_gyro[2]);
}

// --- IMPLEMENTACIÓN REAL DE LA HAL PARA BNO085 ---
// 1. El tiempo en microsegundos
uint32_t hal_getTimeUs(sh2_Hal_t *self)
{
    return HAL_GetTick() * 1000;
}

// 2. Escritura I2C standard
int hal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len)
{
    // Usamos 0x4A, si no funciona prueba con (0x4B << 1)
    if (HAL_I2C_Master_Transmit(&hi2c1, (0x4A << 1), pBuffer, len, 100) == HAL_OK)
    {
        return len;
    }
    return 0;
}

// 3. LECTURA CRÍTICA: El BNO085 requiere leer el header primero
int hal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us)
{
    *t_us = hal_getTimeUs(self);

    // PASO A: El pin INT es "active low". Si está en 1 (HIGH), el sensor no tiene datos.
    // Si intentas leer cuando INT está en 1, el I2C dará error o leerás ceros.
    if (HAL_GPIO_ReadPin(BNO_INT_GPIO_Port, BNO_INT_Pin) == GPIO_PIN_SET)
    {
        return 0;
    }

    // PASO B: Leer los primeros 4 bytes para saber cuánto mide el paquete (SHTP Header)
    uint8_t header[4];
    if (HAL_I2C_Master_Receive(&hi2c1, (0x4A << 1), header, 4, 100) != HAL_OK)
    {
        return 0;
    }

    // PASO C: Calcular longitud real (primeros 2 bytes del header)
    uint16_t packet_len = (uint16_t)((header[1] << 8) | header[0]) & 0x7FFF;

    if (packet_len < 4 || packet_len > len)
    {
        return 0;
    }

    // PASO D: Leer el paquete completo
    // El BNO espera que leas 'packet_len' bytes después de detectar el INT
    if (HAL_I2C_Master_Receive(&hi2c1, (0x4A << 1), pBuffer, packet_len, 100) != HAL_OK)
    {
        return 0;
    }

    return packet_len;
}

int hal_open(sh2_Hal_t *self) { return 0; }
void hal_close(sh2_Hal_t *self) {}

// La instancia que el Linker buscaba
sh2_Hal_t sh2_hal = {
    .open = hal_open,
    .close = hal_close,
    .read = hal_read,
    .write = hal_write,
    .getTimeUs = hal_getTimeUs};
