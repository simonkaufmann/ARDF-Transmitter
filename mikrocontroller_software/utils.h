/*
 *  utils.h - Miscellaneous util functions used in the project
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

#ifndef UTILS_H
#define UTILS_H

#define UTILS_STR_EQUAL		0
#define UTILS_STR_CONTAINS	1
#define UTILS_STR_FALSE		2

/* function definitions */
uint8_t int_to_string(char *string, uint8_t length, uint32_t val);
uint8_t int_to_string_fixed_length(char *string, uint8_t length, uint32_t val);
uint8_t int_to_string_hex(char *string, uint8_t length, uint32_t val);
uint8_t str_to_int(char *string, uint32_t *val);
uint8_t str_compare_progmem(const char *string1, uint16_t string2);
void to_upper_case(char *string);


#endif
