/*
 *  Arduino.h - provide some basic arduino functions, definitions and data types
 *			compatibility layer for rfid arduino library, functions are
 *			ported from C++ to C style.
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

#ifndef ARDUINO_H
#define ARDUINO_H

/* my own compatibility arduino.h */

#include <stddef.h> /* for NULL-constant */
#include <string.h> /* for memcpy */

#include <avr/pgmspace.h> /* for PROGMEM */

#define F(string_literal) ((const __FlashStringHelper *)PSTR(string_literal))

/* for digitalWrite */
#define LOW		0
#define HIGH	1

/* for pinMode */
#define OUTPUT	1
#define INPUT	0

/* for print output type */
#define HEX		0
#define DEC		1

#define SS_PIN 10
#define RST_PIN 9

#define false	0
#define true	1

typedef uint8_t byte;
typedef uint16_t word;
typedef uint8_t bool;


//class __FlashStringHelper;
typedef const char * __FlashStringHelper; /* refer to Arduino.c -> the cast is to const char* */

void Serial_begin(int x); /* ignore uart initialisation is done in main.c */
void Serial_println(void);
void Serial_println_int(unsigned int x);
void Serial_println_string(const char* chr);
void Serial_println_flashstring(const __FlashStringHelper* dat);
void Serial_println_number(unsigned int data, unsigned int mode);
void Serial_print_string(const char* chr);
void Serial_print_flashstring(const __FlashStringHelper* dat);
void Serial_print_number(unsigned int data, unsigned int mode);
void Serial_print_byte(byte point); /* byte gives also human readable ascii, all print give human readable ascii! (otherwise binary data use write!) */

void digitalWrite(uint8_t pin, uint8_t state);
uint8_t digitalRead(uint8_t pin);
void pinMode(uint8_t pin, uint8_t mode);

void delay(uint16_t ms);

#endif
