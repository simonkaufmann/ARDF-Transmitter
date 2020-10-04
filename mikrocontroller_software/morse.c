/*
 *  morse.h - definitions of functions for parsing the received uart commands
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

#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#include "main.h"
#include "startup.h"
#include "morse.h"
#include "rtc.h"
#include "uart.h"
#include "pins.h"
#include "dds.h"

#define BASE_WPM	240	/* 1 WPM equals with 240 * 5000us, BASE_WPM is the morse unit of 1 wpm, morse unit is one dot */
/*
 * BASE_WPM = 60s / 50dits / (5000us/tick) = number of timer-ticks per dits
 * 50dits = number of dits of word PARIS as reference for wpm calculation
 * 5000us/tick = overflow time of timer
 * 60s = 1 minute -> 60s / 50dits for 1 wpm!
 */

#define TIMER2_PRELOAD	(256 - (F_CPU / 256 / 200)) /* 200 tics per second (5000us time between overflow) */

#define MORSE_STARTED_TIME		0	/* if this bit in morse_started is 1 then time is between start and stop time */
#define MORSE_STARTED_MINUTE	1	/* if this bit in morse_started is 1 then it is in the minute for this fox number */

#define MORSE_MINUTE_IS_START			0
#define MORSE_MINUTE_IS_STOP			1

uint8_t EEMEM morsing_enabled;
uint8_t EEMEM fox_number_eemem;
uint8_t EEMEM transmit_minute_eemem;
uint8_t EEMEM call_number_eemem;
uint8_t EEMEM fox_max_eemem;

uint16_t EEMEM morse_unit_eemem; /* time of one dot divided by 100us */

const char PROGMEM fxn[] = "Fox number: ";
const char PROGMEM cfm[] = "Maximum number of foxes used: ";
const char PROGMEM call_sign_number_text[] = "Call sign: ";
const char PROGMEM morse_mode_text[] = "Morse mode: ";
const char PROGMEM transmit_minute_msg[] = "Transmit minute: ";

/*
 * base = morse_unit * 100us
 * one dot is					1 * base_unit long
 * one dash is					3 * base_unit long
 * space parts of a letter is	1 * base_unit long
 * space between letters is		3 * base_unit long
 * space between words ist		7 * base_unit long
 *
 * for each identification of a morse sign is 1 bit long
 * space between parts of a letter ist added after each
 * identifier automatically (so that space between words and
 * space between letters is internally only 2 and 6
 *
 * dot = 0b00
 * dash = 0b01
 * space letter = 0b10
 * space word = 0b11
 */

const uint8_t PROGMEM morse_code[][CALL_MAX + 1] = {
	/* morse code MOE: -- --- * (end of word) */
	{0b01011001, 0b01011000, 0b11000000, 0b00000000},
	/* morse code MOI: -- --- ** (end of word) */
	{0b01011001, 0b01011000, 0b00110000, 0b00000000},
	/* morse code MOS: -- --- *** (end of word) */
	{0b01011001, 0b01011000, 0b00001100, 0b00000000},
	/* morse code MOH: -- --- **** (end of word) */
	{0b01011001, 0b01011000, 0b00000011, 0b00000000},
	/* morse code MO5: -- --- ***** (end of word) */
	{0b01011001, 0b01011000, 0b00000000, 0b11000000},
	/* morse code MO: -- --- (end of word) */
	{0b01011001, 0b01011100, 0b00000000, 0b00000000}
};

const char PROGMEM moe[] = "MOE";
const char PROGMEM moi[] = "MOI";
const char PROGMEM mos[] = "MOS";
const char PROGMEM moh[] = "MOH";
const char PROGMEM mo5[] = "MO5";
const char PROGMEM mo[] = "MO";

const PGM_P morse_text[] = {
	moe,
	moi,
	mos,
	moh,
	mo5,
	mo
};

uint8_t current_call[CALL_MAX + 1];
uint16_t current_morse_unit;
uint8_t current_fox_number;
uint8_t current_fox_max;
volatile uint8_t current_morsing_enabled; /* transmit continuous carrier (only in on-minutes) if morsing is disable */
uint8_t current_transmit_minute;

