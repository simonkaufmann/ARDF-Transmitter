/*
 *	startup.h - definitions of functions of startup time
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

#ifndef STARTUP_H
#define STARTUP_H

#include "rtc.h"

/*
 * do not change (is needed in commands.c for executing the time startup)
 * must equal with defines in rtc.h because it is needed in startup.c
 */
#define STARTUP_SECOND		RTC_SECOND
#define STARTUP_MINUTE		RTC_MINUTE
#define STARTUP_HOUR		RTC_HOUR
#define STARTUP_DATE		RTC_DATE
#define STARTUP_MONTH		RTC_MONTH
#define STARTUP_YEAR		RTC_YEAR
#define STARTUP_WEEKDAY		RTC_WEEKDAY
#define STARTUP_TIME_MAX	RTC_TIME_MAX

void startup_init(void);
void startup_show_configuration(void);
void startup_load_configuration(void);

uint8_t startup_set_start_time(uint8_t type, uint8_t time);
uint8_t startup_set_stop_time(uint8_t type, uint8_t time);
uint8_t startup_get_start_time(uint8_t type);
uint8_t startup_get_stop_time(uint8_t type);

void startup_interrupt(void);


#endif
