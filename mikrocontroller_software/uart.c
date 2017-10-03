/*
 *  uart.c - functions for UART - operations
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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

#include "uart.h"
#include "utils.h"
#include "pins.h"
#include "main.h"

#define UART_SEND_BUFFER_NUMBER		10
#define UART_SEND_BUFFER_LENGTH		10

#define UART_RECEIVE_BUFFER_NUMBER	30
#define UART_RECEIVE_BUFFER_LENGTH	40

/* uart send buffer types: */
#define IN_BUFFER		0
#define IN_SRAM			1
#define IN_EEPROM		2
#define IN_FLASH		3

/* variable declaration */
uint8_t uart_send_buffer[UART_SEND_BUFFER_NUMBER][UART_SEND_BUFFER_LENGTH];
uint8_t uart_send_buffer_type[UART_SEND_BUFFER_NUMBER][2];
	/*
	 *  uart_send_buffer_type:
	 *  first byte:	 if 0 -> ascii data, if anything else -> number of binary bytes
	 *  second byte: see uart send buffer type - defines -> where the data is stored
	 */
uint16_t uart_send_buffer_flash_pointer[UART_SEND_BUFFER_NUMBER]; /* pointer for flash and eeprom */
uint8_t* uart_send_buffer_pointer[UART_SEND_BUFFER_NUMBER]; /* pointer for char, eeprom (and flash?) */
volatile uint8_t uart_send_buffer_count = 0; /* indicates the index of the next free element in the uart send buffer */
volatile uint8_t uart_send_buffer_count_send = 0; /* indicates the index of the current in sending element in uart send buffer or is equal to uart_send_buffer_count if nothin has to be sent */
volatile uint16_t uart_send_buffer_string_index = 0; /* indicates the index of the next char to be sent in the actual string that is sended by the ISR */
volatile uint8_t uart_is_sending = 0; /* indicates wheter uart is sending at the moment or if it is not sending (then the buffer is empty) */

volatile uint8_t uart_receive_buffer[UART_RECEIVE_BUFFER_NUMBER][UART_RECEIVE_BUFFER_LENGTH];
volatile uint8_t uart_receive_buffer_count = 0; /* index of the next free element in receive buffer */
volatile uint8_t uart_receive_buffer_count_receive = 0; /* index of the next element to read by the program in buffer */
volatile uint8_t uart_receive_buffer_count_string_index = 0; /* indicates the index of the next free char variable in the buffer array where the next received byte will be stored */

/*
 * internal functions
 */

/**
 * uart_send_buffer_cont - continue sending data of the uart_send_buffer
 *
 *		This function is called by the ISR of the UART Transmit Complete-
 *		Interrupt to continue transmission and it is called by the
 *		higher level functions like uart_send_text_sram() (via the
 *		increment_uart_send_buffer()-function) to initiate a transmission
 *
 *		Function is for internal use only
 */
