/*
 *  user.c - functions for user tag access and user identification management
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
#include <avr/eeprom.h>

#include "uart.h"
#include "rfid.h"
#include "main.h"
#include "morse.h"
#include "user.h"
#include "rtc.h"
#include "startup.h"
#include "ext_eeprom.h"
#include "pins.h"

#define USER_READ_TIMEOUT_MS	4000 /* should be a multiple of TIMER1_MS in main.h */
#define USER_RFID_LED_ON_MS		700 /* should be a multiple of TIMER1_MS in main.h */

#define TAG_ID_BLOCK	0x01
#define TAG_ID_BYTE		0x00 /* stored with little endian in TAG_ID_BYTE and (TAG_ID_BYTE + 1) */
#define TAG_ID_SIZE		0x02 /* in bytes */

/*
 * fox secret is a number that is set in the fox so that you cannot imitate the
 * data which the fox writes onto the tag (at least if you do not know the secret)
 *
 * fox secret ist stored on tag in little endian!
 */
#define FOX_SECRET_BLOCK	0x01
#define FOX_SECRET_ADDRESS(call)	(TAG_ID_BYTE + TAG_ID_SIZE + call * FOX_SECRET_SIZE)
#define FOX_SECRET_SIZE	0x02 /* bytes */

#define TIMESTAMP_DIVISOR	1	/* timestamp is in steps of TIMESTAMP_DIVISOR seconds */

#define HISTORY_BLOCKS				3
#define HISTORY_ENTRY_SIZE			4		/* in bytes */
#define HISTORY_ENTRIES_PER_BLOCK	4
#define HISTORY_ENTRIES_MAX			12 //(HISTORY_BLOCKS * HISTORY_ENTRIES_PER_BLOCK)
#define HISTORY_WRITE_MAX_TRIES		5

#define MIFARE_BLOCK_MIN	1
#define MIFARE_BLOCK_MAX	15

uint8_t history[HISTORY_ENTRIES_MAX][HISTORY_ENTRY_SIZE] = {{0}}; /* zero the array, refer to: http://stackoverflow.com/questions/5636070/zero-an-array-in-c-code */
uint8_t history_pointer_write = 0; /* points to next place that should be written in history array */

#define EXT_EEPROM_ENTRY_SIZE		HISTORY_ENTRY_SIZE
#define EXT_EEPROM_START_ADDRESS	10 /* some addresses free for things that might have to be stored additionally */
#define EXT_EEPROM_WRITE_POINTER_ADDRESS	0 /* this is address of the pointer in ext_eeprom that shows address of next free element */
#define EXT_EEPROM_WRITE_POINTER_SIZE		2 /* bytes */
#define EXT_EEPROM_MAX_ADDRESS				65535

uint16_t EEMEM secret;

uint8_t is_started = FALSE;
uint8_t write_id = FALSE;
uint16_t next_write_id = 0x00;

volatile uint8_t read_timeout = TRUE;
volatile uint8_t rfid_led_on = FALSE;

const char PROGMEM tag_id[] = "Tag ID: ";
const char PROGMEM fox1[] = "Fox 1";
const char PROGMEM fox2[] = "Fox 2";
const char PROGMEM fox3[] = "Fox 3";
const char PROGMEM fox4[] = "Fox 4";
const char PROGMEM fox5[] = "Fox 5";

const char PROGMEM secret_msg[] = " Secret: ";
const char PROGMEM history_msg[] = ": ";
const char PROGMEM tag_msg[] = "Tag ID: ";
const char PROGMEM timestamp_msg[] = " Timestamp: ";
const char PROGMEM tag_read_begin_msg[] = "--- NEW TAG 0x55005500 ---";
const char PROGMEM tag_read_end_msg[] = "--- END TAG 0x55005500 ---";
const char PROGMEM tag_read_error_msg[] = "--- ERROR TAG 0x55005500 ---";
const char PROGMEM history_written_msg[] = "History written\r\n";
const char PROGMEM eeprom_read_begin_msg[] = "--- BEGIN FOX HISTORY 0x66006600 ---";
const char PROGMEM eeprom_read_end_msg[] = "--- END FOX HISTORY 0x66006600 ---";