uint8_t morse_started = (1 << MORSE_STARTED_MINUTE);
volatile uint8_t reset = TRUE;
volatile uint8_t continuous_carrier = FALSE;

/*
 * internal functions
 */

/**
 * update_start - starts or stops the timer for morsing
 *
 *		This function starts or stops the timer for morsing
 *		depending on the state of the morse_started global variable
 */
static void update_start(void)
{
	uint8_t before = 0;
	if ((TIMSK2 & (1 << TOIE2)) > 0)	{
		before = 1;
	}
	if ((morse_started & ((1 << MORSE_STARTED_TIME) | (1 << MORSE_STARTED_MINUTE))) == ((1 << MORSE_STARTED_TIME) | (1 << MORSE_STARTED_MINUTE)) || continuous_carrier == TRUE)	{
		TIMSK2 |= (1 << TOIE2);
		if (before == 0)	{
#ifdef DEBUG_MORSE
			D("new morse\r\n");
#endif
			reset = TRUE; /* state of timer has changed */
		}
	} else	{
		TIMSK2 &= ~(1 << TOIE2);
		dds_off();
	}
}

/**
 * is_on_minute - calculates if the given minute is a minute where the device
 *				   should start morsing
 * @minute: the minute which be checked as on-minute
 *
 *		Return: TRUE if device should start morsing at beginning of this minute
 *		or FALSE if device should not start morsing at beginning of this minute.
 */
static uint8_t is_on_minute(uint8_t minute)
{
	uint8_t start_minute = startup_get_start_time(STARTUP_MINUTE);
	int16_t min;
	min = (int16_t)minute - start_minute % current_fox_max;
	if (min < 0)	{
		min = 60 - min;
	}
	if (min % current_fox_max == current_transmit_minute)	{
		return TRUE;
	} else	{
		return FALSE;
	}
}

/**
 * is_off_minute - calculates if the given minute is a minute where the device
 *				   should stop morsing
 * @minute: the minute which be checked as off-minute
 *
 *		Return: TRUE if device should stop morsing at beginning of this minute
 *		or FALSE if device should not stop morsing at beginning of this minute.
 */
static uint8_t is_off_minute(uint8_t minute)
{
	if (minute == 0)	{
		minute = 59;
	} else {
		minute--;
	}
	return is_on_minute(minute);
}

/**
 * get_next_on_minute - returns the next minute when device shall start morsing
 * @minute: tells the function the current minute so that it can calculate the
 *			next minute for the device to start morsing
 *
 *		Return: value of minute when device shall start morsing. This depends
 *		on the fox_number and fox_max values.
 */
static uint8_t get_next_on_minute(uint8_t *minute)
{
	uint8_t start_minute, current_minute, ret;
	start_minute = startup_get_start_time(STARTUP_MINUTE);
	ret = rtc_get_time(RTC_MINUTE, &current_minute);
	if (ret != TWI_OK)	{
		return ret;
	}
	uint8_t next_minute;
	next_minute = (current_fox_max - current_minute % current_fox_max) + current_transmit_minute + start_minute % current_fox_max;
	while (next_minute >= current_fox_max)	{
		next_minute -= current_fox_max;
	}
	next_minute = current_minute + next_minute;
	if (next_minute >= 60)	{
		next_minute -= 60;
	}
	*minute = next_minute;
	return ret;
}

/**
 * get_next_off_minute - returns the next minute when device shall stop morsing
 * @minute: tells the function the current minute so that it can calculate the
 *			next minute for the device to stop morsing
 *
 *		Return: value of minute when device shall stop morsing. This depends
 *		on the fox_number and fox_max values.
 */
static uint8_t get_next_off_minute(uint8_t *minute)
{
	uint8_t current_minute, ret;

	ret = rtc_get_time(RTC_MINUTE, &current_minute);
	if (ret != TWI_OK)	{
		return ret;
	}

	uint8_t next_minute = current_minute + 1;
	if (next_minute >= 60)	{
		next_minute -= 60;
	}
	*minute = next_minute;
	return ret;
}

