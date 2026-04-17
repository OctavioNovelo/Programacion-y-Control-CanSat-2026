/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Ultra-Fast Telemetry Diagnostic Build
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "crc.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "sh2.h"
#include "sh2_hal.h"
#include "sh2_SensorValue.h"
#include "telemetry.h"
#include "sx1278.h"

/* ---- Configuración de Rendimiento ---- */
#define LORA_POWER_LEVEL    0x82            /* Potencia baja-media */
#define LORA_INTERVAL_MS    50              /* Intentar 20 Hz de radio */

/* USER CODE BEGIN PV */
extern volatile uint8_t sx1278_tx_done;
volatile uint32_t loop_speed_check = 0;     /* INCREMENTA EN CADA VUELTA DEL WHILE(1) */
volatile uint32_t last_tx_tick = 0;
volatile uint32_t debug_tx_interval = 0;

volatile uint8_t  init_bme = 0, init_bno = 0;
volatile int32_t  temperature = 0;
volatile uint32_t pressure = 0, altitude = 0, tx_count = 0;
volatile float    accel_x=0, accel_y=0, accel_z=0, gyro_x=0, gyro_y=0, gyro_z=0;
volatile float    pressure_init = 0.0f, altitude_filtered = 0.0f;
volatile uint8_t  is_calibrated = 0, apogeo_detectado = 0, trigger_activado = 0;
volatile int      g_sh2_open_rc = 999;

volatile TelemetryPacketLoRa g_last_lora_packet;
volatile PacketLoRaBNO       g_last_bno_packet;
volatile uint8_t             g_lora_ready = 1;

static volatile uint8_t  bno_int_flag = 0;
static volatile uint32_t bno_int_time_us = 0;
/* USER CODE END PV */

/* Prototypes */
void SystemClock_Config(void);
static void lora_fast_init(void);
static void lora_send_async(void);
static void dwt_init(void);
static uint32_t micros32(void);

static void dwt_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
static uint32_t micros32(void) {
    return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000U));
}

/* ================================================================
 *  BNO085 HAL
 * ================================================================ */
void sh2_event_cb(void *cookie, sh2_AsyncEvent_t *pEvent) { (void)cookie; (void)pEvent; }
static void leer_bno(void *cookie, sh2_SensorEvent_t *pEvent) {
    sh2_SensorValue_t v;
    if (sh2_decodeSensorEvent(&v, pEvent) == 0) {
        init_bno = 1;
        if (v.sensorId == SH2_ACCELEROMETER) {
            accel_x = v.un.accelerometer.x; accel_y = v.un.accelerometer.y; accel_z = v.un.accelerometer.z;
        } else if (v.sensorId == SH2_GYROSCOPE_CALIBRATED) {
            gyro_x = v.un.gyroscope.x; gyro_y = v.un.gyroscope.y; gyro_z = v.un.gyroscope.z;
        }
    }
}
static int sh2hal_open(sh2_Hal_t *s) {
    dwt_init();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, 0); HAL_Delay(10); HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, 1); HAL_Delay(50);
    return 0;
}
static int sh2hal_read(sh2_Hal_t *s, uint8_t *p, unsigned l, uint32_t *t) {
    if (!bno_int_flag && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == GPIO_PIN_SET) return 0;
    uint8_t h[4];
    if (HAL_I2C_Master_Receive(&hi2c1, (0x4B<<1), h, 4, 5) != HAL_OK) return 0;
    uint16_t len = (uint16_t)h[0] | ((uint16_t)(h[1] & 0x7F) << 8);
    if (len < 4 || len > l) return 0;
    if (HAL_I2C_Master_Receive(&hi2c1, (0x4B<<1), p, len, 10) != HAL_OK) return 0;
    bno_int_flag = 0; *t = bno_int_time_us; return (int)len;
}
static int sh2hal_write(sh2_Hal_t *s, uint8_t *p, unsigned l) {
    if (HAL_I2C_Master_Transmit(&hi2c1, (0x4B<<1), p, l, 10) != HAL_OK) return 0;
    return (int)l;
}
static uint32_t sh2hal_getTimeUs(sh2_Hal_t *s) {
    (void)s;
    return micros32();
}
sh2_Hal_t g_sh2_hal = { .open=sh2hal_open, .read=sh2hal_read, .write=sh2hal_write, .getTimeUs=sh2hal_getTimeUs };

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == LORA_DIO0_Pin) sx1278_tx_done = 1;
    if (GPIO_Pin == GPIO_PIN_8) { bno_int_flag = 1; bno_int_time_us = micros32(); }
}

