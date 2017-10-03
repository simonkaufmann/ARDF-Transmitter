/*
 *  morse.h - definitions of functions for storing the morse information
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

#ifndef MORSE_H
#define MORSE_H

#define FOX_MAX_MAX		5
#define FOX_MAX_MIN		1
#define FOX_MAX_DEFAULT	1

#define FOX_NUMBER_MAX	5
#define FOX_NUMBER_FIRST	1
#define FOX_NUMBER_DEMO	0

#define TRANSMIT_MINUTE_MAX		4

#define MORSE_WPM_MAX		60
#define MORSE_WPM_MIN		5
#define MORSE_WPM_DEFAULT	10

/*
 * do not change order
 * it must match with the order in the calls-array of commands.c
 */
#define CALL_MOE	0
#define CALL_MOI	1
#define CALL_MOS	2
#define CALL_MOH	3
#define CALL_MO5	4
#define CALL_MO		5
#define CALL_MAX	5

void morse_init(void);
void morse_show_configuration(void);
void morse_load_configuration(void);

uint16_t morse_convert_wpm_morse_unit(uint16_t wpm);
uint16_t morse_convert_morse_unit_wpm(uint16_t morse_unit);

void morse_set_morse_unit(uint16_t morse_unit_value);
uint16_t morse_get_morse_unit(void);

uint8_t morse_set_fox_number(uint8_t fox_number);
uint8_t morse_get_fox_number(void);

uint8_t morse_set_fox_max(uint8_t max);
uint8_t morse_get_fox_max(void);

uint8_t morse_set_transmit_minute(uint8_t transmit_minute);
uint8_t morse_get_transmit_minute(void);

void morse_set_call_sign(uint8_t call);
uint8_t morse_get_call_sign(void);

void morse_set_morse_mode(uint8_t mode);
uint8_t morse_get_morse_mode(void);

void morse_enable_continuous_carrier(void);
void morse_disable_continuous_carrier(void);

void morse_start_time(void);
void morse_stop_time(void);

void morse_interrupt(void);

#endif
