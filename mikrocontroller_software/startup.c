/*
 *  startup.c - functions for changing the startup time of the transmitter
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
#include <util/delay.h>
#include <avr/pgmspace.h>

#include "main.h"
#include "morse.h"
#include "startup.h"
#include "uart.h"
#include "rtc.h"
#include "user.h"
#include "utils.h"

#define START_TIME		0
#define STOP_TIME		0

uint8_t EEMEM start_time[STARTUP_TIME_MAX + 1];
uint8_t EEMEM stop_time[STARTUP_TIME_MAX + 1];

const char PROGMEM start_time_text[] = "Start time: ";
const char PROGMEM start_date_text[] = "Start date: ";
const char PROGMEM stop_time_text[] = "Stop time: ";
const char PROGMEM stop_date_text[] = "Stop date: ";

#define IS_NOT_SET						0
#define IS_BETWEEN_START_AND_STOP		1
#define IS_NOT_BETWEEN_START_AND_STOP	2

uint8_t type_max[STARTUP_TIME_MAX + 1] = {59, 59, 23, 31, 12, 99};
/* second, minute, hour, date, month, weekday */


/*
 * internal functions
 */

/**
 * is_time_between_start_and_stop - checks if time is between start and stop time
 *
 *		Return: TRUE if time is between start and stop time, false otherwise.
 *		If time == start_time then TRUE will be returned.
 *		If time == stop_time then FALSE will be returned.
 */
uint8_t is_time_between_start_and_stop(void)
{
	uint8_t is_started = TRUE; /* if competition is already started while power up */
	int8_t j;
	for (j = STARTUP_YEAR; j >= STARTUP_SECOND; j--)	{
		/* do not compare weekday because it is not needed! */
		uint8_t time, start_time;
		start_time = startup_get_start_time(j);
		rtc_get_time(j, &time);
		if (time > start_time)	{
			break; /* if it is in between start and stop time it is started */
			/*
			 * if it is same as start_time or stop_time in this type then the
			 * next type has to be checked too -> continue loop
			 */
		} else if(time != start_time)	{
			is_started = FALSE;
			break;
			/* if it is outside start and stop time it is not started */
		}
	}
	for (j = STARTUP_YEAR; j >= STARTUP_SECOND; j--)	{
		/* do not compare weekday because it is not needed! */
		uint8_t time, stop_time;
		stop_time = startup_get_stop_time(j);
		rtc_get_time(j, &time);
		if (time < stop_time)	{
			break; /* if it is in between start and stop time it is started */
			/*
			 * if it is same as start_time or stop_time in this type then the
			 * next type has to be checked too -> continue loop
			 */
		} else if(time != stop_time)	{
			is_started = FALSE;
			break;
			/* if it is outside start and stop time it is not started */
		}
		if (j == STARTUP_SECOND && time == stop_time) {
			/* so that if time == stop time false is returned! */
			is_started = FALSE;
		}
	}
	return is_started;
}

/**
 * set_rtc_alarm_start_time - set the rtc alarm to start time value
 *
 *		Return: TWI_OK on success and TWI_ERR on failure
 */
static uint8_t set_rtc_alarm_start_time(void)
{
	uint8_t type;
	uint8_t ret;
	for (type = STARTUP_SECOND; type < STARTUP_YEAR; type++)	{
		uint8_t time = startup_get_start_time(type);
		ret = rtc_set_alarm0_time(type, time);
		if (ret != TWI_OK)	{
			return ret;
		}
	}
	uint8_t time = startup_get_start_time(STARTUP_WEEKDAY);
	ret = rtc_set_alarm0_time(STARTUP_WEEKDAY, time);
	return ret;
}

/**
 * set_rtc_alarm_stop_time - set the rtc alarm to stop time value
 *
 *		Return: TWI_OK on success an TWI_ERR on failure
 */