/* ================================================================
 *  LoRa Fast Config
 * ================================================================ */
static void lora_fast_init(void) {
    sx1278_init();
    sx1278_write(0x01, 0x80); // Sleep
    sx1278_write(0x01, 0x81); // Standby
    sx1278_write(0x1D, 0x72); // BW 125kHz
    sx1278_write(0x1E, 0x74); // SF7, CRC On
    sx1278_write(0x26, 0x04); // LowDataRateOptimize Off
    sx1278_write(0x09, LORA_POWER_LEVEL); 
}

/* ================================================================
 *  MAIN
 * ================================================================ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_SPI1_Init();

    /* Blink rápido de vida */
    for(int i=0; i<6; i++) { HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_4); HAL_Delay(30); }

    lora_fast_init();
    sh2_open(&g_sh2_hal, sh2_event_cb, NULL);
    sh2_setSensorCallback(leer_bno, NULL);
    sh2_SensorConfig_t cfg = { .reportInterval_us = 10000 };
    sh2_setSensorConfig(SH2_ACCELEROMETER, &cfg);
    sh2_setSensorConfig(SH2_GYROSCOPE_CALIBRATED, &cfg);

    uint32_t t_lora = HAL_GetTick();
    uint32_t t_bme = HAL_GetTick();

    while (1)
    {
        loop_speed_check++; /* SI ESTE VALOR SUBE RAPIDO, EL CPU ESTA BIEN */
        
        sh2_service(); 

        uint32_t t_now = HAL_GetTick();

        /* Lectura BME280 cada 100ms */
        if (t_now - t_bme >= 100) {
            t_bme = t_now;
            uint8_t d[6];
            if (HAL_I2C_Mem_Read(&hi2c1, (0x76<<1), 0xF7, 1, d, 6, 5) == HAL_OK) {
                uint32_t p_raw = (uint32_t)((d[0]<<12)|(d[1]<<4)|(d[2]>>4));
                pressure = p_raw / 100;
                if (!is_calibrated && p_raw > 0) { pressure_init = (float)p_raw; is_calibrated = 1; }
                if (is_calibrated) {
                    float al = 44330.0f * (1.0f - powf((float)p_raw/pressure_init, 0.1903f));
                    altitude = (uint32_t)al;
                }
                init_bme = 1;
            }
        }

        /* Lógica LoRa */
        if (t_now - t_lora >= LORA_INTERVAL_MS) {
            if (g_lora_ready) {
                debug_tx_interval = t_now - last_tx_tick;
                last_tx_tick = t_now;
                t_lora = t_now;
                lora_send_async();
            }
        }
        
        /* Polling de radio */
        if (g_lora_ready == 0) {
            if ((sx1278_read(0x12) & 0x08) || sx1278_tx_done) {
                sx1278_write(0x12, 0xFF);
                sx1278_tx_done = 0;
                g_lora_ready = 1;
            }
        }

        /* Fuerza al compilador a no optimizar estas variables */
        if (loop_speed_check > 0xFFFFFFFE) loop_speed_check = 0;
        if (debug_tx_interval > 0xFFFFFFFE) debug_tx_interval = 0;
    }
}

static void lora_send_async(void) {
    static uint8_t buf[sizeof(TelemetryPacketLoRa) + sizeof(PacketLoRaBNO)];
    TelemetryPacketLoRa pL;
    telemetry_build_LoRa(&pL, (int16_t)25, (uint16_t)altitude, (uint16_t)pressure, init_bme);
    PacketLoRaBNO pB;
    telemetry_LoRa_BNO(&pB, accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z);
    memcpy(buf, &pL, sizeof(pL)); memcpy(buf+sizeof(pL), &pB, sizeof(pB));
    
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_4);
    g_lora_ready = 0;
    sx1278_send(buf, sizeof(buf));
    tx_count++;
}

void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 12;
    RCC_OscInitStruct.PLL.PLLN = 192; /* 96MHz real */
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3);
}

void Error_Handler(void) { while(1); }
