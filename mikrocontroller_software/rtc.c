/*
 *  rtc.c - functions for accessing the real time clock chip MCP79410
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

#include "pins.h"
#include "rtc.h"
#include "uart.h"
#include "twi.h"
#include "main.h"
#include "startup.h"
#include "morse.h"
#include "utils.h"

/* rtc i2c address */
#define RTC_ADDRESS			0b11011110
#define RTC_ADDRESS_W		0b11011110
#define RTC_ADDRESS_R		0b11011111

/* rtc register addresses */
#define RTC_RTCSEC		0x00
#define RTC_RTCMIN		0x01
#define RTC_RTCHOUR		0x02
#define RTC_RTCWKDAY	0x03
#define RTC_RTCDATE		0x04
#define RTC_RTCMONTH	0x05
#define RTC_RTCYEAR		0x06
#define RTC_CONTROL		0x07
#define RTC_ALM0SEC		0x0A
#define RTC_ALM0WKDAY	0x0D
#define RTC_ALM1SEC		0x11
#define RTC_ALM1WKDAY	0x14

/* bit numbers in rtc registers */
/* in register RTCSEC */
#define RTC_ST			7 /* bit 7 */

/* in register RTCHOUR */
#define RTC_12_24		6

/* in register RTCWKDAY */
#define RTC_VBATEN		3

/* in register CONTROL */
#define RTC_ALM0EN		4
#define RTC_ALM1EN		5

/* in register ALM0WKDAY */
#define RTC_ALM0IF			3
#define RTC_ALM0MSK0		4
#define RTC_ALMPOL			7

/* in register ALM1WKDAY */
#define RTC_ALM1IF			3
#define RTC_ALM1MSK0		4
/* ALMPOL in ALM1WKDAY is the same as in ALM0WKDAY! */



uint8_t interrupt_was_enabled = FALSE;
volatile uint8_t during_bitmask = FALSE;

/*
 * internal functions
 */

/**
 * rtc_write_register() - write a register of real time clock
 * @address:	address of the real time clock register
 * @byte:		byte to write into the register
 *
 * Return: TWI_OK on success, TWI_ERR on error
 **/
uint8_t rtc_write_register(uint8_t address, uint8_t byte)
{
	rtc_disable_avr_interrupt();
	uint8_t ret = 0;
	ret |= twi_start();
	if (ret != TWI_OK)
		goto rtc_write_register_exit;
	ret |= twi_send_slave_address(TWI_WRITE, RTC_ADDRESS);
	if (ret != TWI_OK)
		goto rtc_write_register_exit;
	ret |= twi_send_byte(address);
	if (ret != TWI_OK)
		goto rtc_write_register_exit;
	ret |= twi_send_byte(byte);
	if (ret != TWI_OK)
		goto rtc_write_register_exit;
	twi_stop();
rtc_write_register_exit:
	if (during_bitmask == FALSE)	{
		rtc_enable_avr_interrupt();
	}
	return ret;
}

/**
 * rtc_read_register() - read a register of realt time clock
 *
 * @address:	address of realt time clock register
 * @byte:		pointer to address where the read byte will be stored in
 *
 * Return: TWI_OK on success, TWI_ERR on error
 */
uint8_t rtc_read_register(uint8_t address, uint8_t* byte)
{
	rtc_disable_avr_interrupt();
	uint8_t ret = 0;
	ret |= twi_start();
	if (ret != TWI_OK)
		goto rtc_read_register_exit;
	ret |= twi_send_slave_address(TWI_WRITE, RTC_ADDRESS);
	if (ret != TWI_OK)
		goto rtc_read_register_exit;
	ret |= twi_send_byte(address);
	if (ret != TWI_OK)
		goto rtc_read_register_exit;
	ret |= twi_start();
	if (ret != TWI_OK)
		goto rtc_read_register_exit;
	ret |= twi_send_slave_address(TWI_READ, RTC_ADDRESS);
	if (ret != TWI_OK)
		goto rtc_read_register_exit;
	ret |= twi_read_byte(TWI_NACK, byte);
	twi_stop();
rtc_read_register_exit:
	if (during_bitmask == FALSE)	{
		rtc_enable_avr_interrupt();
	}
	return ret;
}

/**
 * rtc_set_bitmask - set a bit in a register of the rtc chip
 * @address:	address of the rtc register
 * @bitmask:		bitmask to set
 *
 *		Return: TWI_OK on success and TWI_ERR on failure
 */
