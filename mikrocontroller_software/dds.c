/*
 *  dds.c - different functions for using the AD9859 DDS chip
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
#include <avr/eeprom.h>
#include <util/delay.h>

#include "dds.h"
#include "main.h"
#include "pins.h"
#include "uart.h"
#include "SPI.h"
#include "morse.h"

/*
 * dds register addresses
 */
#define DDS_CFR1			0x00 /* configuration register 1 */
#define DDS_CFR2			0x01 /* configuration register 2 */
#define DDS_ASF				0x02 /* amplitude scale factor register */
#define DDS_ARR				0x03 /* amplitude ramp rate register */
#define DDS_FTW0			0x04 /* frequency tuning word */
#define DDS_POW0			0x05 /* phase offset word */


 
/*
 * define bits in DDS registers
 */
/* DDS_CFR1 byte 3 */
#define DDS_OSK_ENABLE		1
#define DDS_AUTO_OSK_KEYING	0
/* DDS_CFR1 byte 1 */
#define DDS_SDIO_INPUT_ONLY		1
/* DDS_CFR0 byte 0 */
#define DDS_DIGITAL_POWER_DOWN		7
#define DDS_DAC_POWER_DOWN			5
#define DDS_CLOCK_INPUT_POWER_DOWN	4
/* DDS_CFR2 byte 1 */
#define DDS_CRYSTAL_OUT_ACTIVE	1


/* flags for dds_spi.spi_flag struct */
#define DDS_SPI_ACTIVE		1
#define DDS_SPI_READ		2

#define DDS_ON				1
#define DDS_OFF				0

#define SIN_VALUES			10

#define MODULATION_FREQUENCY	600 /* deprecated */
#define TIMER0_OVERFLOW_NUMBER	6
#define TIMER0_OVERFLOW_SET_SPI_OFF 1 /* after so many overflows set spi to off so that rfid library will not send any more */
#define TIMER0_PRELOAD			(256 - F_CPU / 8 / (MODULATION_FREQUENCY * SIN_VALUES) / TIMER0_OVERFLOW_NUMBER + 6) /* + 6 because of deviation because of cycles lost before setting the preload value -> depends on TIMER0_OVERFLOW_NUMBER and SIN_VALUES */

/* variable declarations */
const uint16_t sin_table[SIN_VALUES] = {8703, 13217, 16007, 16007, 13217, 8703, 4189, 1399, 1399, 4189};
uint8_t sin_table_byte0[SIN_VALUES] = {0}; /* to store values temporary so that amplitude calculate does not have to be done in ISR */
uint8_t sin_table_byte1[SIN_VALUES] = {0};

uint32_t EEMEM frequency;
uint32_t EEMEM calibrated_crystal;
uint8_t EEMEM modulation;
uint8_t EEMEM amplitude;

volatile uint8_t on = DDS_OFF;
volatile uint8_t current_modulation = FALSE;

#define EXECUTE_NOTHING		0
#define	EXECUTE_DDS_ON		1
#define EXECUTE_DDS_OFF		2
uint8_t execute_command_buffer;

#define CONTINUOUS_CARRIER_OFF	0
#define CONTINUOUS_CARRIER_2M	1
#define CONTINUOUS_CARRIER_80M	2
static uint8_t continuous_carrier = CONTINUOUS_CARRIER_OFF;

/*
 * internal functions
 */

/**
 * wait_spi - wait until SPI transmit is ready
 */
static inline void wait_spi(void)
{
	while ((SPSR & (1 << SPIF)) == 0);
}

static void dds_disable_power_amplifier()
{
#ifdef NEW_PROTOTYPE
	/* turn off the amplifier */
	PA_PORT &= ~(1 << PA_80M);
	PA_PORT &= ~(1 << PA_2M);
#endif
}

/**
 * dds_enable_power_amplifier - turn on power amplifier 80m/2m
 *
 *		Either 80m or 2m power amplifier is activated depending
 *		on configuration setting	
 */