static uint8_t set_rtc_alarm_stop_time(void)
{
	uint8_t type;
	uint8_t ret;
	for (type = STARTUP_SECOND; type < STARTUP_YEAR; type++)	{
		uint8_t time = startup_get_stop_time(type);
		ret = rtc_set_alarm0_time(type, time);
		if (ret != TWI_OK)	{
			return ret;
		}
	}
	uint8_t time = startup_get_stop_time(STARTUP_WEEKDAY);
	ret = rtc_set_alarm0_time(STARTUP_WEEKDAY, time);
	return ret;
}

/**
 * check_start_time - check if time is between start time and turn on or off the
 *					  morsing
 */
static void check_start_time(void)
{
	static uint8_t last_mode = IS_NOT_SET;

	rtc_disable_alarm0();

	if (is_time_between_start_and_stop() == TRUE && last_mode != IS_BETWEEN_START_AND_STOP)	{
#ifdef DEBUG_START_TIME
		DP(start_time_text);
#endif
		set_rtc_alarm_stop_time();
		morse_start_time();
		main_start_time();
		user_start_time();
		last_mode = IS_BETWEEN_START_AND_STOP;
	} else if (is_time_between_start_and_stop() != TRUE && last_mode != IS_NOT_BETWEEN_START_AND_STOP)	{
#ifdef DEBUG_START_TIME
		DP(stop_time_text);
#endif
		morse_stop_time();
		main_stop_time();
		user_stop_time();
		set_rtc_alarm_start_time();
		last_mode = IS_NOT_BETWEEN_START_AND_STOP;
	}

	rtc_enable_alarm0();
}

/*
 * public functions
 */

/**
 * startup_init - initialise startup module
 */
void startup_init(void)
{
	rtc_set_alarm0_mask(RTC_ALARM_MASK_DATE);

	startup_load_configuration();

	check_start_time();
}

/**
 * startup_show_configuration - output configuartion of startup module
 */
void startup_show_configuration(void)
{
	uint8_t i;
	char time[3][3];
	char date[3][3];
	for (i = STARTUP_SECOND; i <= STARTUP_HOUR; i++)	{
		uint8_t time_temp = startup_get_start_time(i);
		if (int_to_string_fixed_length(&(time[i][0]), 3, time_temp) != TRUE);
	}
	for (i = STARTUP_DATE; i <= STARTUP_YEAR; i++)	{
		uint8_t date_temp = startup_get_start_time(i);
		int_to_string_fixed_length(date[i - STARTUP_DATE], 3, date_temp);
	}

	uart_send_text_flash((uint16_t)start_time_text);
	uart_send_text_buffer(time[2]);
	uart_send_text_sram(":");
	uart_send_text_buffer(time[1]);
	uart_send_text_sram(":");
	uart_send_text_buffer(time[0]);
	UART_NEWLINE();

	uart_send_text_flash((uint16_t)start_date_text);
	uart_send_text_sram("20"); /* for 20xx (year between 2000 and 2099) */
	uart_send_text_buffer(date[2]);
	uart_send_text_sram("-");
	uart_send_text_buffer(date[1]);
	uart_send_text_sram("-");
	uart_send_text_buffer(date[0]);
	UART_NEWLINE();

	for (i = STARTUP_SECOND; i <= STARTUP_HOUR; i++)	{
		uint8_t time_temp = startup_get_stop_time(i);
		int_to_string_fixed_length(time[i], 3, time_temp);
	}
	for (i = STARTUP_DATE; i <= STARTUP_YEAR; i++)	{
		uint8_t date_temp = startup_get_stop_time(i);
		int_to_string_fixed_length(date[i - STARTUP_DATE], 3, date_temp);
	}

	uart_send_text_flash((uint16_t)stop_time_text);
	uart_send_text_buffer(time[2]);
	uart_send_text_sram(":");
	uart_send_text_buffer(time[1]);
	uart_send_text_sram(":");
	uart_send_text_buffer(time[0]);
	UART_NEWLINE();

	uart_send_text_flash((uint16_t)stop_date_text);
	uart_send_text_sram("20"); /* for 20xx (year between 2000 and 2099) */
	uart_send_text_buffer(date[2]);
	uart_send_text_sram("-");
	uart_send_text_buffer(date[1]);
	uart_send_text_sram("-");
	uart_send_text_buffer(date[0]);
	UART_NEWLINE();

}