const char PROGMEM write_tag_id_error_msg[] = "writing tag id failed, maybe tag is write protected with access key";

#define READ_TAG_MSG_MAX 5

const PGM_P read_tag_msg[] =	{
	tag_id,
	fox1,
	fox2,
	fox3,
	fox4,
	fox5
};

/*
 * internal functions
 */

/**
 * user_rfid_led_on - register rfid led as on (blinking) to indicate ready rfid operation
 *
 *		Blinking is done with user_time_tick function
 */
static void user_rfid_led_on(void)
{
	RFID_LED_ON();
	rfid_led_on = TRUE;
}

/**
 * user_get_rfid_led - returns if user_rfid_led_on or user_rfid_led_off was last called
 *
 *		Function is needed for user_time_tick to determine if it should blink or not!
 *
 *		Return: TRUE if user_rfid_led_on was called last, FALSE if user_rfid_led_off
 *		was called last
 */
static uint8_t user_get_rfid_led(void)
{
	return rfid_led_on;
}

/**
 * user_rfid_led_off - register rfid_led as off
 *
 *		Tells user_time_tick function that rfid led should not blink any more
 */
static void user_rfid_led_off(void)
{
	RFID_LED_OFF();
	rfid_led_on = FALSE;
}

/**
 * user_get_read_timeout - returns state of read_timeout if new tag should also be
 *						   read if it is the same as last tag
 *		Return: TRUE if read_timeout has passed and FALSE if read_timeout will
 *		pass in future
 */
static uint8_t user_get_read_timeout(void)
{
	return read_timeout;
}

/**
 * user_set_read_timeout - set state of read_timeout
 * @timeout: either TRUE -> read tag if it is last tag or FALSE -> do not read
 *			 tag if it is last read tag
 *
 *		If read_timeout is set to true, a tag will also be read if it was the
 *		last tag
 */
static void user_set_read_timeout(uint8_t timeout)
{
	read_timeout = timeout;
}

/**
 *	user_get_ext_eeprom_pointer - read from ext_eeprom the pointer of next free
 *								  element in ext_eeprom
 *
 *		Return: the pointer (16-bit)
 */
static uint16_t user_get_ext_eeprom_pointer(void)
{
	uint8_t byte;
	ext_eeprom_read_byte(EXT_EEPROM_WRITE_POINTER_ADDRESS, &byte);
	return byte;
}

/**
 * user_set_ext_eeprom_pointer - write ext_eeprom pointer
 * @pointer:	the value that the pointer should be set to
 */
static void user_set_ext_eeprom_pointer(uint16_t pointer)
{
	ext_eeprom_write_byte(EXT_EEPROM_WRITE_POINTER_ADDRESS, pointer);
}

/**
 * user_reset_ext_eeprom_pointer - set ext_eeprom pointer to start address
 */
static void user_reset_ext_eeprom_pointer(void)
{
	ext_eeprom_write_byte(EXT_EEPROM_WRITE_POINTER_ADDRESS, EXT_EEPROM_START_ADDRESS);
}

/**
 * get_history_block_physical - returns the block number on rfid tag for history
 * @block_logical:	the block number for history between 0 and (HISTORY_BLOCKS - 1)
 * @num:			fox number
 *
 *		The physical blocks on mifare tag depend on the the fox number. This
 *		functions returns which blocks the user module should use for the
 *		history
 *
 *		The function uses the call_sign from morse.h because the location
 *		of the history should depend on the call sign of the fox
 *
 *		Return: the block number to use for the given logical block_number
 */
static uint8_t get_history_block_physical(uint8_t block_logical, uint8_t num)
{
	if (num == 0)	{
		num = 1;
	}
	uint8_t base_block = (num - 1) * 4 + 4;
	return base_block + block_logical + block_logical / 3;
	/*
	 * (block_logical / 3) has to be added because of the access-bit-blocks of
	 * mifare chips that have to be let out
	 */
}

