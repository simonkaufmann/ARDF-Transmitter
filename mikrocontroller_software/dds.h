/*
 *  dds.h - definition of functions and values for using the AD9859 DDS chip
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

#ifndef DDS_H
#define DDS_H


/*
 * definitions
 */
#define DDS_AMPLITUDE_MAX		0x3FFF /* = 16384 = 2^14 */
#define DDS_AMPLITUDE_MAX_80M				80 /* percent */
#define DDS_AMPLITUDE_MAX_2M				100 /* percent */
#define DDS_AMPLITUDE_MAX_2M_MODULATION		100 /* percent */

#define DDS_FREQ_MULTIPLIER		16 /* between 1 and 16 for a 25MHz crystal */
#define DDS_SYSCLK				(DDS_FREQ_MULTIPLIER * 25000000UL)

#define DDS_FREQ_2M_80M_LIMIT	4000000UL

/*
 * macros
 */
#define FREQ_TO_FTW(freq) ((uint32_t)(((double)freq/(dds_get_crystal_frequency()\
					      *DDS_FREQ_MULTIPLIER))*(0xFFFFFFFFUL)))	/* deprecated */
						  /* 0xFFFFFFFF is 2 to the power of 32 */
#define FTW_TO_FREQ(ftw)  ((uint32_t)(((double)ftw/(0xFFFFFFFFUL)*\
						  (dds_get_crystal_frequency()*DDS_FREQ_MULTIPLIER))))

#define DDS_FREQUENCY_DEFAULT		3500000
#define DDS_FREQUENCY_MAX			150000000
#define DDS_FREQUENCY_MIN			2000000

#define DDS_CRYSTAL_FREQUENCY_DEFAULT	20000000
#define DDS_CRYSTAL_FREQUENCY_MIN		10000000
#define DDS_CRYSTAL_FREQUENCY_MAX		30000000

#define DDS_AMPLITUDE_EEPROM_MAX		100
#define DDS_AMPLITUDE_EEPROM_MIN		0
#define DDS_AMPLITUDE_EEPROM_DEFAULT	100

#define DDS_DEFAULT_FREQUENCY_80M	3500000
#define DDS_DEFAULT_FREQUENCY_2M	144700000

/*
 * auto OSK operation:
 *	-> a 10-bit value is counted up or down (auto scale factor)
 *	   this auto scale factor scales the amplitude scale factor
 *	   the auto scale factor is incremented if OSK-pin is high
 *	   and is decremented if OSK-pin is low
 *	   the time beetween an increment or decrement is ARR * system_clock_cycle
 *
 *	   SYSCLK / (1024 * ARR) = 1 / time for amplitude 0 to amplitude in
 *	   amplitude scale factor register
 *	   Note: for any reason the value 1024 is not valid, tests have shown that
 *	   replacing 1024 by 36536 leads to the correct value!
 */


/*
 * function declarations
 */

void dds_init(void);
void dds_show_configuration(void);
void dds_load_configuration(void);

uint8_t dds_set_frequency(uint32_t frequency);
uint32_t dds_get_frequency(void);

uint8_t dds_set_crystal_frequency(uint32_t frequency);
uint32_t dds_get_crystal_frequency(void);

void dds_set_modulation(uint8_t mod);
uint8_t dds_get_modulation(void);

uint8_t dds_set_amplitude_percentage(uint8_t amp);
uint8_t dds_get_amplitude_percentage(void);

uint8_t dds_on(void);
uint8_t dds_off(void);

void dds_execute_command_buffer(void);

void dds_disable_continuous_carrier(void);
void dds_enable_continuous_carrier_2m(void);
void dds_enable_continuous_carrier_80m(void);

void dds_powerdown(void);
void dds_powerup(void);

#endif