/**
 * set_rtc_alarm_morse_on - set the rtc alarm for the next start minute
 *
 *		This functions sets the rtc alarm to the value of the next minute
 *		where the device should start morsing.
 *
 *		Return: TWI_OK on success and TWI_ERR on failure of communication with
 *		rtc.
 */
static uint8_t set_rtc_alarm_morse_on(void)
{
	uint8_t next_minute = 0;
	uint8_t ret = get_next_on_minute(&next_minute);
	if (ret != TWI_OK)	{
		return ret;
	}
#ifdef DEBUG_MORSE
	uart_send_text_sram("next start minute: ");
	uart_send_int(next_minute);
	UART_NEWLINE();
#endif
	uint8_t byte;
	rtc_get_alarm1_mask(&byte); /* mask to all set -> needed because otherwise minute cannot be set, if MASK_MINUTE is set, then rtc hangs */
	rtc_set_alarm1_mask(RTC_ALARM_MASK_ALL);
	rtc_set_alarm1_time(RTC_MINUTE, next_minute);
	rtc_set_alarm1_mask(byte);
	return ret;
}

/**
 * set_rtc_alarm_morse_on - set the rtc alarm for the next stop minute
 *
 *		This functions sets the rtc alarm to the value of the next minute
 *		where the device should stop morsing.
 *
 *		Return: TWI_OK on success and TWI_ERR on failure of communication with
 *		rtc.
 */
static uint8_t set_rtc_alarm_morse_off(void)
{
	uint8_t next_minute;
	uint8_t ret = get_next_off_minute(&next_minute);
	if (ret != TWI_OK)	{
		return ret;
	}
	uint8_t byte;
	rtc_get_alarm1_mask(&byte); /* mask to all set -> needed because otherwise minute cannot be set, if MASK_MINUTE is set, then rtc hangs */
	rtc_set_alarm1_mask(RTC_ALARM_MASK_ALL);
	rtc_set_alarm1_time(RTC_MINUTE, next_minute);
	rtc_set_alarm1_mask(byte);
	return ret;
}

/**
 * get_morse_minute_state - returns if device is morsing at the moment
 *
 *		Return: MORSE_MINUTE_IS_START if device is morsing and
 *		MORSE_MINUTE_IS_STOP if device is not morsing.
 */
static uint8_t get_morse_minute_state(void)
{
	if ((morse_started & (1 << MORSE_STARTED_MINUTE)) > 0)	{
		return MORSE_MINUTE_IS_START;
	} else	{
		return MORSE_MINUTE_IS_STOP;
	}
}

/**
 * morse_start_minute - start morsing now
 *
 *		This functions let the device start morsing. It is executed by morse_interrupt
 *		in morse.c when the correct minute is reached.
 */
static void morse_start_minute(void)
{
	morse_started |= (1 << MORSE_STARTED_MINUTE);
	dds_powerup();
	update_start();
}

/**
 * morse_stop_minute - stop morsing now
 *
 *		This function let the device stop morsing. It is executed by morse_interrupt
 *		in morse.c depending on the minute and fox_number an fox_max
 */
static void morse_stop_minute(void)
{
	morse_started &= ~(1 << MORSE_STARTED_MINUTE);
	dds_powerdown();
	update_start();
}

/*
 * public functions
 */

/**
 * morse_init - initialise morse module
 */
void morse_init()
{
	TCCR2B = (1 << CS22) | (1 << CS21); /* prescaler = 256 */
	TCNT2 = TIMER2_PRELOAD;
	/* TIMSK will get set by the update_start routine */

	rtc_set_alarm1_mask(RTC_ALARM_MASK_MINUTES);

	morse_load_configuration();
}

/**
 * morse_show_configuration - output configuration of morse module to uart
 */