/**
 * calculate_timediff - calculates the difference between two times
 * time1: pointer to array with time values (second, minute, hour, date)
 * time2: pointer to array with time values (second, minute, hour, date)
 * len:   length of array because array can also be shorter -> e.g. no year given!)
 *
 *		Note: Calculates the difference time2 - time1, therefore time2 should
 *		be greater than time1
 *		The function does not read month and year. The two times should be in same
 *		month and year because otherwise difference calculation would be complicated
 *
 *		Return: the difference in seconds
 */
static uint16_t calculate_timediff(uint8_t *time1, uint8_t *time2, uint8_t len)
{
	uint8_t i;
	int32_t diff = 0; /* 32 bit to be sure that there is no overflow! */
	if (len > RTC_DATE + 1)	{
		len = RTC_DATE + 1;
	}
	for (i = len - 1; i >= 0 && i < len; i--)	{
		diff = diff + (time2[i] - time1[i]);
		switch (i)	{
			case (RTC_DATE):
				diff *= 24;
				break;
			case (RTC_HOUR):
				diff *= 60;
				break;
			case (RTC_MINUTE):
				diff *= 60;
				break;
		}
	}
	if (diff < 0)	{
		diff = 0; /* time2 was smaller than time1, maybe because there was a month overflow */
	}
	return diff / TIMESTAMP_DIVISOR;
}

/**
 * user_add_to_history - add user to the history of fox in ram
 * tag_id: tag id that should be added to history
 *
 *		Transponder is only added to history if it does not have
 *		the same id as the last transponder in history
 */
static void user_add_to_history(uint16_t tag_id)
{
	uint8_t history_pointer_write_temp = history_pointer_write;
	if (history_pointer_write_temp == 0)	{
		history_pointer_write_temp = HISTORY_ENTRIES_MAX;
	} else {
		history_pointer_write_temp--;
	}
	if (history[history_pointer_write_temp][0] == (tag_id & 0xff) &&
		history[history_pointer_write_temp][1] == (tag_id >> 8))	{
#ifdef DEBUG_WRITE_HISTORY
		uart_send_text_sram("already in history");
		UART_NEWLINE();
#endif
		return; /* do not add to history if tag id is the same as last tag id */
	}

	uint8_t i;
	uint8_t time[RTC_DATE - RTC_SECOND + 1];
	uint8_t start_time[STARTUP_DATE - STARTUP_SECOND + 1];
	for (i = RTC_SECOND; i <= RTC_DATE; i++)	{
		rtc_get_time(i, time + i - RTC_SECOND);
		start_time[i - RTC_SECOND] = startup_get_start_time(i);
	}

	uint16_t timestamp = calculate_timediff(start_time, time, STARTUP_DATE - STARTUP_SECOND + 1);

	history[history_pointer_write][0] = tag_id & 0xff;
	history[history_pointer_write][1] = tag_id >> 8;
	history[history_pointer_write][2] = timestamp & 0xff;
	history[history_pointer_write][3] = timestamp >> 8;

	uint16_t ext_eeprom_pointer = user_get_ext_eeprom_pointer();
	ext_eeprom_write_block(history[history_pointer_write], ext_eeprom_pointer, EXT_EEPROM_ENTRY_SIZE);
	if (ext_eeprom_pointer <= (EXT_EEPROM_MAX_ADDRESS - 2 * EXT_EEPROM_ENTRY_SIZE))	{
		ext_eeprom_pointer += EXT_EEPROM_ENTRY_SIZE;
		user_set_ext_eeprom_pointer(ext_eeprom_pointer);
	}

	history_pointer_write++;
	if (history_pointer_write >= HISTORY_ENTRIES_MAX)	{
		history_pointer_write = 0;
	}
}

/**
 * user_write_history - write history to current tag
 *
 *		Return: TRUE on success, FALSE on failure
 */
