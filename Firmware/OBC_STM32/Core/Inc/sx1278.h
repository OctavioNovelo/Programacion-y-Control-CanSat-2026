/*
 * sx1278.h
 *
 *  Created on: Apr 10, 2026
 *      Author: shuten-doji
 */

#ifndef INC_SX1278_H_
#define INC_SX1278_H_

#include "main.h"

// Registros LoRa básicos
#define REG_FIFO                    0x00
#define REG_OP_MODE                 0x01
#define REG_FRF_MSB                 0x06
#define REG_FRF_MID                 0x07
#define REG_FRF_LSB                 0x08
#define REG_PA_CONFIG               0x09
#define REG_FIFO_ADDR_PTR           0x0D
#define REG_FIFO_TX_BASE_ADDR       0x0E
#define REG_IRQ_FLAGS               0x12
#define REG_MODEM_CONFIG_1          0x1D
#define REG_MODEM_CONFIG_2          0x1E
#define REG_DIO_MAPPING_1           0x40
#define REG_VERSION                 0x42

// Modos de operación
#define MODE_LONG_RANGE_MODE        0x80
#define MODE_SLEEP                  0x00
#define MODE_STDBY                  0x01
#define MODE_TX                     0x03

void sx1278_init(SPI_HandleTypeDef *hspi);
void sx1278_transmit_it(uint8_t *data, uint8_t size);
void sx1278_clear_irq(void);
uint8_t sx1278_read_reg(uint8_t reg);
#endif
