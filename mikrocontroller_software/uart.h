/*
 *  uart.h - definition of functions for UART - operations
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

#ifndef UART_H
#define UART_H

#define BAUDRATE	38400UL //9600UL
#define UBRR_SETTING	(2 * F_CPU / (16UL * BAUDRATE) - 1)

#define UART_NEWLINE()		uart_send_text_sram("\r\n")

/* function definitions */
void uart_init(void);

void uart_send_text_sram(const char *text);
void uart_send_binary_sram(uint8_t number, uint8_t *data);

void uart_send_text_flash(uint16_t text);
void uart_send_binary_flash(uint8_t number, uint16_t data);

void uart_send_text_eeprom(const char *text);
void uart_send_binary_eeprom(uint8_t number, uint8_t *data);

void uart_send_text_buffer(const char *text);
void uart_send_binary_buffer(uint8_t number, uint8_t *data);

uint8_t uart_receive_buffer_text(char **text);

uint8_t uart_send_int(uint32_t val);
uint8_t uart_send_int_hex(uint32_t val);

#endif
