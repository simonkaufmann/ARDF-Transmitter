/*
 *  user.h - definitions for functions of user management
 *  Copyright (C) 2016  Simon Kaufmann, HeKa
 *
 *  This file is part of ADRF transmitter firmware.
 *
 *  ADRF transmitter firmware is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ADRF transmitter firmware is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ADRF transmitter firmware.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef USER_H
#define USER_H

#ifdef NEW_PROTOTYPE
#define RFID_LED_ON()		RFID_PORT |= (1 << RFID_LED)
#define RFID_LED_OFF()	RFID_PORT &= ~(1 << RFID_LED)
#define RFID_LED_TOGGLE()		RFID_PORT ^= (1 << RFID_LED)
#else
#define RFID_LED_ON()		LED_PORT |= (1 << LED)
#define RFID_LED_OFF()	LED_PORT &= ~(1 << LED)
#define RFID_LED_TOGGLE()		LED_PORT ^= (1 << LED)
#endif



void user_init(void);
void user_show_configuration(void);

void user_start_time(void);
void user_stop_time(void);

uint8_t user_new_tag(void);

void user_time_tick(void);

uint8_t user_set_id(uint16_t tag_id);
void user_cancel_set_id(void);
uint8_t user_check_set_id_state(void);

uint8_t user_set_secret(uint16_t secret);
uint16_t user_get_secret(void);

void user_clear_history(void);

void user_print_fox_history(void);

#endif
