/*
 * sx1278.h
 *
 *  Created on: Mar 14, 2026
 *      Author: shuten-doji
 */

#ifndef INC_SX1278_H_
#define INC_SX1278_H_


void sx1278_init(void); // Funcion para iniciar LoRa
void sx1278_send(uint8_t *data, uint16_t len); // Funcion para enviar LoRa
void sx1278_onDio0Irq(void); // Funciona para ...
uint8_t sx1278_read(uint8_t reg); // Funcion para ...
void sx1278_write(uint8_t reg, uint8_t value); // Funcion para ...
void sx1278_reset(void); // Funcion para reiniciar el sx1278
uint8_t sx1278_check(void); // Funcion para checar la version del modulo

#endif /* INC_SX1278_H_ */
