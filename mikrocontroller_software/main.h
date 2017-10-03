/*
 *  main.h - miscellaneous definitions and declarations for ARDF transmitter
 *           firmware used all over the project
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

#ifndef MAIN_H
#define MAIN_H

/*
 * 8e6MHz / 1024 * (500ms / 1000ms) -> preload value for 500ms overflow time
 * 1024 = prescaler
 */
#define TIMER1_MS		50
#define TIMER1_PRELOAD	(65536 - F_CPU * TIMER1_MS / 1000 / 1024)

#define LED_ON()			LED_PORT |= (1 << LED)
#define LED_TOGGLE()		LED_PORT ^= (1 << LED)
#define LED_OFF()			LED_PORT &= ~(1 << LED)

#define TRUE				1
#define FALSE				0

void main_init(void);

void main_reload(void);

void main_start_time(void);
void main_stop_time(void);

void main_set_blinking(uint8_t mode);

#endif
