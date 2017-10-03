/*
 *  rtc.h - header file declaring functions for accessing the
 *          real time clock chip MCP79410
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

#ifndef RTC_H
#define RTC_H

#include "twi.h" /* for TWI_OK and TWI_ERR */

#define RTC_POLARITY_ACTIVE_HIGH	0
#define RTC_POLARITY_ACTIVE_LOW		1

#define RTC_ALARM_MASK_ALL			0b111
#define RTC_ALARM_MASK_SECONDS		0b000
#define RTC_ALARM_MASK_MINUTES		0b001
#define RTC_ALARM_MASK_HOURS		0b010
#define RTC_ALARM_MASK_WEEKDAY		0b011
#define RTC_ALARM_MASK_DATE			0b100

/* internal representation for date/time types */
#define RTC_SECOND		0
#define RTC_MINUTE		1
#define RTC_HOUR		2
#define RTC_DATE		3
#define RTC_MONTH		4
#define RTC_YEAR		5
#define RTC_WEEKDAY		6
#define RTC_TIME_MAX	6

void rtc_init(void);
void rtc_show_configuration(void);
void rtc_load_configuration(void);

uint8_t rtc_set_time(uint8_t type, uint8_t val);
uint8_t rtc_get_time(uint8_t type, uint8_t *val);

uint8_t rtc_set_alarm0_time(uint8_t type, uint8_t val);
uint8_t rtc_get_alarm0_time(uint8_t type, uint8_t *val);

uint8_t rtc_set_alarm1_time(uint8_t type, uint8_t val);
uint8_t rtc_get_alarm1_time(uint8_t type, uint8_t *val);

uint8_t rtc_start_oscillator(void);
uint8_t rtc_stop_oscillator(void);

uint8_t rtc_enable_24h(void);
uint8_t rtc_disable_24h(void);

uint8_t rtc_enable_battery(void);
uint8_t rtc_disable_battery(void);

uint8_t rtc_get_max(uint8_t type);

uint8_t rtc_set_alarm0_interrupt_flag(void);
uint8_t rtc_clear_alarm0_interrupt_flag(void);
uint8_t rtc_get_alarm0_interrupt_flag(uint8_t *flag);

uint8_t rtc_set_alarm1_interrupt_flag(void);
uint8_t rtc_clear_alarm1_interrupt_flag(void);
uint8_t rtc_get_alarm1_interrupt_flag(uint8_t *flag);

uint8_t rtc_set_alarm_polarity(uint8_t polarity);

uint8_t rtc_set_alarm0_mask(uint8_t alarm_mask);
uint8_t rtc_get_alarm0_mask(uint8_t *alarm_mask);

uint8_t rtc_set_alarm1_mask(uint8_t alarm_mask);
uint8_t rtc_get_alarm1_mask(uint8_t *alarm_mask);

uint8_t rtc_enable_alarm0(void);
uint8_t rtc_disable_alarm0(void);

uint8_t rtc_disable_alarm1(void);
uint8_t rtc_enable_alarm1(void);

void rtc_enable_avr_interrupt(void);
void rtc_disable_avr_interrupt(void);

#endif