static void dds_enable_power_amplifier()
{
#ifdef NEW_PROTOTYPE
	/* turn on the amplifier */
	if (dds_get_frequency() > DDS_FREQ_2M_80M_LIMIT)	{
		PA_PORT &= ~(1 << PA_80M);
		PA_PORT |= (1 << PA_2M);
	} else {
		PA_PORT &= ~(1 << PA_2M);
		PA_PORT |= (1 << PA_80M);
	}
#endif
}

/**
 * dds_io_update - perform a input/output buffer update in dds chip
 *
 *		An io update has to be performed after changing the dds registers
 *		to affect current output state of the dds
 */
static void dds_io_update(void)
{
	DDS_PORT |= (1 << DDS_IO_UPDATE);
	//_delay_us(75); /* this time is needed for updating the buffer */
	DDS_PORT &= ~(1 << DDS_IO_UPDATE);
}

/**
 * dds_write_register - write dds register
 * @register_address:	address of the dds register to write into
 * @data:				pointer to data to send in SRAM
 * @length:				length of the register (and therefore the array that
 *						data points to) in bytes
 *
 *		Return: TRUE if spi was free and dds could be written, returns
 *		FALSE if spi was not free (rfid was sending at the moment)
 */
static uint8_t dds_write_register(int8_t register_address, const int8_t *data, int8_t length)
{
	if (SPI_is_free() == FALSE)	{
		return FALSE;
	}

	DDS_PORT &= ~(1 << DDS_CS);
	SPDR = register_address;
	wait_spi();
	uint8_t i;
	for (i = 0; i < length; i++)	{
		SPDR = data[i];
		wait_spi();
	}
	DDS_PORT |= (1 << DDS_CS);

#ifdef WITH_IOSYNC
	DDS_PORT &= ~(1 << DDS_IOSYNC);
	DDS_PORT |= (1 << DDS_IOSYNC);
	DDS_PORT &= ~(1 << DDS_IOSYNC);
#endif

	return TRUE;
}

/**
 * dds_read_register - read a dds register
 * @register_address:	address of the dds register to read
 * @data:				shall point to an array where the read data will
 *						get stored
 * @length:				length of the register in bytes, buffer pointed to
 *						by data must have at least the size of length-bytes
 *
 *		Return: TRUE if spi was free and data could be read, returns
 *		FALSE if spi was not free (rfid was sending at the moment)
 */
static uint8_t dds_read_register(int8_t register_address, int8_t *data, int8_t length)
{
	if (SPI_is_free() == FALSE)	{
		return FALSE;
	}

	DDS_PORT &= ~(1 << DDS_CS);
	SPDR = register_address | (1 << 7);
	wait_spi();
	uint8_t i;
	for (i = 0; i < length; i++)	{
		SPDR = 0x00;
		wait_spi();
		data[i] = SPDR;
	}
	DDS_PORT |= (1 << DDS_CS);

#ifdef WITH_IOSYNC
	DDS_PORT &= ~(1 << DDS_IOSYNC);
	DDS_PORT |= (1 << DDS_IOSYNC);
	DDS_PORT &= ~(1 << DDS_IOSYNC);
#endif

	return TRUE;
}

/**
 * dds_set_amplitude - write amplitude value to eeprom
 * @amp: 14 bit value for amplitude (between 0 and DDS_AMPLITUDE_MAX)
 *
 *		This function converts the 14 bit value into a percentage as
 *		the amplitude is saved as percentag in eeprom
 */
static void dds_set_amplitude(uint16_t amplitude)
{
	uint32_t dds_amplitude_max;
	if (dds_get_frequency() < DDS_FREQ_MULTIPLIER)	{
		dds_amplitude_max = DDS_AMPLITUDE_MAX_80M / 100 * DDS_AMPLITUDE_MAX;
	} else {
		if (dds_get_modulation() == TRUE)	{
			dds_amplitude_max = DDS_AMPLITUDE_MAX_2M_MODULATION / 100 * DDS_AMPLITUDE_MAX;
		} else {
			dds_amplitude_max = DDS_AMPLITUDE_MAX_2M / 100 * DDS_AMPLITUDE_MAX;
		}
	}
	dds_set_amplitude_percentage((uint32_t)amplitude * 100 / dds_amplitude_max);
}