static uint8_t user_write_history(void)
{
	uint8_t ret = STATUS_OK;

	uint8_t block_temp_length = RFID_BLOCK_SIZE + 2;
	uint8_t block_temp[block_temp_length];
	uint8_t i, j, k;
	uint8_t temp_pointer_write = history_pointer_write;
	uint8_t tries = 0;

	for (i = 0; i < HISTORY_BLOCKS; i++)	{
		for (j = 0; j < HISTORY_ENTRIES_PER_BLOCK; j++)	{
			if (temp_pointer_write == 0)	{
				temp_pointer_write = HISTORY_ENTRIES_MAX - 1;
			} else {
				temp_pointer_write--;
			}
			for (k = 0; k < HISTORY_ENTRY_SIZE; k++)	{
				block_temp[j * HISTORY_ENTRY_SIZE + k] = history[temp_pointer_write][k];
			}
		}
		ret = rfid_write_block(get_history_block_physical(i, morse_get_fox_number()), block_temp, RFID_BLOCK_SIZE);
		if (ret != STATUS_OK)	{
			tries++;
			if (tries > HISTORY_WRITE_MAX_TRIES)	{
				goto user_write_history_return;
			} else {
				i--; /* try to write this block another time */
			}
		} else {
			tries = 0; /* if writing of block was successful -> reset tries */
		}
	}

	tries = 0;
	while (1)	{
		uint8_t len = block_temp_length;
		ret = rfid_read_block(FOX_SECRET_BLOCK, block_temp, &len);
		tries++;
		if (ret == STATUS_OK && len == block_temp_length)	{
			break;
		} else if (tries > HISTORY_WRITE_MAX_TRIES)	{
			goto user_write_history_return;
		}
	};

	uint16_t secret = user_get_secret();
	block_temp[FOX_SECRET_ADDRESS(morse_get_call_sign())] = secret & 0xff;
	block_temp[FOX_SECRET_ADDRESS(morse_get_call_sign()) + 1] = secret >> 8;

	tries = 0;
	while (1)	{
		ret = rfid_write_block(FOX_SECRET_BLOCK, block_temp, RFID_BLOCK_SIZE);
		tries++;
		if (ret == STATUS_OK)	{
			break;
		} else if (tries > HISTORY_WRITE_MAX_TRIES)	{
			goto user_write_history_return;
		}
	}

user_write_history_return:
	/* rfid_close_tag() has to be called in outside function, e.g. in
	 * user_new_tag()
	 */
	return ret;
}

/**
 * user_read_id - read id of the tag
 * @tag_id: pointer to variable where the id will be stored
 *
 *		Return: STATUS_OK on success STATUS_??? with error code
 *		otherwise
 */
static uint8_t user_read_tag_id(uint16_t *tag_id)
{
	uint8_t buffer[18];
	uint8_t buffer_size = 18;
	uint8_t ret;
	ret = rfid_read_block(TAG_ID_BLOCK, buffer, &buffer_size);
	if (ret != STATUS_OK)	{
		goto user_read_id_return;
	}
	if (buffer_size == 18)	{
		*tag_id = buffer[TAG_ID_BYTE] | (buffer[TAG_ID_BYTE + 1] << 8);
	} else {
		ret = STATUS_ERROR;
	}
user_read_id_return:
	/* rfid_close_tag() must be done in outside function (e.g. in user_new_tag) */
	return ret;
}

/**
 * user_read_tag - read history from tag and send to uart
 *
 *		Return: STATUS_OK on success, STATUS_?? otherwise
 */