void morse_show_configuration()
{
	uint8_t call = morse_get_call_sign();

	uart_send_text_flash((uint16_t)fxn);
	uint8_t fox_number = morse_get_fox_number();
	if (fox_number == FOX_NUMBER_DEMO)	{
		uart_send_text_sram("Demo");
	} else {
		uart_send_int(morse_get_fox_number());
	}
	UART_NEWLINE();

	uart_send_text_flash((uint16_t)transmit_minute_msg);
	uart_send_int(morse_get_transmit_minute());
	UART_NEWLINE();

	uart_send_text_flash((uint16_t)cfm);
	uart_send_int(morse_get_fox_max());
	UART_NEWLINE();

	uart_send_text_sram("Morsing speed: ");
	uart_send_int(morse_convert_morse_unit_wpm(current_morse_unit));
	uart_send_text_sram(" wpm");
	UART_NEWLINE();

	uart_send_text_flash((uint16_t)call_sign_number_text);
	uart_send_text_flash((uint16_t)morse_text[call]);
	UART_NEWLINE();

	uart_send_text_flash((uint16_t)morse_mode_text);
	if (current_morsing_enabled == TRUE)	{
		uart_send_text_sram("on");
	} else {
		uart_send_text_sram("off");
	}
	UART_NEWLINE();
}

/**
 * morse_load_configuration - load configuration in eeprom to ram
 */
void morse_load_configuration()
{
	current_morse_unit = morse_get_morse_unit();
	current_fox_number = morse_get_fox_number();
	current_transmit_minute = morse_get_transmit_minute();
	current_fox_max = morse_get_fox_max();
	current_morsing_enabled = morse_get_morse_mode();

	uint8_t i, call = morse_get_call_sign();
	for (i = 0; i <= CALL_MAX; i++)	{
		current_call[i] = pgm_read_byte(&morse_code[call][i]);
	}
}

/**
 * morse_start_time - starts the morse module
 *
 *		This functions starts the morsing module. It means that we are between
 *		start and stop time. It does not mean that device is morsing at this
 *		moment because this depends on the minute!
 *		The function is executed by startup.c when start time is reached.
 *
 *		If the current minute is also a start minute -> morsing will be started.
 */
void morse_start_time()
{
	morse_started |= (1 << MORSE_STARTED_TIME);

	uint8_t current_minute;
	rtc_get_time(RTC_MINUTE, &current_minute);
	if (is_on_minute(current_minute) == TRUE)	{
		morse_start_minute();
		set_rtc_alarm_morse_off();
	} else {
		morse_stop_minute();
		set_rtc_alarm_morse_on();
	}
	update_start();
}

/**
 * morse_stop_time - stops the morse module
 *
 *		This functions stops the morse module. The function is executed by
 *		startup.c when stop time is reached.
 */
void morse_stop_time()
{
	morse_started &= ~(1 << MORSE_STARTED_TIME);
	update_start();
}

/**
 * morse_convert_wpm_morse_unit - calculate the morse_unit out of words per minute
 * @wpm:	words per minute value
 *
 *		Return: the morse unit (time in 100us)
 */
uint16_t morse_convert_wpm_morse_unit(uint16_t wpm)
{
	return BASE_WPM / wpm;
}

/**
 * morse_convert_morse_unit_wpm - calculate the wpm out of morse_unit
 * @morse_unit: internal morse unit to convert to wpm
 *
 *		Return: the wpm value
 */
uint16_t morse_convert_morse_unit_wpm(uint16_t morse_unit)
{
	return BASE_WPM / morse_unit;
}

/**
 * morse_set_morse_unit - stores given morse unit in eeprom
 * @morse_unit_value:	the time for a did divided by 100us
 */
void morse_set_morse_unit(uint16_t morse_unit_value)
{
	eeprom_write_word(&morse_unit_eemem, morse_unit_value);
}

/**
 * morse_get_morse_unit - returns the morse unit saved in eeprom
 *
 *		Return: the morse unit (returns the time divided by 100us)
 */
uint16_t morse_get_morse_unit()
{
	uint16_t unit = eeprom_read_word(&morse_unit_eemem);
	uint16_t wpm = morse_convert_morse_unit_wpm(unit);
	if (wpm > MORSE_WPM_MAX || wpm < MORSE_WPM_MIN)	{
		return morse_convert_wpm_morse_unit(MORSE_WPM_DEFAULT);
	}
	return unit;
}

/**
 * morse_set_fox_number - set the number of this transmitter
 * @fox_number_local:	the number of this transmitter to set
 *
 *		The fox number ist to recognise the fox for the pc software
 *
 *		Return: TRUE if fox number was in correct range, FALSE if fox number
 *		could not be set because it is not in the correct range
 */