/**
 * dds_get_amplitude - read the amplitude value saved in eeprom
 *
 *		Return: amplitude value from eeprom. Value is between 0 and
 *		DDS_AMPLITUDE_MAX.
 */
static uint16_t dds_get_amplitude(void)
{
	uint32_t dds_amplitude_max;
	if (continuous_carrier == CONTINUOUS_CARRIER_OFF)	{
		if (dds_get_frequency() < DDS_FREQ_MULTIPLIER)	{
			dds_amplitude_max = ((uint32_t)DDS_AMPLITUDE_MAX_80M * DDS_AMPLITUDE_MAX) / 100;
		} else {
			if (dds_get_modulation() == TRUE)	{
				dds_amplitude_max = ((uint32_t)DDS_AMPLITUDE_MAX_2M_MODULATION * DDS_AMPLITUDE_MAX) / 100;
			} else {
				dds_amplitude_max = ((uint32_t)DDS_AMPLITUDE_MAX_2M * DDS_AMPLITUDE_MAX) / 100;
			}
		}
	} else if (continuous_carrier == CONTINUOUS_CARRIER_2M)	{
		dds_amplitude_max = ((uint32_t)DDS_AMPLITUDE_MAX_2M * DDS_AMPLITUDE_MAX) / 100;
	} else {
		dds_amplitude_max = ((uint32_t)DDS_AMPLITUDE_MAX_80M * DDS_AMPLITUDE_MAX) / 100;
	}
	return (eeprom_read_byte(&amplitude) / 100.0 * dds_amplitude_max);
}

/**
 * register_execute_command_buffer - save one function into the execute buffer
 * @num: number of function to store in buffer, value like EXECUTE_DDS_ON
 *		 EXECUTE_DDS_OFF
 *
 *		This function is at the time only implemented for dds_on and
 *		dds_off because they are needed from morse.c which has to execute
 *		this function while also rfid is using spi.
 *		So this buffer saves the last execute function to execute it as
 *		soon as as spi is free again (buffer is executed by the Arduino.c
 *		file as soon as the rfid chip select is brought to high)
 *
 *		If more functions have to be executed while rfid could also be sending
 *		this buffer has to be improved!
 */
static void register_execute_command_buffer(uint8_t num)
{
	execute_command_buffer = num;
}

/**
 * dds_write_output_ftw - set frequency of dds output
 * @ftw:	frequency tuning word - value of the FTW-register
 *
 *		Note: the ftw can be generated by using the FREQ_TO_FTW(freq)-macro
 *		call the function like this:
 *		dds_write_output_ftw(FREQ_TO_FTW(144.5)); -> set frequency to 144.5 MHz
 */
static void dds_write_output_ftw(uint32_t ftw_val)
{
	uint8_t data[4];
	data[3] = ftw_val & 0xFF;
	ftw_val >>= 8;
	data[2] = ftw_val & 0xFF;
	ftw_val >>= 8;
	data[1] = ftw_val & 0xFF;
	ftw_val >>= 8;
	data[0] = ftw_val & 0xFF;

	dds_write_register(DDS_FTW0, (int8_t *)data, 4);
	dds_io_update();
}

/**
 * dds_get_ftw - read the frequency tuning word from eeprom
 *
 *		Return: the frequency tuning word for dds register
 */
static uint32_t dds_get_ftw(void)
{
	return FREQ_TO_FTW(dds_get_frequency());
}

/**
 * dds_write_amplitude - update dds amplitude depending on state of dds
 * @amp: 14-bit value for amplitude of dds to set
 *
 *		Return: TRUE if dds write was successful and returns FALSE
 *		if writing the dds failed (because spi was not free)
 */
static uint8_t dds_write_amplitude(uint16_t amp)
{
	if (on != DDS_ON)	{
		amp = 0;
	}
	uint8_t data[2];
	data[0] = (amp>> 8) & 0b00111111;
	data[1] = amp& 0xFF;

	uint8_t ret = dds_write_register(DDS_ASF, (int8_t *)data, 2);
	dds_io_update();
	return ret;
}