static void uart_send_buffer_cont(void)
{
	uint8_t ready = FALSE;
	/* only for internal use in uart_send_text and in the isr USART_TX_vect */
	if (uart_is_sending > 0 && (UCSR0A & (1 << UDRE0)) > 0)	{
		if (uart_send_buffer_type[uart_send_buffer_count_send][0] == 0)	{
			/* ascii data is to	 be sent */
			uint8_t byte = 0;
			if (uart_send_buffer_type[uart_send_buffer_count_send][1] == IN_BUFFER)	{
				byte = uart_send_buffer[uart_send_buffer_count_send][uart_send_buffer_string_index];
			} else if (uart_send_buffer_type[uart_send_buffer_count_send][1] == IN_SRAM)	{
				byte = uart_send_buffer_pointer[uart_send_buffer_count_send][uart_send_buffer_string_index];
			} else if (uart_send_buffer_type[uart_send_buffer_count_send][1] == IN_EEPROM)	{
				byte = eeprom_read_byte(uart_send_buffer_pointer[uart_send_buffer_count_send]+uart_send_buffer_string_index);
			} else if (uart_send_buffer_type[uart_send_buffer_count_send][1] == IN_FLASH)	{
				byte = pgm_read_byte(uart_send_buffer_flash_pointer[uart_send_buffer_count_send]+uart_send_buffer_string_index);
			}

			if (byte != 0)	{
				UDR0 = byte;
				uart_send_buffer_string_index++;
				if (uart_send_buffer_string_index == 0)	{
					/* overflow from uart_send_buffer_string_index -> finish sending otherwise it would be infinite loop */
					ready = TRUE;
				}
			}
			else	{
				ready = TRUE;
			}
		} else	{
			/* binary data is to be sent */
			uint8_t byte = 0;
			if (uart_send_buffer_type[uart_send_buffer_count_send][1] == IN_BUFFER)	{
				byte = uart_send_buffer[uart_send_buffer_count_send][uart_send_buffer_string_index];
			}
			else if (uart_send_buffer_type[uart_send_buffer_count_send][1] == IN_SRAM)	{
				byte = uart_send_buffer_pointer[uart_send_buffer_count_send][uart_send_buffer_string_index];
			}
			else if (uart_send_buffer_type[uart_send_buffer_count_send][1] == IN_EEPROM)	{
				byte = eeprom_read_byte(uart_send_buffer_pointer[uart_send_buffer_count_send] + uart_send_buffer_string_index);
			}
			else if (uart_send_buffer_type[uart_send_buffer_count_send][1] == IN_FLASH)	{
				byte = pgm_read_byte(uart_send_buffer_flash_pointer[uart_send_buffer_count_send] + uart_send_buffer_string_index);
			}

			UDR0 = byte;
			uart_send_buffer_string_index++;
			if (uart_send_buffer_string_index == 0)	{
				/* overflow from uart_send_buffer_string_index -> finish sending otherwise it would be infinite loop */
				ready = TRUE;
			}
			if (uart_send_buffer_string_index >= uart_send_buffer_type[uart_send_buffer_count_send][0])	{
				ready = TRUE;
			}
		}
		if (ready== TRUE)	{
			uart_send_buffer_string_index = 0;
			uart_send_buffer_count_send++;
			if (uart_send_buffer_count_send >= UART_SEND_BUFFER_NUMBER)	{
				uart_send_buffer_count_send = 0;
			}
			if (uart_send_buffer_count_send == uart_send_buffer_count)	{
				uart_is_sending = 0; /* all things in buffer are sent now */
			} else	{
				uart_send_buffer_cont(); /* recursive call to continue sending */
			}
		}
	}
}

/**
 * increment_uart_send_buffer - increment the index of uart_send_buffer
 *
 *		Function is called by the higher level functions like
 *		uart_send_text_sram() to do the uart_send_buffer handling
 *		that is common for all the higher level functions
 *
 *		Start transmitting if it is not started yet.
 *
 *		For internal use only
 */
static void increment_uart_send_buffer(void)
{
	/* only for internal use in the uart_send-routines */
	uint8_t uart_send_buffer_count_new = uart_send_buffer_count + 1;
	if (uart_send_buffer_count_new >= UART_SEND_BUFFER_NUMBER)	{
		uart_send_buffer_count_new = 0;
	}
	if (uart_send_buffer_count_new == uart_send_buffer_count_send)	{
		uart_send_buffer_cont();
		while (uart_send_buffer_count_new == uart_send_buffer_count_send && uart_is_sending != 0)
				uart_send_buffer_cont(); /* why do we have to call this function here (interrupt should be fine!) but otherwise uC is blocking! */
		uart_send_buffer_count = uart_send_buffer_count_new;
	} else	{
		uart_send_buffer_count = uart_send_buffer_count_new;
		uart_send_buffer_cont();
	}
}

/*
 * public functions
 */

/**
 * uart_init - initialize uart interface
 *
 *		set BAUDRATE in uart.h
 */
void uart_init()
{
	uart_is_sending = 0;
	uart_send_buffer_count = 0;
	uart_send_buffer_count_send = 0;

	uart_receive_buffer_count = 0;
	uart_receive_buffer_count_receive = 0;

	UBRR0H = UBRR_SETTING >> 8;
	UBRR0L = UBRR_SETTING & 0xFF;

	UCSR0A |= (1 << U2X0);
	UCSR0B |= (1 << TXEN0) | (1 << RXEN0) | (1 << TXCIE0) | (1 << RXCIE0);
}

/**
 * uart_send_text_sram - send a string stored in SRAM via uart
 * @text:	pointer to a string stored in SRAM
 *
 *		not the entire string is stored in the uart_send_buffer
 *		but only a pointer to the string, so the string must
 *		not be changed until it is completely transmitted.
 *
 *		if the string should be copied to the send buffer
 *		look at uart_send_text_buffer.
 */
void uart_send_text_sram(const char *text)
{
	uart_is_sending = 1;

	uart_send_buffer_pointer[uart_send_buffer_count] = (uint8_t *)text;
	uart_send_buffer_type[uart_send_buffer_count][0] = 0;
	uart_send_buffer_type[uart_send_buffer_count][1] = IN_SRAM;

	increment_uart_send_buffer();
}