uint8_t rtc_set_bitmask(uint8_t address, uint8_t bitmask)
{
	rtc_disable_avr_interrupt();
	during_bitmask = TRUE;
	uint8_t temp;
	uint8_t ret = rtc_read_register(address, &temp);
	if (ret != TWI_OK)
		goto rtc_set_bitmask_exit;
	uint8_t write_val = bitmask | temp;
	ret = rtc_write_register(address, write_val);
	if (ret != TWI_OK)
		goto rtc_set_bitmask_exit;
rtc_set_bitmask_exit:
	rtc_enable_avr_interrupt();
	during_bitmask = FALSE;
	return ret;
}

/**
 * rtc_clear_bitmask - clear a bit in a register of the rtc chip
 * @address:	address of the rtc register
 * @bitmask:	bitmask to clear
 *
 *		Return: TWI_OK on success and TWI_ERR on failure
 */
uint8_t rtc_clear_bitmask(uint8_t address, uint8_t bitmask)
{
	rtc_disable_avr_interrupt();
	during_bitmask = TRUE;
	uint8_t temp;
	uint8_t ret = rtc_read_register(address, &temp);
	if (ret != TWI_OK)
		goto rtc_clear_bitmask_exit;
	uint8_t write_val = (~bitmask) & temp;
	ret = rtc_write_register(address, write_val);
	if (ret != TWI_OK)
		goto rtc_clear_bitmask_exit;
rtc_clear_bitmask_exit:
	rtc_enable_avr_interrupt();
	during_bitmask = FALSE;
	return ret;
}

/**
 * get_one - calculate unit position of a given value
 * @val:	the given value
 *
 *		Return: the unit value of val
 */
static uint8_t get_one(uint8_t val)
{
	return val % 10;
}

/**
 * get_ten - calculate the tens digit of a given value
 * @val:	the given value
 *
 *		Return: the tens digit of val
 */
static uint8_t get_ten(uint8_t val)
{
	val /= 10;
	return val % 10;
}

/**
 * rtc_set_time_internal() - set time or date or alarm date and time
 *
 * @type:	what should be set (RTC_SECOND, RTC_MINUTE, RTC_YEAR ...)
 * @offset:	offset of the SECOND register (set for alarm because the registers
 *			have offsets and are not equal to RTC_SECOND)
 * @val:	value to set as binary value (not bcd)
 *
 *		Backend for rtc_set_time and rtc_set_alarm0_time
 *
 *		Return: TWI_OK on success, TWI_ERR on error
 */
static uint8_t rtc_set_time_internal(uint8_t type, uint8_t offset, uint8_t val)
{
	uint8_t ret = 0;
	uint8_t one = get_one(val);
	uint8_t ten = get_ten(val);

	uint8_t temp;
	uint8_t write_val;
	switch (type)	{
		case RTC_SECOND:
			ret |= rtc_read_register(RTC_RTCSEC + offset, &temp);
			if (ret != TWI_OK)
				return ret;
			write_val = (temp & 0b10000000) | ((ten & 0b111) << 4) | one;
			ret |= rtc_write_register(RTC_RTCSEC + offset, write_val);
			if (ret != TWI_OK)
				return ret;
			break;
		case RTC_MINUTE:
			write_val = ((ten & 0b111) << 4) | one;
			ret |= rtc_write_register(RTC_RTCMIN + offset, write_val);
			if (ret != TWI_OK)
				return ret;
			break;
		case RTC_HOUR:
			ret |= rtc_read_register(RTC_RTCHOUR + offset, &temp);
			if (ret != TWI_OK)
				return ret;
			write_val = (temp & 0b01000000) | ((ten & 0b11) << 4) | one;
			ret |= rtc_write_register(RTC_RTCHOUR + offset, write_val);
			if (ret != TWI_OK)
				return ret;
			break;
		case RTC_WEEKDAY:
			ret |= rtc_read_register(RTC_RTCWKDAY + offset, &temp);
			if (ret != TWI_OK)
				return ret;
			write_val = (temp & 0b11111000) | one;
			ret |= rtc_write_register(RTC_RTCWKDAY + offset, write_val);
			if (ret != TWI_OK)
				return ret;
			break;
		case RTC_DATE:
			write_val = ((ten & 0b11) << 4) | one;
			ret |= rtc_write_register(RTC_RTCDATE + offset, write_val);
			if (ret != TWI_OK)
				return ret;
			break;
		case RTC_MONTH:
			ret |= rtc_read_register(RTC_RTCMONTH + offset, &temp);
			if (ret != TWI_OK)
				return ret;
			write_val = (temp & 0b00100000) | ((ten & 0b1) << 4) | one;
			ret |= rtc_write_register(RTC_RTCMONTH + offset, write_val);
			if (ret != TWI_OK)
				return ret;
			break;
		case RTC_YEAR:
			write_val = (ten << 4) | one;
			ret |= rtc_write_register(RTC_RTCYEAR + offset, write_val);
			if (ret != TWI_OK)
				return ret;
			break;
	}
	return ret;
}