/**
 * dds_write_amplitude_isr - called by ISR to set amplitude for amplitude modulation
 *
 *		For maximum speed not the dds_write_register function is called
 *		but the spi communication is directly implemented inside the function
 *
 *		Return: TRUE on success, FALSE otherwise
 */
static inline uint8_t dds_write_amplitude_isr(uint8_t byte1, uint8_t byte0)
{
	uint8_t ret = TRUE;
	if (SPI_is_free() == FALSE)	{
		return FALSE;
	}

	DDS_PORT &= ~(1 << DDS_CS);
	SPDR = DDS_ASF;
	wait_spi();
	if (on != DDS_ON)	{
		SPDR = 0x00;
		wait_spi();
		SPDR = 0x00;
		wait_spi();
	} else {
		SPDR = byte0;
		wait_spi();
		SPDR = byte1;
		wait_spi();
	}
	DDS_PORT |= (1 << DDS_CS);

#ifdef WITH_IOSYNC
	DDS_PORT &= ~(1 << DDS_IOSYNC);
	DDS_PORT |= (1 << DDS_IOSYNC);
	DDS_PORT &= ~(1 << DDS_IOSYNC);
#endif

	dds_io_update();

	return ret;
}

/**
 * dds_disable_continuous_carrier_int - disable continuous carrier
 *
 *		The function tells the dds module and the morse module that
 *		continuous carrier mode should stop
 */
static void dds_disable_continuous_carrier_int(void)
{
	morse_disable_continuous_carrier();
	dds_powerdown();
	continuous_carrier = CONTINUOUS_CARRIER_OFF;
}

/*
 * public functions
 */

/**
 * dds_init - initialise dds chip
 *
 *		DDS initialise is also important for SPI and SPI pin initialisation
 */
void dds_init()
{
	/* timer configuration for modulation */
	TCCR0B |= (1 << CS01); /* switch on timer0 with prescaler = 8 */
	TIMSK0 &= ~(1 << TOIE0); /* disable timer interrupt because there must not be sent anything while dds is getting configured! */

	DDS_PORT &= ~((DDS_IOSYNC) | (1 << DDS_IO_UPDATE));
	DDS_PORT |= (1 << DDS_CS) | (1 << DDS_MODULATION_CS);
	DDS_DDR  |= (1 << DDS_CS) | (1 << DDS_IO_UPDATE) | (1 << DDS_MODULATION_CS) |
			(1 << DDS_IOSYNC) | (1 << DDS_MOSI) | (1 << DDS_SCK)| (1 << DDS_RESET);
	
	/* set power amplifier pins to output */
	PA_PORT &= ~((1 << PA_80M) | (1 << PA_2M));
	PA_DDR |= (1 << PA_80M) | (1 << PA_2M);

	/* perform a reset */
	DDS_PORT |= (1 << DDS_RESET);
	_delay_us(10);
	DDS_PORT &= ~(1 << DDS_RESET);

	/* spi enable, master, f_cpu/2 as clk freq, clock low while idle,
	 * clock phase 0 (first clock up means first bit), msb first
	 * maximum clock frequency of DDS is 25Mhz -> f_cpu/2 is fastest of avr,
	 * it is slow enough for dds!
	 */
	SPCR |= (1 << SPE) | (1 << MSTR);
	SPSR |= (1 << SPI2X);

	int8_t data[4];

	/* access configuratin register 1 */
	data[0] = (1 << DDS_OSK_ENABLE); //| (1 << DDS_AUTO_OSK_KEYING);
	data[1] = 0x00;
	data[2] = (1 << DDS_SDIO_INPUT_ONLY);
	data[3] = 0x00;

	dds_write_register(DDS_CFR1, data, 4);
	dds_io_update();

	/* access configuration register 2 */
	data[0] = 0x00;
	data[1] = 0x00 | (1 << DDS_CRYSTAL_OUT_ACTIVE);
	data[2] = DDS_FREQ_MULTIPLIER << 3; /* clock multiplier to 20 */

	dds_write_register(DDS_CFR2, data, 3);
	dds_io_update();

	dds_off();

	dds_load_configuration();
}