/**
 * uart_send_binary_sram - send binary data stored in SRAM via uart
 * @number:		number of bytes to send
 * @data:		pointer to the binary data in SRAM
 *
 *		the data is not copied to SRAM -> look for
 *		uart_send_binary_buffer if the data should be copied
 *		to the uart_send_buffer. The data should not be changed
 *		during transmission. Otherwise wrong values will be sent
 *		over uart
 */
void uart_send_binary_sram(uint8_t number, uint8_t *data)
{
	uart_is_sending = 1;

	uart_send_buffer_pointer[uart_send_buffer_count] = data;
	uart_send_buffer_type[uart_send_buffer_count][0] = number;
	uart_send_buffer_type[uart_send_buffer_count][1] = IN_SRAM;

	increment_uart_send_buffer();
}

/**
 * uart_send_text_eeprom - transmit string stored in eeprom via uart
 * @text:		pointer to an address in internal eeprom where the string is stored
 *
 *		Use the EEMEM flag to declare variables stored in eeprom.
 *		e.g. int EEMEM eeprom_variable; You cannot directly input via C
 *		assignment operator (=) data into an eeprom variable, use
 *		eeprom_write_byte etc. from avr/eeprom.h instead
 */
void uart_send_text_eeprom(const char *text)
{
	uart_is_sending = 1;

	uart_send_buffer_pointer[uart_send_buffer_count] = (uint8_t *)text;
	uart_send_buffer_type[uart_send_buffer_count][0] = 0;
	uart_send_buffer_type[uart_send_buffer_count][1] = IN_EEPROM;

	increment_uart_send_buffer();
}

/**
 * uart_send_binary_eeprom - send binary data stored in eeprom
 * @number:		number of bytes to send
 * @data:		pointer to an address in internal eeprom where data is stored
 */
void uart_send_binary_eeprom(uint8_t number, uint8_t* data)
{
	uart_is_sending = 1;

	uart_send_buffer_pointer[uart_send_buffer_count] = data;
	uart_send_buffer_type[uart_send_buffer_count][0] = number;
	uart_send_buffer_type[uart_send_buffer_count][1] = IN_EEPROM;

	increment_uart_send_buffer();
}

/**
 * uart_send_text_flash - transmit string stored in flash via uart interface
 * @text:		16-bit address of an string stored in flash memory
 *
 *		Use the PROGMEM flag to declare variables that are stored in flash
 *		e.g. int PROGMEM flash_variable; You can assign an initial value
 *		to the variable with the = operator. Look into avr/progmem.h.
 */
void uart_send_text_flash(uint16_t text)
{
	uart_is_sending = 1;

	uart_send_buffer_flash_pointer[uart_send_buffer_count] = text;
	uart_send_buffer_type[uart_send_buffer_count][0] = 0;
	uart_send_buffer_type[uart_send_buffer_count][1] = IN_FLASH;

	increment_uart_send_buffer();
}

/**
 * uart_send_binary_flash - transmit binary data stored in flash via uart interface
 * @number:		number of bytes to transmit
 * @data:		16-bit address of the binary data stored in flash memory
 */
void uart_send_binary_flash(uint8_t number, uint16_t data)
{
	uart_is_sending = 1;

	uart_send_buffer_flash_pointer[uart_send_buffer_count] = data;
	uart_send_buffer_type[uart_send_buffer_count][0] = number;
	uart_send_buffer_type[uart_send_buffer_count][1] = IN_FLASH;

	increment_uart_send_buffer();
}

/**
 * uart_send_text_buffer - copy string in SRAM to uart_send_buffer and transmit
 *						   via uart interface
 * @text:	pointer to a string stored in SRAM
 *
 *		The string will be copied to the send buffer. The given string
 *		can be changed immediately after calling this function because
 *		the string has been copied to internal uart_send_buffer
 */
void uart_send_text_buffer(const char *text)
{
	uart_is_sending = 1;

	uint8_t i = 0;

	while (text[i] != 0)	{
		uart_send_buffer[uart_send_buffer_count][i] = ((uint8_t *)text)[i];
		i++;
		if(i >= UART_SEND_BUFFER_LENGTH-1)	{
			break;
		}
	}
	uart_send_buffer[uart_send_buffer_count][i] = 0;
	uart_send_buffer_type[uart_send_buffer_count][0] = 0;
	uart_send_buffer_type[uart_send_buffer_count][1] = IN_BUFFER;

	increment_uart_send_buffer();
}

/**
 * uart_send_binary_buffer - copy binary data from SRAM to uart_send_buffer
 *							 and transmit via uart interface
 * @number:		number of bytes to transmit
 * @data:		pointer to data stored in SRAM
 */