uint8_t morse_set_fox_number(uint8_t fox_number_local)
{
	if (fox_number_local > FOX_NUMBER_MAX)	{
		return FALSE;
	}
	eeprom_write_byte(&fox_number_eemem, fox_number_local);
	return TRUE;
}

/**
 * morse_get_fox_number - returns number of this transmitter
 *
 *		Return: number of this transmitter, if transmitter is demo fox ->
 *		zero is returned
 */
uint8_t morse_get_fox_number(void)
{
	uint8_t byte = eeprom_read_byte(&fox_number_eemem);
	if (byte > FOX_NUMBER_MAX)	{
		byte = FOX_NUMBER_DEMO;
	}
	return byte;
}

/**
 * morse_set_transmit_minute - set the minute where the fox will transmit
 * @transmit_minute_local - minute between 0 and (get_fox_max() - 1)
 *
 *		The transmit minute is to determine when the fox should start
 *		transmiting and when it should stop
 *
 *		Return: TRUE if transmit_minute was in correct range, FALSE
 *		if transmit_minute could not be set because it was in wrong range
 */
uint8_t morse_set_transmit_minute(uint8_t transmit_minute_local)
{
	if (transmit_minute_local > TRANSMIT_MINUTE_MAX)	{
		return FALSE;
	}
	eeprom_write_byte(&transmit_minute_eemem, transmit_minute_local);
	return TRUE;
}

/**
 * morse_get_transmit_minute - get the minute where the fox will transmit
 *
 *		Return: the minute where fox will transmit (between 0 and (fox_max - 1))
 */
uint8_t morse_get_transmit_minute(void)
{
	uint8_t byte = eeprom_read_byte(&transmit_minute_eemem);
	if (byte >= morse_get_fox_max())	{
		byte = 0;
	}
	return byte;
}

/**
 * morse_get_fox_max - returns the current set number of transmitters used
 *
 *		Return: the number of transmitters used during this challenge
 */
uint8_t morse_get_fox_max(void)
{
	uint8_t byte = eeprom_read_byte(&fox_max_eemem);
	if (byte > FOX_MAX_MAX || byte < FOX_MAX_MIN)	{
		byte = FOX_MAX_MAX;
	} else if (byte == 0)	{
		byte = FOX_MAX_DEFAULT;
	}
	return byte;
}

/**
 * morse_set_fox_max - set the number of transmitters used during the challenge
 * @max:	number of transmitters used
 *
 *		The number of foxes is needed for selecting the time when this transmitter
 *		will start and stop morsing
 *
 *		Return: TRUE if fox_max was in correct range, FALSE if fox_max could
 *		not be set because it is in the wrong range
 */
uint8_t morse_set_fox_max(uint8_t max)
{
	if (max > FOX_MAX_MAX || max < FOX_MAX_MIN)	{
		return FALSE;
	}
	eeprom_write_byte(&fox_max_eemem, max);
	return TRUE;
}

/**
 * morse_set_call_sign - set the call sign of the transmitter
 * @call:	either CALL_MOE, CALL_MOI etc.
 *			(refer to the definitions in morse.h)
 */
void morse_set_call_sign(uint8_t call)
{
	eeprom_write_byte(&call_number_eemem, call);
}

/**
 * morse_get_call_sign - returns the in eeprom saved call sign
 *
 *		Return: number of the call sign selected, either
 *		CALL_MOE, CALL_MOI etc. (refer to morse.h definitions)
 */
uint8_t morse_get_call_sign(void)
{
	uint8_t byte = eeprom_read_byte(&call_number_eemem);
	if (byte > CALL_MAX)	{
		byte = 0;
	}
	return byte;
}

/**
 * morse_set_morse_mode - turn morsing on or off
 * @mode:	if mode is TRUE morsing is enabled
 *			if mode is FALSE a continuous carrier will be sent out
 */
void morse_set_morse_mode(uint8_t mode)
{
	eeprom_write_byte(&morsing_enabled, mode);
}