/**
 * dds_show_configuration - output the configuration in RAM of dds module
 *
 *		Prints a debugging output with all the configured settings of the
 *		dds module that are saved in eeprom. Only the settings are outputed
 *		that can be configured using the eeprom settings.
 */
void dds_show_configuration(void)
{
	uart_send_text_sram("Frequency: ");
	uart_send_int(dds_get_frequency());
	uart_send_text_sram("Hz");
	UART_NEWLINE();

	uart_send_text_sram("Crystal frequency: ");
	uart_send_int(dds_get_crystal_frequency());
	uart_send_text_sram("Hz");
	UART_NEWLINE();

	uart_send_text_sram("Amplitude: ");
	uart_send_int(dds_get_amplitude_percentage());
	uart_send_text_sram("%");
	UART_NEWLINE();

	if (dds_get_modulation() == TRUE)	{
		uart_send_text_sram("Modulation: on");
		UART_NEWLINE();
	} else {
		uart_send_text_sram("Modulation: off");
		UART_NEWLINE();
	}
}

/**
 * dds_load_configuration - reads configuration of dds module from eeprom
 *
 *		Loads the configuration from eeprom into RAM and executes the
 *		new configuration
 */
void dds_load_configuration(void)
{
	dds_disable_continuous_carrier_int();

	/* access frequency tuning word register */
	dds_write_output_ftw(dds_get_ftw());

	uint8_t i;
	for (i = 0; i < SIN_VALUES; i++)	{
		uint16_t temp = (double)sin_table[i] / DDS_AMPLITUDE_MAX * dds_get_amplitude();
		sin_table_byte0[i] = temp & 0xff;
		sin_table_byte1[i] = (temp >> 8) & 0b00111111;
	}

	/* access configuration amplitude scale register */
	dds_write_amplitude(dds_get_amplitude());

	dds_io_update();
	//current_modulation = dds_get_modulation(); // We do not activate classic modulation here but new modulation instead
	current_modulation = FALSE;
}

/**
 * dds_set_amplitude_percentage - set the percentage of amplitude max value in eeprom
 * @amp: percentage of amplitude
 *
 *		Return: TRUE if set amplitude was in correct range, FALSE if amplitude
 *		could not be set because it is in the wrong range
 */
uint8_t dds_set_amplitude_percentage(uint8_t amp)
{
	if (amp < DDS_AMPLITUDE_EEPROM_MIN || amp > DDS_AMPLITUDE_EEPROM_MAX)	{
		return FALSE;
	}
	eeprom_write_byte(&amplitude, amp);
	return TRUE;
}

/**
 * dds_get_amplitude_percentage - get the percentage of amplitude
 */
uint8_t dds_get_amplitude_percentage(void)
{
	uint8_t amp = eeprom_read_byte(&amplitude);
	if (amp < DDS_AMPLITUDE_EEPROM_MIN || amp > DDS_AMPLITUDE_EEPROM_MAX)	{
		amp = DDS_AMPLITUDE_EEPROM_DEFAULT;
	}
	return amp;
}

/**
 * dds_on - set dds state to on
 *
 *		Return: TRUE if writing the dds was successful, returns FALSE
 *		if writing failed
 */
uint8_t dds_on(void)
{
#ifdef DDS_FUNC_IS_LED_STATE
	LED_ON();
#endif
	uint8_t tmp_on = on;
	on = DDS_ON;
	if (dds_write_amplitude(dds_get_amplitude()) == TRUE)	{
		if (current_modulation == TRUE)	{
			TIMSK0 |= (1 << TOIE0);
		} else {
			TIMSK0 &= ~(1 << TOIE0);
		}
#ifdef DDS_STATE_IS_LED_STATE
		LED_ON();
#endif
		return TRUE;
	} else {
		/* if writing failed -> put this function into the dds_execution buffer
		 * that gets executed as soon as rfid is ready and spi is free
		 */
		on = tmp_on;
		register_execute_command_buffer(EXECUTE_DDS_ON);
		return FALSE;
	}
}