/**
 * startup_load_configuration - load configuration in eeprom to ram
 */
void startup_load_configuration(void)
{
	/* nothing to do */
}

/**
 * startup_set_start_time - save time of competition start in eeprom
 * @type: defines the type of the time element (STARTUP_SECOND, etc.)
 * @time: the value to save
 *
 *		Return: TRUE if time value was in correct range, FALSE otherwise
 */
uint8_t startup_set_start_time(uint8_t type, uint8_t time)
{
	if (type > STARTUP_TIME_MAX)	{
		return FALSE;
	}
	if (time > type_max[type])	{
		return FALSE;
	}
	if (type == STARTUP_DATE || type == STARTUP_MONTH)	{
		if (time == 0)	{
			return FALSE;
		}
	}
	eeprom_write_byte(&start_time[type], time);
	return TRUE;
}

/**
 * startup_set_stop_time - save time of competition stop in eeprom
 * @type: defines the type of the time element (STARTUP_SECOND, etc.)
 * @time: the value to save
 *
 *		Return: TRUE if time value was in correct range, FALSE otherwise
 */
uint8_t startup_set_stop_time(uint8_t type, uint8_t time)
{
	if (type > STARTUP_TIME_MAX)	{
		return FALSE;
	}
	if (time > type_max[type])	{
		return FALSE;
	}
	if (type == STARTUP_DATE || type == STARTUP_MONTH)	{
		if (time == 0)	{
			return FALSE;
		}
	}
	eeprom_write_byte(&stop_time[type], time);
	return TRUE;
}

/**
 * startup_get_start_time - read time when competition ends
 * @type: type of time information like STARTUP_SECOND etc.
 *
 *		Return: value of start time defined by type (second, hour, date etc.)
 */
uint8_t startup_get_start_time(uint8_t type)
{
	uint8_t byte = eeprom_read_byte(&start_time[type]);
	if (byte > type_max[type])	{
		byte = type_max[type];
	}
	if (type == STARTUP_MONTH || type == STARTUP_DATE)	{
		if (byte == 0){
			byte = 1;
		}
	}
	return byte;
}

/**
 * startup_get_stop_time - read time when competition ends
 * @type: type of time information like STARTUP_SECOND etc.
 *
 *		Return: value of stop time defined by type (second, hour, date etc.)
 */
uint8_t startup_get_stop_time(uint8_t type)
{
	uint8_t byte = eeprom_read_byte(&stop_time[type]);
	if (byte > type_max[type])	{
		byte = type_max[type];
	}
	if (type == STARTUP_MONTH || type == STARTUP_DATE)	{
		if (byte == 0){
			byte = 1;
		}
	}
	return byte;
}

/**
 * startup_interrupt - function is called by rtc-interrupt to indicate rtc alarm
 */
void startup_interrupt()
{
	uint8_t byte;
	rtc_get_alarm0_mask(&byte);
	if (byte == RTC_ALARM_MASK_DATE)	{
		rtc_set_alarm0_mask(RTC_ALARM_MASK_HOURS);
	} else if (byte == RTC_ALARM_MASK_HOURS)	{
		rtc_set_alarm0_mask(RTC_ALARM_MASK_MINUTES);
	} else if (byte == RTC_ALARM_MASK_MINUTES)	{
		rtc_set_alarm0_mask(RTC_ALARM_MASK_SECONDS);
	} else {
		rtc_set_alarm0_mask(RTC_ALARM_MASK_DATE);
	}

	check_start_time();
}