uint8_t user_read_tag(void)
{
	uint8_t buffer_length = RFID_BLOCK_SIZE + 2;
	uint8_t buffer[buffer_length];
	uint8_t ret;

	uint8_t len = buffer_length;
	ret =  rfid_read_block(TAG_ID_BLOCK, buffer, &len);
	if (ret != STATUS_OK)	{
		goto user_read_tag_return;
	}
	if (len != buffer_length)	{
		ret = STATUS_ERROR;
		goto user_read_tag_return;
	}

	UART_NEWLINE();
	uart_send_text_flash((uint16_t)tag_read_begin_msg);
	UART_NEWLINE();
	UART_NEWLINE();

	uint8_t i;
	for (i = 0; i <= READ_TAG_MSG_MAX; i++)	{
		uint16_t temp = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
		uart_send_text_flash((uint16_t)read_tag_msg[i]);
		if (i != 0)
			uart_send_text_flash((uint16_t)secret_msg);
		uart_send_int(temp);
		UART_NEWLINE();
	}
	UART_NEWLINE();

	uint8_t j;
	for (i = FOX_NUMBER_FIRST; i <= FOX_NUMBER_MAX; i++)	{
		uart_send_text_flash((uint16_t)read_tag_msg[i]);
		uart_send_text_flash((uint16_t)history_msg);
		UART_NEWLINE();

		for (j = 0; j < HISTORY_BLOCKS; j++)	{
			len = buffer_length;
			ret = rfid_read_block(get_history_block_physical(j, i), buffer, &len);
			if (ret != STATUS_OK)	{
				goto user_read_tag_return;
			}
			if (len != buffer_length)	{
				ret = STATUS_ERROR;
				goto user_read_tag_return;
			}
			uint8_t k;
			for (k = 0; k < HISTORY_ENTRIES_PER_BLOCK; k++)	{
				uart_send_text_flash((uint16_t)tag_id);
				uart_send_int((uint16_t)buffer[k * HISTORY_ENTRY_SIZE] | (buffer[k * HISTORY_ENTRY_SIZE + 1] << 8));
				UART_NEWLINE();
				uart_send_text_flash((uint16_t)timestamp_msg);
				uart_send_int((uint16_t)buffer[k * HISTORY_ENTRY_SIZE + 2] | (buffer[k * HISTORY_ENTRY_SIZE + 3] << 8));
				UART_NEWLINE();
			}
		}
		UART_NEWLINE();
	}

	uart_send_text_flash((uint16_t)tag_read_end_msg);
	UART_NEWLINE();
	UART_NEWLINE();

user_read_tag_return:
	rfid_close_tag();
	if (ret != STATUS_OK)	{
		uart_send_text_flash((uint16_t)tag_read_error_msg);
	}
	return ret;
}

/**
 * user_write_id - write an tag id to a tag
 * @tag_id: the 16-bit id to write on the tag
 *
 *		Return: STATUS_OK if writing was successful, returns STATUS_??? otherwise
 */
static uint8_t user_write_tag_id(uint16_t tag_id)
{
	uint8_t buffer[16] = {0};
	uint8_t length = 16;
	uint8_t ret;
	uint8_t i;

	for (i = MIFARE_BLOCK_MIN; i <= MIFARE_BLOCK_MAX; i++)	{
		if ((i % 4) != 3)	{
			/* do not write access bit blocks! */
			ret = rfid_write_block(i, buffer, length);
		} else {
			ret = STATUS_OK;
		}
		if (ret != STATUS_OK)	{
			goto user_write_id_return_code;
		}
		break;
	}

		buffer[TAG_ID_BYTE] = next_write_id & 0xff;
		buffer[TAG_ID_BYTE+1] = next_write_id >> 8;
		for (i = FOX_SECRET_ADDRESS(CALL_MOE); i < FOX_SECRET_ADDRESS(CALL_MO5) + FOX_SECRET_SIZE; i++)	{
			buffer[i] = 0;
		}
		length = 16;
		ret = rfid_write_block(TAG_ID_BLOCK, buffer, length);
		if (ret != STATUS_OK)	{
			goto user_write_id_return_code;
		}
	if (ret == STATUS_OK)	{
		/* tag write was successful */
		write_id = FALSE;
	}
user_write_id_return_code:
	rfid_close_tag();
	return ret;
}

/*
 * public functions
 */

/**
 * user_init - intialise user module
 */
void user_init(void)
{
#ifdef NEW_PROTOTYPE
	RFID_DDR |= (1 << RFID_LED);
#endif
}

/**
 * user_show_configuration - output configuration of user module to uart 
 */
void user_show_configuration(void)
{
	uart_send_text_sram("Secret key: ");

	uart_send_int(user_get_secret());
	UART_NEWLINE();
}

/**
 * user_start_time - to enable the user module -> writing tags to eeprom if
 * tags are recognised
 *
 *		is called by startup.c
 */