/**
 * rtc_get_time_internal - read time or datevalue from rtc
 * @type:	type of date value to read (RTC_SECOND, RTC_HOUR...)
 * @offset:	offset of the SECOND register (set for alarm because the registers
 *			have offsets and are not equal to RTC_SECOND)
 * @val:	pointer to a variable were the read value will be stored
 *
 *		Backend for rtc_get_time and rtc_get_alarm0_time
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
static uint8_t rtc_get_time_internal(uint8_t type, uint8_t offset, uint8_t *val)
{
	uint8_t ret = 0;
	uint8_t temp;

	uint8_t ten;
	uint8_t one;

	switch (type)	{
		case RTC_SECOND:
			ret |= rtc_read_register(RTC_RTCSEC + offset, &temp);
			if (ret != TWI_OK)
				return ret;
			ten = (temp & (0b111 << 4)) >> 4;
			one = temp & 0b1111;
			*val = ten * 10 + one;
			break;
		case RTC_MINUTE:
			ret |= rtc_read_register(RTC_RTCMIN + offset, &temp);
			if (ret != TWI_OK)
				return ret;
			ten = (temp & (0b111 << 4)) >> 4;
			one = temp & 0b1111;
			*val = ten * 10 + one;
			break;
		case RTC_HOUR:
			ret |= rtc_read_register(RTC_RTCHOUR + offset, &temp);
			if (ret != TWI_OK)
				return ret;
			ten = (temp & (0b11 << 4)) >> 4;
			one = temp & 0b1111;
			*val = ten * 10 + one;
			break;
		case RTC_WEEKDAY:
			ret |= rtc_read_register(RTC_RTCWKDAY + offset, &temp);
			if (ret != TWI_OK)
				return ret;
			*val = ret & (1 << 0b111);
			break;
		case RTC_DATE:
			ret |= rtc_read_register(RTC_RTCDATE + offset, &temp);
			if (ret != TWI_OK)
				return ret;
			ten = (temp & (0b11 << 4)) >> 4;
			one = temp & 0b1111;
			*val = ten * 10 + one;
			break;
		case RTC_MONTH:
			ret |= rtc_read_register(RTC_RTCMONTH + offset, &temp);
			if (ret != TWI_OK)
				return ret;
			ten = (temp & (0b1 << 4)) >> 4;
			one = temp & 0b1111;
			*val = ten * 10 + one;
			break;
		case RTC_YEAR:
			ret |= rtc_read_register(RTC_RTCYEAR + offset, &temp);
			if (ret != TWI_OK)
				return ret;
			ten = (temp & (0b1111 << 4)) >> 4;
			one = temp & 0b1111;
			*val = ten * 10 + one;
			break;
	}
	return ret;
}

/*
 * public functions
 */

/**
 * rtc_init - initializes the real time clock chip
 */
void rtc_init()
{
	EICRA |= (1 << ISC11) | (1 << ISC10); /* rising edge at int1 causes an interrupt */
	interrupt_was_enabled = TRUE;
	rtc_enable_avr_interrupt();

	rtc_set_alarm_polarity(RTC_POLARITY_ACTIVE_HIGH);

	rtc_clear_alarm0_interrupt_flag();
	rtc_enable_alarm0();

	rtc_clear_alarm1_interrupt_flag();
	rtc_enable_alarm1();

	rtc_load_configuration();
}

/**
 * rtc_show_configuration - output configuration to uart
 */
