/*
 *  rfid.h - definitions for rfid system and participant registration system
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

#ifndef RFID_H
#define RFID_H

#include "MFRC522.h"

#define RFID_BLOCK_SIZE	16

void rfid_init(void);

void rfid_loop(void);

uint8_t rfid_read_block(uint8_t block, uint8_t *buff, uint8_t *length);
uint8_t rfid_write_block(uint8_t block, uint8_t *buff, uint8_t length);

void rfid_close_tag(void);



#endif