void user_start_time(void)
{
	is_started = TRUE;
}

/**
 * user_stop_time - to disable the user module -> do not write recognised tags
 *
 *		is called by startup.c
 */
void user_stop_time(void)
{
	is_started = FALSE;
}

/**
 * user_new_tag - process a new tag that is found
 *
 *		is called by rfid.c as soon as a new tag is found
 *
 *		Return: STATUS_OK on success Return: STATUS_??? otherwise
 */
uint8_t user_new_tag(void)	{
	uint8_t ret = STATUS_OK;
	uint8_t history_written = FALSE;
	uint8_t read_timeout_old = FALSE;

	static uint16_t last_tag_id;

	if (write_id == TRUE)	{
		ret = user_write_tag_id(next_write_id);
		if (ret != TRUE)	{
			uart_send_text_flash((uint16_t)write_tag_id_error_msg);
			UART_NEWLINE();
			return ret;
		} else {
			uart_send_text_sram("tag id ");
			uart_send_int(next_write_id);
			uart_send_text_sram(" written!");
			UART_NEWLINE();
			last_tag_id = next_write_id;
			user_set_read_timeout(FALSE);
		}
	} else {
		/* if it is not demo fox -> write history */
		if (morse_get_fox_number() != FOX_NUMBER_DEMO)	{
			/* it is not demo fox -> write history */
			if (is_started == TRUE)	{
				static uint16_t tag_id_old = 0;
				uint16_t tag_id;
				ret = user_read_tag_id(&tag_id);
				if (ret != STATUS_OK)	{
#ifdef DEBUG_WRITE_HISTORY
					uart_send_text_sram("read id failed");
					UART_NEWLINE();
#endif
					goto user_new_tag_exit;
				} else {
#ifdef DEBUG_WRITE_HISTORY
					uart_send_text_sram("read id success");
					UART_NEWLINE();
#endif
				}
				user_add_to_history(tag_id);
				if (tag_id != tag_id_old)	{
					ret = user_write_history();
					if (ret == STATUS_OK)	{
#ifdef DEBUG_WRITE_HISTORY
						uart_send_text_sram("history written");
#endif
						uart_send_text_flash((uint16_t)history_written_msg);
						uart_send_text_sram(" tag id: ");
						uart_send_int(tag_id);
						UART_NEWLINE();
						tag_id_old = tag_id;
						history_written = TRUE;
					} else {
#ifdef DEBUG_WRITE_HISTORY
						uart_send_text_sram("history failed to write");
						UART_NEWLINE();
						goto user_new_tag_exit;
#endif
					}
				} else {
					/* also show here success light!! */
				}
			}
		}
		/* read history and send via uart */
		uint16_t tag_id;
		uint8_t ret = user_read_tag_id(&tag_id);
		if (ret != STATUS_OK)	{
			goto user_new_tag_exit;
		}
		if (tag_id != last_tag_id || read_timeout == TRUE)	{
			if (user_read_tag() == STATUS_OK)	{
				last_tag_id = tag_id;
				read_timeout_old = user_get_read_timeout();
				user_set_read_timeout(FALSE);
			}
		}
		if (read_timeout_old == TRUE || history_written == TRUE)	{
			user_rfid_led_on();
		}
	}
user_new_tag_exit:
	rfid_close_tag();
	return ret;
}

/**
 * user_set_id - write an id to next tag
 * @tag_id: 16-bit id to write onto next tag
 *
 *		Function returns before tag is written.
 *		Tag is written as soon as a new tag is recognised
 *
 *		Return: TRUE if the last tag is written, returns FALSE
 *		if there is already a tag id in the buffer to write onto
 *		next recognised tag. Retunrs FALSE if tag_id == 0 because 0 as tag id
 *		is forbidden.
 */
uint8_t user_set_id(uint16_t tag_id)
{
	if (tag_id == 0)	{
		return FALSE;
	}
	if (write_id == FALSE)	{
		write_id = TRUE;
		next_write_id = tag_id;
		return TRUE;
	}
	return FALSE;
}