/**
 * dds_off - set dds state to off
 *
 *		Return: TRUE if writing the dds was successful, returns FALSE
 *		if writing failed
 */
uint8_t dds_off(void)
{
#ifdef DDS_FUNC_IS_LED_STATE
	LED_OFF();
#endif
	uint8_t tmp_on = on;
	on = DDS_OFF;
	if (dds_write_amplitude(dds_get_amplitude()) == TRUE)	{
		TIMSK0 &= ~(1 << TOIE0);
		SPI_set_state(SPI_ON);
#ifdef DDS_STATE_IS_LED_STATE
		LED_OFF();
#endif
		return TRUE;
	} else	{
		/* if writing failed -> put this function into the dds_execution buffer
		 * that gets executed as soon as rfid is ready and spi is free
		 */
		on = tmp_on;
		register_execute_command_buffer(EXECUTE_DDS_OFF);
		return FALSE;
	}
}

/**
 * dds_set_frequency - write the frequency of the dds to eeprom
 * @frequency_val: the frequency in Hz
 *
 *		Return: TRUE if frequency is in the correct range, FALSE if frequency
 *		could not be set because it is in the wrong range
 */
uint8_t dds_set_frequency(uint32_t frequency_val)
{
	if (frequency_val > DDS_FREQUENCY_MAX || frequency_val < DDS_FREQUENCY_MIN)	{
		return FALSE;
	}
	eeprom_write_dword(&frequency, frequency_val);
	return TRUE;
}

/**
 * dds_get_frequency - read the frequency value saved in eeprom
 *
 *		Return: frequency of dds saved in eeprom in Hz
 */
uint32_t dds_get_frequency(void)
{
	uint32_t freq = eeprom_read_dword(&frequency);
	if (freq > DDS_FREQUENCY_MAX || freq < DDS_FREQUENCY_MIN)	{
		freq = DDS_FREQUENCY_DEFAULT;
	}
	return freq;
}

/**
 * dds_set_crystal_frequency - save crystal frequency of dds chip (for calibration)
 *							   in eeprom
 * @frequency: the frequency of dds's crystal in Hz
 *
 *		Return: TRUE if set frequency is in the correct range, FALSE if
 *		frequency could not be set because it is in the wrong range
 */
uint8_t dds_set_crystal_frequency(uint32_t frequency)
{
	if (frequency > DDS_CRYSTAL_FREQUENCY_MAX || frequency < DDS_CRYSTAL_FREQUENCY_MIN)	{
		return FALSE;
	}
	eeprom_write_dword(&calibrated_crystal, frequency);
	return TRUE;
}

/**
 * dds_get_crystal_frequency - read crystal frequency of dds chip saved in eeprom
 *
 *		Return: frequency of dds's crystal in Hz
 */
uint32_t dds_get_crystal_frequency(void)
{
	uint32_t freq = eeprom_read_dword(&calibrated_crystal);
	if (freq < DDS_CRYSTAL_FREQUENCY_MIN || freq > DDS_CRYSTAL_FREQUENCY_MAX)	{
		freq = DDS_CRYSTAL_FREQUENCY_DEFAULT;
	}
	return freq;
}

/**
 * dds_set_modulation - set if amplitude modulation should be true or false
 * @mod:	either TRUE or FALSE, represents the new modulation state
 */
void dds_set_modulation(uint8_t mod)
{
	eeprom_write_byte(&modulation, mod);
}

/**
 * dds_get_modulation - returns the current set amplitude modulation state
 *
 *		Return: either TRUE if modulation is on or FALSE if modulation
 *		is off (saved in eeprom)
 */
uint8_t dds_get_modulation(void)
{
	uint8_t mod = eeprom_read_byte(&modulation);
	if (mod != TRUE)	{
		mod = FALSE;
	}
	return mod;
}

/**
 * dds_execute_command_buffer - executes the last function saved in the execute
 *							    command buffer
 */