void rtc_show_configuration(void)
{
	uart_send_text_sram("Time: ");
	uint8_t i;
	char time[3][3];
	for (i = RTC_SECOND; i < (RTC_SECOND + 3); i++)	{
		uint8_t time_temp;
		rtc_get_time(i, &time_temp);
		int_to_string_fixed_length(time[i], 3, time_temp);
	}

	uart_send_text_buffer(time[2]);
	uart_send_text_sram(":");
	uart_send_text_buffer(time[1]);
	uart_send_text_sram(":");
	uart_send_text_buffer(time[0]);
	UART_NEWLINE();

	uart_send_text_sram("Date: ");
	for (i = RTC_DATE; i < (RTC_DATE + 3); i++)	{
		uint8_t time_temp;
		rtc_get_time(i, &time_temp);
		int_to_string_fixed_length(time[i - RTC_DATE], 3, time_temp);
	}

	uart_send_text_sram("20");
	uart_send_text_buffer(time[2]);
	uart_send_text_sram("-");
	uart_send_text_buffer(time[1]);
	uart_send_text_sram("-");
	uart_send_text_buffer(time[0]);
	UART_NEWLINE();


}

/**
 * rtc_load_configuration - load the configuration from eeprom
 */
void rtc_load_configuration(void)
{
	/* nothing to do */
}

/**
 * rtc_enable_24h - enables 24 hour mode in rtc chip
 */
uint8_t rtc_enable_24h()
{
	return rtc_clear_bitmask(RTC_RTCHOUR, (1 << RTC_12_24));
}

/**
 * rtc_disable_24h - disables 24 hour mode and switch to 12 hour mode
 *
 *		Return: TWI_OK on success and TWI_ERR on failure
 */
uint8_t rtc_disable_24h()
{
	return rtc_set_bitmask(RTC_RTCHOUR, (1 << RTC_12_24));
}

/**
 * rtc_enable_battery - enables the battery fallback in rtc chip
 *
 *		Return: TWI_OK on success and TWI_ERR on failure
 */
uint8_t rtc_enable_battery()
{
	return rtc_set_bitmask(RTC_RTCWKDAY, (1 << RTC_VBATEN));
}

/**
 * rtc_disable_battery - disable battery fallback in rtc chip
 *
 *		Return: TWI_OK on success and TWI_ERR on failure
 */
uint8_t rtc_disable_battery()
{
	return rtc_clear_bitmask(RTC_RTCWKDAY, (1 << RTC_VBATEN));
}

/**
 * rtc_start_oscillator() - start the rtc clock
 *
 *		Return: TWI_OK on success, TWI_ERR on error
 */
uint8_t rtc_start_oscillator()
{
	return rtc_set_bitmask(RTC_RTCSEC, (1 << RTC_ST));
}

/**
 * rtc_stop_oscillator() - stop the rtc clock
 *
 *		Return: TWI_OK on success, TWI_ERR on error
 */
uint8_t rtc_stop_oscillator()
{
	return rtc_clear_bitmask(RTC_RTCSEC, (1 << RTC_ST));
}

/**
 * rtc_set_alarm0_interrupt_flag - set the interrupt flag for alarm0
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_set_alarm0_interrupt_flag(void)
{
	return rtc_set_bitmask(RTC_ALM0WKDAY, (1 << RTC_ALM0IF));
}

/**
 * rtc_clear_alarm0_interrupt_flag - clear the interrupt flag for alarm0
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_clear_alarm0_interrupt_flag(void)
{
	return rtc_clear_bitmask(RTC_ALM0WKDAY, (1 << RTC_ALM0IF));
}

/**
 * rtc_get_alarm0_interrupt_flag - read the interrupt flag of alarm0
 * @flag: pointer to uint8_t variable where 1 or 0 as state of interrupt flag
 *		  gets stored
 *
 *		  Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_get_alarm0_interrupt_flag(uint8_t *flag)
{
	uint8_t value, ret;
	ret = rtc_read_register(RTC_ALM0WKDAY, &value);
	if (ret != TWI_OK)	{
		return ret;
	}
	*flag = (value & (1 << RTC_ALM0IF)) >> RTC_ALM0IF;
	return ret;
}

/**
 * rtc_set_alarm1_interrupt_flag - set the interrupt flag for alarm1
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_set_alarm1_interrupt_flag(void)
{
	return rtc_set_bitmask(RTC_ALM1WKDAY, (1 << RTC_ALM1IF));
}

/**
 * rtc_clear_alarm1_interrupt_flag - clear the interrupt flag for alarm1
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_clear_alarm1_interrupt_flag(void)
{
	return rtc_clear_bitmask(RTC_ALM1WKDAY, (1 << RTC_ALM1IF));
}

/**
 * rtc_get_alarm1_interrupt_flag - read the interrupt flag of alarm1
 * @flag: pointer to uint8_t variable where 1 or 0 as state of interrupt flag
 *		  gets stored
 *
 *		  Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_get_alarm1_interrupt_flag(uint8_t *flag)
{
	uint8_t value, ret;
	ret = rtc_read_register(RTC_ALM1WKDAY, &value);
	if (ret != TWI_OK)	{
		return ret;
	}
	*flag = (value & (1 << RTC_ALM1IF)) >> RTC_ALM1IF;
	return ret;
}

/**
 * rtc_set_alarm_polarity - set the polarity of the interrupt pin from rtc
 * @polarity: use either RTC_POLARITY_ACTIVE_LOW (interrupt line low when
 *			  interrupt occurs) or
 *			  use RTC_POLARITY_ACTIVE_HIGH (interrupt line high when
 *			  interrupt occurs)
 *
 *		This function changes the behavior for both alarms because there
 *		is only one setting on the rtc chip
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_set_alarm_polarity(uint8_t polarity)
{
	if (polarity == RTC_POLARITY_ACTIVE_LOW)	{
		return rtc_clear_bitmask(RTC_ALM0WKDAY, (1 << RTC_ALMPOL));
	} else {
		return rtc_set_bitmask(RTC_ALM0WKDAY, (1 << RTC_ALMPOL));
	}
}

/**
 * rtc_set_alarm0_mask - set which time values have to be equal for interrupt
 * @alarm_mask:  give value like RTC_ALARM_MASK_ALL (everything must be equal)
 *				 or RTC_ALARM_MASK_SECONDS (only seconds have to be equal) etc.
 *
 *		Return: TWI_OK on success and TWI_ERR on failure
 */
