/*
 *  Arduino.c - provide some basic arduino functions, definitions and data types
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

#include "Arduino.h"
#include "main.h"
#include "pins.h"
#include "util/delay.h"
#include "SPI.h"
#include "uart.h"
#include "dds.h"

/* serial-class functions */
void Serial_println(void)
{
	uart_send_text_sram("\r\n");
}

void Serial_println_int(unsigned int data)
{
	Serial_print_number(data, DEC);
	Serial_println();
}

void Serial_println_string(const char* string)
{
	Serial_print_string(string);
	Serial_println();
}

void Serial_println_number(unsigned int data, unsigned int mode)
{
	Serial_print_number(data, mode);
	Serial_println();
}

void Serial_println_flashstring(const __FlashStringHelper* dat)
{
	Serial_print_flashstring(dat),
	Serial_println();
}

void Serial_print_string(const char* string)
{
	uart_send_text_sram(string);
//	uint16_t i = 0;
//	wait_uart();
//	while (string[i] != 0)	{
//		UDR = string[i];
//		wait_uart();
//		i++;
//		if (i==0)	{
//			break; /* overflow happened */
//		}
//	}
}

void Serial_print_flashstring(const __FlashStringHelper *ifsh)
{
	const char * __attribute__((progmem)) p = (const char * ) ifsh;
// while (1) {
	uart_send_text_flash((uint16_t)p);
//    unsigned char c = pgm_read_byte(p);
//	p++;
//    if (c == 0) break;
//	UDR = c;
//	wait_uart();
//	n++;
//  }
}

void Serial_print_number(unsigned int data, unsigned int mode)
{
	if (mode == DEC)	{
		uart_send_int(data);
	} else if (mode == HEX)	{
		uart_send_int_hex(data);
	}
}

void Serial_print_byte(byte data)
{
	Serial_print_number((unsigned int) data, DEC);
}

/* standard arduino functions */
void digitalWrite(uint8_t pin, uint8_t state)
{
	if (pin == SS_PIN)	{
		if (state == HIGH)	{
			RFID_PORT |= (1 << RFID_CS);
			dds_execute_command_buffer(); /* execute all the dds commands that
										   * failed while rfid was sending
										   */
		} else if (state == LOW)	{
			if ((RFID_PORT & (1 << RFID_CS)) > 0)	{
#ifdef CS_LED
			LED_ON();
#endif
				while (SPI_get_state() != SPI_ON);
#ifdef CS_LED
			LED_OFF();
#endif
				/* not valid: only wait for spi if RFID_CS is high at the moment,
				 * otherwise controller could hang!
				 */
			}
			RFID_PORT &= ~(1 << RFID_CS);
		}
	} else if (pin == RST_PIN)	{
		if (state == HIGH)	{
			RFID_PORT |= (1 << RFID_RESET);
		} else if (state == LOW)	{
			RFID_PORT &= ~(1 << RFID_RESET);
		}
	}
}

uint8_t digitalRead(uint8_t pin)
{
	if (pin == RST_PIN)	{
		return ((RFID_PIN & (1 << RFID_RESET)) >> RFID_RESET);
	}
	return 0;
}

void pinMode(uint8_t pin, uint8_t mode)
{
	if (pin == RST_PIN)	{
		if (mode == OUTPUT)	{
			RFID_DDR |= (1 << RFID_RESET);
		} else if (mode == INPUT)	{
			RFID_DDR &= ~(1 << RFID_RESET);
		}
	} else if (pin == SS_PIN)	{
		if (mode == OUTPUT)	{
			RFID_DDR |= (1 << RFID_CS);
		} else if (mode == INPUT)	{
			RFID_DDR &= (1 << RFID_CS);
		}
	}
}

void delay(uint16_t ms)
{
	uint8_t i;
	for (i = 0; i < ms; i++)	{
		_delay_ms(1);
	}
}

void Serial_begin(int x)
{

}
