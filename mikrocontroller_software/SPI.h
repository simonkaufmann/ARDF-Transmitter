/*
 *  SPI.h - provide some arduino like spi functions - compatibility layer for
 *		rfid arduino library, functions are ported from C++ to C style.
 *  Copyright (C) 2016  Simon Kaufmann, HeKa
 *
 *  This file is part of ADRF transmitter firmware.
 *
 *  ADRF transmitter firmware is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ADRF transmitter firmware is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ADRF transmitter firmware.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SPI_H
#define SPI_H

#include <avr/io.h>

#define SPI_ON		1
#define SPI_OFF		0

#define MSBFIRST 1
#define SPI_MODE0 0

void SPI_begin(void);				/* these three functions are ignored -> SPI initialisation is done in main.c */
void SPI_setBitOrder(int x);
void SPI_setDataMode(int x);
uint8_t SPI_transfer(uint8_t x);

/* not arduino like functions but own extensions: */
void SPI_set_state(uint8_t state);
uint8_t SPI_get_state(void);
uint8_t SPI_is_free(void);

#endif