/**
 * user_check_set_id_state - check if user_set_id would return TRUE
 *
 *		Return: TRUE if the last tag is written, returns FALSE
 *		if there is already a tag id in the buffer to write onto
 *		next recognised tag.
 */
uint8_t user_check_set_id_state(void)
{
	if (write_id == FALSE)	{
		return TRUE;
	} else {
		return FALSE;
	}
}

/**
 * user_cancel_set_id - delete the buffered tag id that should have been written to next tag
 */
void user_cancel_set_id(void)
{
	write_id = FALSE;
}

/**
 * user_set_secret - set the secret key of the fox that is written on the tags
 * secret_int:	the 16-bit secret key
 *
 *		Return: TRUE if secret was set correctly, FALSE if secret could not
 *		be set because it is zero and zero is not allowed
 */
uint8_t user_set_secret(uint16_t secret_int)
{
	if (secret_int == 0)	{
		return FALSE;
	}
	eeprom_write_word(&secret, secret_int);
	return TRUE;
}

/**
 * user_get_secret - get the secret key of the fox that is written on the tags
 *
 *		Return: the 16-bit secret key
 */
uint16_t user_get_secret(void)
{
	uint16_t secret_int = eeprom_read_word(&secret);
	if (secret_int == 0)	{
		secret_int = 1;
	}
	return secret_int;
}

/**
 * user_print_fox_history - read user history saved in eeprom and write it to uart
 */
void user_print_fox_history(void)
{
	UART_NEWLINE();
	uart_send_text_flash((uint16_t)eeprom_read_begin_msg);
	UART_NEWLINE();
	UART_NEWLINE();

	uint16_t eeprom_pointer_end = user_get_ext_eeprom_pointer();
	uint16_t eeprom_pointer = EXT_EEPROM_START_ADDRESS;
	uint8_t buffer[EXT_EEPROM_ENTRY_SIZE] = {2,2,2,2};

	while (eeprom_pointer < eeprom_pointer_end)	{
		ext_eeprom_read_block(buffer, eeprom_pointer, 4);
		uint16_t eeprom_pointer_temp = eeprom_pointer + 4;
		if (eeprom_pointer_temp < eeprom_pointer)	{
			break; /* overflow of eeprom_pointer happened -> break to prevent infinte loop */
		}
		eeprom_pointer = eeprom_pointer_temp;
		uart_send_text_flash((uint16_t)tag_id);
		uart_send_int((uint16_t)buffer[0] | (buffer[1] << 8));
		uart_send_text_flash((uint16_t)timestamp_msg);
		uart_send_int((uint16_t)buffer[2] | (buffer[3] << 8));
	}

	UART_NEWLINE();
	uart_send_text_flash((uint16_t)eeprom_read_end_msg);
	UART_NEWLINE();
	UART_NEWLINE();
}

/**
 * user_clear_history - deletes user history in ram and external eeprom
 */
void user_clear_history(void)
{
	user_reset_ext_eeprom_pointer();
	uint8_t i, j;
	for (i = 0; i < HISTORY_ENTRIES_MAX; i++)	{
		for (j = 0; j < HISTORY_ENTRIES_PER_BLOCK; j++)	{
			history[i][j] = 0x00;
		}
	}
}

/**
 * user_time_tick - function is called regularly by main module for time base of user module
 */
void user_time_tick(void)
{
	static uint8_t user_read_timeout_count = 0;
	if (user_read_timeout_count >= (USER_READ_TIMEOUT_MS / TIMER1_MS))	{
		user_read_timeout_count = 0;
		user_set_read_timeout(TRUE);
	}
	if (user_get_read_timeout() == FALSE)	{
		user_read_timeout_count++;
	}
	static uint8_t rfid_led_count = 0;
	if (rfid_led_count >= (USER_RFID_LED_ON_MS / TIMER1_MS))	{
		rfid_led_count = 0;
		user_rfid_led_off();
	}
	if (user_get_rfid_led() == TRUE)	{
		rfid_led_count++;
		RFID_LED_TOGGLE();
	}
}