uint8_t rtc_set_alarm0_mask(uint8_t alarm_mask)
{
	uint8_t ret;
	ret = rtc_set_bitmask(RTC_ALM0WKDAY, (alarm_mask & 0b111) << RTC_ALM0MSK0);
	if (ret != TWI_OK)	{
		return ret;
	}
	return rtc_clear_bitmask(RTC_ALM0WKDAY, ((~alarm_mask & 0b111) << RTC_ALM0MSK0));
}

/**
 * rtc_get_alarm0_mask - get information which time values have to be equal for interrupt!
 * @alarm_mask: pointer to variable where the mask (e.g. RTC_ALARM_MASK_ALL) will
 *				be stored
 *
 *		Return: TWI_OK on success and TWI_ERR on failure
 */
uint8_t rtc_get_alarm0_mask(uint8_t *alarm_mask)
{
	uint8_t ret, byte;
	ret = rtc_read_register(RTC_ALM0WKDAY, &byte);
	byte = byte & 0b01110000;
	*alarm_mask = byte >> 4;
	return ret;
}

/**
 * rtc_set_alarm0_mask - set which time values have to be equal for interrupt
 * @alarm_mask:  give value like RTC_ALARM_MASK_ALL (everything must be equal)
 *				 or RTC_ALARM_MASK_SECONDS (only seconds have to be equal) etc.
 *
 *		Return: TWI_OK on success and TWI_ERR on failure
 */
uint8_t rtc_set_alarm1_mask(uint8_t alarm_mask)
{
	uint8_t ret;
	ret = rtc_set_bitmask(RTC_ALM1WKDAY, (alarm_mask & 0b111) << RTC_ALM1MSK0);
	if (ret != TWI_OK)	{
		return ret;
	}
	return rtc_clear_bitmask(RTC_ALM1WKDAY, ((~alarm_mask & 0b111) << RTC_ALM1MSK0));
}

/**
 * rtc_get_alarm1_mask - get information which time values have to be equal for interrupt!
 * @alarm_mask: pointer to variable where the mask (e.g. RTC_ALARM_MASK_ALL) will
 *				be stored
 *
 *		Return: TWI_OK on success and TWI_ERR on failure
 */
uint8_t rtc_get_alarm1_mask(uint8_t *alarm_mask)
{
	uint8_t ret, byte;
	ret = rtc_read_register(RTC_ALM1WKDAY, &byte);
	byte = byte & 0b01110000;
	*alarm_mask = byte >> 4;
	return ret;
}

