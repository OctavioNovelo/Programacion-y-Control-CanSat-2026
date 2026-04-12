/*
 * sx1278.c
 *
 *  Created on: Apr 10, 2026
 *      Author: shuten-doji
 */
#include "sx1278.h"

static SPI_HandleTypeDef *sx1278_hspi;

static void sx1278_write(uint8_t reg, uint8_t val) {
    uint8_t data[2] = { reg | 0x80, val };
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(sx1278_hspi, data, 2, 100);
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);
}

void sx1278_init(SPI_HandleTypeDef *hspi) {
    sx1278_hspi = hspi;

    // Reset físico
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10);

    sx1278_write(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
    HAL_Delay(10);

    // Configuración de frecuencia (ejemplo 433MHz)
    sx1278_write(REG_FRF_MSB, 0x6C);
    sx1278_write(REG_FRF_MID, 0x40);
    sx1278_write(REG_FRF_LSB, 0x00);

    // Potencia máxima (PA_BOOST)
    sx1278_write(REG_PA_CONFIG, 0xFF);

    // Mapeo de DIO0 para que se active en "TxDone"
    sx1278_write(REG_DIO_MAPPING_1, 0x40);

    sx1278_write(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

void sx1278_transmit_it(uint8_t *data, uint8_t size) {
    sx1278_write(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);

    // Resetear puntero de FIFO
    sx1278_write(REG_FIFO_ADDR_PTR, 0x00);
    sx1278_write(REG_FIFO_TX_BASE_ADDR, 0x00);

    // Escribir payload
    for(uint8_t i = 0; i < size; i++) {
        sx1278_write(REG_FIFO, data[i]);
    }

    // Disparar transmisión
    sx1278_write(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);
}

void sx1278_clear_irq(void) {
    sx1278_write(REG_IRQ_FLAGS, 0xFF); // Limpia todas las banderas
}

uint8_t sx1278_read_reg(uint8_t reg) {
    uint8_t val = 0;
    uint8_t addr = reg & 0x7F; // Bit 7 en 0 para lectura

    // Usamos LORA_NSS_GPIO_Port y LORA_NSS_Pin para ser consistentes
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_RESET);

    // Usamos sx1278_hspi que es el puntero que inicializamos en sx1278_init
    HAL_SPI_Transmit(sx1278_hspi, &addr, 1, 100);
    HAL_SPI_Receive(sx1278_hspi, &val, 1, 100);

    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);

    return val;
}
