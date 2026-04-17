/*
 * sx1278.h
 *
 *  Created on: Mar 14, 2026
 *      Author: shuten-doji
 */

#ifndef INC_SX1278_H_
#define INC_SX1278_H_


void sx1278_init(void);
void sx1278_send(uint8_t *data, uint16_t len);
void sx1278_onDio0Irq(void);
uint8_t sx1278_read(uint8_t reg);

#endif /* INC_SX1278_H_ */