/**
 * rtc_enable_alarm0 - enables alarm0
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_enable_alarm0()
{
	return rtc_set_bitmask(RTC_CONTROL, (1 << RTC_ALM0EN));
}

/**
 * rtc_disable_alarm0 - disable alarm0
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_disable_alarm0()
{
	return rtc_clear_bitmask(RTC_CONTROL, (1 << RTC_ALM0EN));
}

/**
 * rtc_enable_alarm1 - enable alarm1
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_enable_alarm1()
{
	return rtc_set_bitmask(RTC_CONTROL, (1 << RTC_ALM1EN));
}

/**
 * rtc_disable_alarm1 - disable alarm1
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_disable_alarm1()
{
	return rtc_clear_bitmask(RTC_CONTROL, (1 << RTC_ALM1EN));
}


/**
 * rtc_set_time - set the time at real time clock
 * @type: time type to be set (like RTC_SECOND, RTC_HOUR, ...)
 * @val: the value to set
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_set_time(uint8_t type, uint8_t val)
{
	return rtc_set_time_internal(type, 0, val);
}

/**
 * rtc_get_time - get time from real time clock
 * @type: time type to get (like RTC_SECOND, RTC_HOUR, ...)
 * @val: pointer to uint8_t variable where the read value will be stored
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_get_time(uint8_t type, uint8_t *val)
{
	return rtc_get_time_internal(type, 0, val);
}

/**
 * rtc_set_alarm0_time - set time for alarm0
 * @type: time type to be set (RTC_SECOND, RTC_HOUR, ...)
 * @val: the value to set
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_set_alarm0_time(uint8_t type, uint8_t val)
{
	/* do not set YEAR, alarm has no year! */
	return rtc_set_time_internal(type, RTC_ALM0SEC, val);
}

/**
 * rtc_set_alarm1_time - set time for alarm1
 * @type: time type to be set (RTC_SECOND, RTC_HOUR, ...)
 * @val: the value to set
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_set_alarm1_time(uint8_t type, uint8_t val)
{
	/* do not set YEAR, alarm has no year! */
	return rtc_set_time_internal(type, RTC_ALM1SEC, val);
}

/**
 * rtc_get_alarm0_time - get time of alarm0
 * @type: time type to get (like RTC_SECOND, RTC_HOUR, ...)
 * @val: pointer to uint8_t variable where value will be stored
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_get_alarm0_time(uint8_t type, uint8_t *val)
{
	/* do not read YEAR, alarm has no year! */
	return rtc_get_time_internal(type, RTC_ALM0SEC, val);
}

/**
 * rtc_get_alarm1_time - get time of alarm1
 * @type: time type to get (like RTC_SECOND, RTC_HOUR, ...)
 * @val: pointer to uint8_t variable where value will be stored
 *
 *		Return: TWI_OK on success, TWI_ERR on failure
 */
uint8_t rtc_get_alarm1_time(uint8_t type, uint8_t *val)
{
	/* do not read YEAR, alarm has no year! */
	return rtc_get_time_internal(type, RTC_ALM1SEC, val);
}

/**
 * rtc_get_max - return the maximum value that can be set to a time/date type
 * @type:	time/date type like RTC_SECOND, RTC_MINUTE
 *
 *		Return: the maximum value that can be assigned as given time/date/
 *		type
 */
uint8_t rtc_get_max(uint8_t type)	{
	switch (type)	{
		case RTC_SECOND:
			return 59;
		case RTC_MINUTE:
			return 59;
		case RTC_HOUR:
			return 23;
		case RTC_WEEKDAY:
			return 7;
		case RTC_DATE:
			return 31;
		case RTC_MONTH:
			return 12;
		case RTC_YEAR:
			return 99;
	}
	return 0;
}

/**
 * rtc_enable_avr_interrupt - enable the INT-interrupt from rtc at avr
 */
void rtc_enable_avr_interrupt()
{
	if (interrupt_was_enabled == TRUE)	{
		EIMSK |= (1 << INT1);
	}
}

/**
 * rtc_disable_avr_interrupt - disable the INT-interrupt from rtc at avr
 */
void rtc_disable_avr_interrupt()
{
	EIMSK &= ~(1 << INT1);
}

/**
 * ISR for interrupt on pin INT1
 *
 * is executed when rtc interrupt occurs
 */
ISR(INT1_vect)
{
#ifdef ISR_LED
	LED_ON();
#endif

	uint8_t flag, ret;
	ret = rtc_get_alarm0_interrupt_flag(&flag);
	if (ret == TWI_OK && flag > 0)	{
		uart_send_text_sram("startup interrupt\n");
		startup_interrupt();
	}
	ret = rtc_get_alarm1_interrupt_flag(&flag);
	if (ret == TWI_OK && flag > 0)	{
		uart_send_text_sram("morse interrup\n");
		morse_interrupt();
	}
	rtc_clear_alarm0_interrupt_flag();
	rtc_clear_alarm1_interrupt_flag();
#ifdef ISR_LED
	LED_OFF();
#endif
}