void uart_send_binary_buffer(uint8_t number, uint8_t* data)
{
	uart_is_sending = 1;

	uint8_t i = 0;

	for (i = 0; i < number; i++)	{
		if(i >= UART_SEND_BUFFER_LENGTH)	{
			break;
		}
		uart_send_buffer[uart_send_buffer_count][i] = data[i];
	}
	uart_send_buffer_type[uart_send_buffer_count][0] = number;
	uart_send_buffer_type[uart_send_buffer_count][1] = IN_BUFFER;

	increment_uart_send_buffer();
}

/**
 * uart_send_int - convert integer to ascii and send via uart interface
 * @val:	integer value to convert and send
 *
 *		The functions stores the converted integer in the uart_send_buffer
 *		and transmits it
 *
 *		Returns the return value of the int_to_string function in utils.c
 */
uint8_t uart_send_int(uint32_t val)	{
	char *str = (char *)uart_send_buffer[uart_send_buffer_count];
	uint8_t ret;
	ret = int_to_string(str, UART_SEND_BUFFER_LENGTH, val);
	if (ret != TRUE)	{
		return ret;
	}

	uart_send_buffer_type[uart_send_buffer_count][0] = 0;
	uart_send_buffer_type[uart_send_buffer_count][1] = IN_BUFFER;

	increment_uart_send_buffer();
	return TRUE;
}

/**
 * uart_send_int_hex - converts an integer to ascii (hexadecimal) and send over uart
 * @val: the integer value to convert and send over uart
 *
 *		Returns TRUE on success, FALSE if number was too big for uart buffer
 */
uint8_t uart_send_int_hex(uint32_t val)	{
	char *str = (char *)uart_send_buffer[uart_send_buffer_count];
	uint8_t ret;
	ret = int_to_string_hex(str, UART_SEND_BUFFER_LENGTH, val);
	if (ret != TRUE)	{
		return ret;
	}

	uart_send_buffer_type[uart_send_buffer_count][0] = 0;
	uart_send_buffer_type[uart_send_buffer_count][1] = IN_BUFFER;

	increment_uart_send_buffer();
	return TRUE;
}

/**
 * uart_receive_buffer_text - check if some data has been received and get the
 *							  latest received string
 * @text:		points to a char pointer where the address of the received string
 *				will get stored. The value in the char pointer is only valid
 *				if the function returns true
 *
 *		The uart_receive_buffer receives linewise. You will get the last line
 *		after receiving an '\n' or '\r' or '\n\r' newline
 *
 *		Returns FALSE if there is no new string, TRUE if there is a new string;
 */
uint8_t uart_receive_buffer_text(char **text)
{
	if (uart_receive_buffer_count != uart_receive_buffer_count_receive)	{
		if (text != NULL)	{
			*text = (char *) uart_receive_buffer[uart_receive_buffer_count_receive];
			uart_receive_buffer_count_receive++;
			if (uart_receive_buffer_count_receive >= UART_RECEIVE_BUFFER_NUMBER)	{
				uart_receive_buffer_count_receive = 0;
			}
		}
		return TRUE;
	} else	{
		return FALSE;
	}
}

ISR(USART0_TX_vect)
{
#ifdef ISR_LED
	LED_ON();
#endif

	uart_send_buffer_cont();
#ifdef ISR_LED
	LED_OFF();
#endif
}

ISR(USART0_RX_vect)
{
#ifdef ISR_LED
	LED_ON();
#endif

	char byte = UDR0;
	static uint8_t flag_new = 0;
	if (byte == '\n' || byte == '\r')	{
		if (flag_new != 0)	{
			uart_receive_buffer[uart_receive_buffer_count][uart_receive_buffer_count_string_index] = 0; /* 0-terminated */
			uart_receive_buffer_count++;
			if (uart_receive_buffer_count >= UART_RECEIVE_BUFFER_NUMBER)	{
				uart_receive_buffer_count = 0;
			}
			uart_receive_buffer_count_string_index = 0;
			flag_new = 0;
		}
	} else	{
		flag_new = 1;
		uart_receive_buffer[uart_receive_buffer_count][uart_receive_buffer_count_string_index] = byte;
		uart_receive_buffer_count_string_index++;
		if (uart_receive_buffer_count_string_index >= UART_RECEIVE_BUFFER_LENGTH)	{
			uart_receive_buffer_count_string_index = 0;
			uart_receive_buffer[uart_receive_buffer_count][UART_RECEIVE_BUFFER_LENGTH-1] = 0; /* that the string is always 0-terminated */
		}
	}
#ifdef ISR_LED
	LED_OFF();
#endif
}