/**
 * morse_get_morse_mode - returns morsing mode
 *
 *		Return: TRUE if morsing is activated on principle (this must not mean
 *		that the device is morsing at the moment because this depends on start
 *		and stop time), returns FALSE if device transmit a continuous carrier
 *		instead of morsing
 */
uint8_t morse_get_morse_mode(void)
{
	uint8_t byte = eeprom_read_byte(&morsing_enabled);
	if (byte != FALSE)	{
		byte = TRUE;
	}
	return byte;
}

/**
 * morse_enable_continuous_carrier - transmit continuous carrier independent from start minute
 */
void morse_enable_continuous_carrier(void)
{
	continuous_carrier = TRUE;
	dds_on();
	update_start();
}

/**
 * morse_disable_continuous_carrier - stop transmiting continuous carrier and set
 *									  set back to state of start minute
 */
void morse_disable_continuous_carrier(void)
{
	continuous_carrier = FALSE;
	update_start();
}

ISR(TIMER2_OVF_vect)
{
#ifdef ISR_LED
	LED_ON();
#endif

	TCNT2 = TIMER2_PRELOAD;
	if (current_morsing_enabled == FALSE || continuous_carrier == TRUE)	{
		/* so that dds_on is not executed that often (dds has problems
		 * with too many requests!)
		 */
		static uint8_t count;
		count++;
		if (count == 0)	{
			dds_on();
		}
	}	else {
		static uint8_t call_byte_index = 0;
		static uint8_t call_inner_byte_index = 0;
		static uint8_t time = 0;
		static uint16_t morse_unit = 0;
		if (reset == TRUE)	{
			/* to ensure that when timer is activated */
			call_byte_index = 0;
			call_inner_byte_index = 0;
			time = 0;
			morse_unit = 0;
			reset = FALSE;
		}

		if (morse_unit == 0)	{
			morse_unit = current_morse_unit;

			if (time == 1)	{
				/* the space between symbols */
				dds_off();
				time--;
			} else if (time == 0)	{
				uint8_t inc = FALSE;
				switch ((current_call[call_byte_index] >> ((3 - call_inner_byte_index) * 2)) & 0b11)	{
					case 0b00:
						/* dot */
						time = 1 + 1;
						dds_on();
						inc = TRUE;
						break;
					case 0b01:
						/* dash */
						time = 3 + 1;
						dds_on();
						inc = TRUE;
						break;
					case 0b10:
						/* space letter */
						time = 3 - 1;
						inc = TRUE;
						break;
					case 0b11:
						/* space word */
						time = 7 - 1;
						call_inner_byte_index = 0;
						call_byte_index = 0;
						break;
				}
				if (inc == TRUE)	{
					call_inner_byte_index++;
					if (call_inner_byte_index > 3)	{
						call_inner_byte_index = 0;
						call_byte_index++;
						if (call_byte_index > CALL_MAX)	{
							call_byte_index = 0;
						}
					}
				}
			} else {
				time--;
			}
		} else {
			morse_unit--;
		}
	}
#ifdef ISR_LED
	LED_OFF();
#endif
}

/**
 * morse_interrupt - check the current minute and switch on or off morsing
 *
 *		this function is called by rtc.c when the an interrupt by the rtc
 *		occurs. The function checks if it is now time to start or stop
 *		morsing (depends on the fox_number during which minute the transmitter
 *		has to send its call sign)
 */
void morse_interrupt(void)
{
	uint8_t current_minute;
	rtc_get_time(RTC_MINUTE, &current_minute);
	if (is_on_minute(current_minute) == TRUE)	{
		morse_start_minute();
		set_rtc_alarm_morse_off();
		uart_send_text_sram("morse start\r\n");
	} else if (is_off_minute(current_minute) == TRUE)	{
		morse_stop_minute();
		set_rtc_alarm_morse_on();
		uart_send_text_sram("morse stop\r\n");
	} else	{
		if (get_morse_minute_state() == MORSE_MINUTE_IS_START)	{
			set_rtc_alarm_morse_off();
			uart_send_text_sram("morse start2\r\n");
		} else {
			set_rtc_alarm_morse_on();
			uart_send_text_sram("morse stop2\r\n");
		}
	}
}
