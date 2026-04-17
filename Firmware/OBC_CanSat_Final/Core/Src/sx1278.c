/*
 * sx1278.c
 *
 *  Created on: Mar 14, 2026
 *      Author: shuten-doji
 */
#include "main.h"
#include "sx1278.h"

extern SPI_HandleTypeDef hspi1;

#define SX1278_CS_PORT LORA_NSS_GPIO_Port
#define SX1278_CS_PIN  LORA_NSS_Pin

#define SX1278_RESET_PORT LORA_RST_GPIO_Port
#define SX1278_RESET_PIN  LORA_RST_Pin

volatile uint8_t sx1278_tx_done = 0;

static void cs_low()
{
    HAL_GPIO_WritePin(SX1278_CS_PORT, SX1278_CS_PIN, GPIO_PIN_RESET);
}

static void cs_high()
{
    HAL_GPIO_WritePin(SX1278_CS_PORT, SX1278_CS_PIN, GPIO_PIN_SET);
}

void sx1278_write(uint8_t reg, uint8_t value)
{
    uint8_t buffer[2];

    buffer[0] = reg | 0x80;
    buffer[1] = value;

    cs_low();
    HAL_SPI_Transmit(&hspi1, buffer, 2, 100);
    cs_high();
}

uint8_t sx1278_read(uint8_t reg)
{
    uint8_t tx = reg & 0x7F;
    uint8_t rx;

    cs_low();
    HAL_SPI_Transmit(&hspi1, &tx, 1, 100);
    HAL_SPI_Receive(&hspi1, &rx, 1, 100);
    cs_high();

    return rx;
}

void sx1278_reset()
{
    HAL_GPIO_WritePin(SX1278_RESET_PORT, SX1278_RESET_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);

    HAL_GPIO_WritePin(SX1278_RESET_PORT, SX1278_RESET_PIN, GPIO_PIN_SET);
    HAL_Delay(10);
}

void sx1278_init()
{
    sx1278_reset();

    // sleep
    sx1278_write(0x01, 0x80);

    HAL_Delay(10);

    // LoRa standby
    sx1278_write(0x01, 0x81);

    // frecuencia 433 MHz
    sx1278_write(0x06, 0x6C);
    sx1278_write(0x07, 0x80);
    sx1278_write(0x08, 0x00);

    // PA config
    sx1278_write(0x09, 0x8F);

    // LNA
    sx1278_write(0x0C, 0x23);

    // FIFO base
    sx1278_write(0x0E, 0x00);
    sx1278_write(0x0F, 0x00);

    // idk
    // DIO0 = TX_DONE
    sx1278_write(0x40, 0x40);

    // BW 125kHz SF7 CR 4/5
    sx1278_write(0x1D, 0x72);
    sx1278_write(0x1E, 0x74);

    // preamble
    sx1278_write(0x20, 0x00);
    sx1278_write(0x21, 0x08);

    // sync word
    sx1278_write(0x39, 0x34);

    sx1278_write(0x0E, 0x00); // TX base addr
    sx1278_write(0x0F, 0x00); // RX base addr
}

void sx1278_send(uint8_t *data, uint16_t len)
{
    sx1278_tx_done = 0;

    // standby
    sx1278_write(0x01, 0x81);

    // limpiar IRQ flags
    sx1278_write(0x12, 0xFF);

    // FIFO ptr
    sx1278_write(0x0D, 0x00);

    // escribir payload
    for(int i = 0; i < len; i++)
        sx1278_write(0x00, data[i]);

    // tamaño payload
    sx1278_write(0x22, len);

    // modo TX
    sx1278_write(0x01, 0x83);
}

void sx1278_onDio0Irq(void)
{
    sx1278_tx_done = 1;
}

uint8_t sx1278_check(void)
{
    uint8_t version = sx1278_read(0x42);
    return version;
}