void dds_execute_command_buffer()
{
	if (execute_command_buffer == EXECUTE_DDS_ON)	{
		if (dds_on() != TRUE)	{
			return;
		}
	} else if (execute_command_buffer == EXECUTE_DDS_OFF)	{
		if (dds_off() != TRUE)	{
			return;
		}
	}
	execute_command_buffer = EXECUTE_NOTHING;
}

/**
 * dds_powerdown - set dds into power down mode
 */
void dds_powerdown(void)
{
	int8_t data[4];

	dds_read_register(DDS_CFR1, data, 4);

	data[3] |= (1 << DDS_DIGITAL_POWER_DOWN) | (1 << DDS_DAC_POWER_DOWN) | (1 << DDS_CLOCK_INPUT_POWER_DOWN);
	dds_write_register(DDS_CFR1, data, 4);
	dds_io_update();

	dds_disable_power_amplifier();
}

/**
 * dds_powerup - set dds into power up mode
 */
void dds_powerup(void)
{
	int8_t data[4];

	dds_read_register(DDS_CFR1, data, 4);

	data[3] &= ~((1 << DDS_DIGITAL_POWER_DOWN) | (1 << DDS_DAC_POWER_DOWN) | (1 << DDS_CLOCK_INPUT_POWER_DOWN));
	dds_write_register(DDS_CFR1, data, 4);
	dds_io_update();

	//_delay_ms(2);

	dds_enable_power_amplifier();
}

/**
 * dds_enable_continuous_carrier_2m - enable continuous carrier in 2m frequency band 
 */
void dds_enable_continuous_carrier_2m(void)
{
	dds_powerup();
	PA_PORT &= ~(1 << PA_80M);
	PA_PORT |= (1 << PA_2M);
	dds_write_output_ftw(FREQ_TO_FTW(DDS_DEFAULT_FREQUENCY_2M));
	continuous_carrier = CONTINUOUS_CARRIER_2M;
	dds_off(); /* dds_off is necessary otherwise controller can hang at next dds_on because timer is deativated without freeing spi */
	current_modulation = FALSE;
	morse_enable_continuous_carrier();
}

/**
 * dds_enable_continuous_carrier_80m - enable continuous carrier in 80m frequency band
 */
void dds_enable_continuous_carrier_80m(void)
{
	dds_powerup();
	PA_PORT &= ~(1 << PA_2M);
	PA_PORT |= (1 << PA_80M);
	dds_write_output_ftw(FREQ_TO_FTW(DDS_DEFAULT_FREQUENCY_80M));
	continuous_carrier = CONTINUOUS_CARRIER_80M;
	dds_off(); /* dds_off is necessary otherwise controller can hang at next dds_on because timer is deativated without freeing spi */
	current_modulation = FALSE;
	morse_enable_continuous_carrier();
}

/**
 * dds_disable_continuous_carrier - disable continuous carrier
 */
void dds_disable_continuous_carrier(void)
{
	dds_load_configuration();
	dds_disable_continuous_carrier_int();
}

/*
 * interrupt service routines
 */

ISR(TIMER0_OVF_vect)
{
	TCNT0 = TIMER0_PRELOAD;
#if defined ISR_LED || defined ISR_LED_MODULATION
	LED_ON();
#endif

	static uint8_t overflow_count = 0;
	overflow_count++;
	if (overflow_count == TIMER0_OVERFLOW_SET_SPI_OFF)	{
		SPI_set_state(SPI_OFF); /* rfid should not send any more */
	} else if (overflow_count >= TIMER0_OVERFLOW_NUMBER) {
		static volatile uint8_t x = 0;
		if (++x == SIN_VALUES)	{
				x = 0;
		}
		if (dds_write_amplitude_isr(sin_table_byte0[x], sin_table_byte1[x]) == TRUE)	{
#ifdef SPI_NOT_FREE_LED
			LED_ON();
#endif
			SPI_set_state(SPI_ON);
#ifdef SPI_NOT_FREE_LED
			LED_OFF()();
#endif
		}
		overflow_count = 0;
	}
#if defined ISR_LED || defined ISR_LED_MODULATION
	LED_OFF();
#endif
}
