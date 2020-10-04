/*
 *  main.c - the main programme, accessing the other libs
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
#include <util/delay.h>
#include <util/twi.h>
#include <stdio.h>

#include "pins.h"
#include "dds.h"
#include "dds_modulation.h"
#include "rtc.h"
#include "uart.h"
#include "utils.h"
#include "main.h"
#include "commands.h"
#include "morse.h"
#include "startup.h"
#include "rfid.h"
#include "ext_eeprom.h"
#include "user.h"

#define CARRIER_OFF		0
#define CARRIER_2M		1
#define CARRIER_80M		2

uint8_t adc_2m_values[7] = {12, 13, 15, 20, 30, 50, 100}; /* swr values multiplied by ten as limits for leds */
uint8_t adc_80m_values[7] = {12, 13, 15, 20, 30, 50, 100}; /* swr values multiplied by ten as limits for leds */

uint8_t should_reload = FALSE;

static uint8_t is_started = FALSE;

volatile uint8_t blinking = FALSE;

static volatile uint8_t continuous_carrier = CARRIER_OFF;

/*
 * internal functions
 */

/**
 * led_bar_set - set led bar to number of leds
 * @val:	number of leds that should light
 *
 *		DO NOT CALL THIS FUNCTION FROM AN ISR
 *		BECAUSE CONTROLLER CAN HANG WHEN SPI HAS
 *		TO BE FREED IN MAIN-ROUTINE!
 *
 *		all routines in main-loop must free spi afterwards
 *		otherwise controller can hang.
 */
static void led_bar_set(uint8_t val)
{
#ifdef NEW_PROTOTYPE
	while (SPI_is_free() == FALSE);
	uint8_t i, temp = (1 << (8 - val));
	LED_BAR_PORT &= ~(1 << LED_BAR_CE);
	SPDR = temp;
	while ((SPSR & (1 << SPIF)) == 0);
	LED_BAR_PORT |= (1 << LED_BAR_CE);
#endif
}

/**
 * reload_int - reload all modules to apply new eeprom settings
 */
static void reload_int(void)
{
	continuous_carrier = CARRIER_OFF; /* after reload continuous carrier is off */

	RFID_PORT |= (1 << RFID_CS); /* otherwise spi is not seen as free by dds spi access */

	LED_DDR |= (1 << LED);
	LED_PORT &= ~(1 << LED);

	twi_init();

	rtc_init();

	dds_init();

	dds_modulation_init();

	morse_init(); /* needs dds */
	startup_init(); /* startup needs rtc, morese and dds*/

	sei();
}

/**
 * adc_init - initialise adc
 */
static void adc_init(void)
{
	ADMUX |= (1 << ADLAR);
	ADCSRA |= (1 << ADEN) | (1 << ADATE) | (1 << ADPS2) | (1 << ADPS1);
}

/**
 * enable_adc_int - internal function for enabling adc
 */
static void enable_adc_int(void)
{
	ADCSRA |= (1 << ADEN) | (1 << ADSC);
}

/**
 * enable_adc- enable adc and select channel for 2m signal
 * continuous_carrier: either CARRIER_2M or CARRIER_80M
 * num: either 0 or 1 for first or second adc of directional coupler
 */
static void enable_adc(uint8_t continuous_carrier, uint8_t num)
{
	ADMUX &= ~((1 << MUX4) | (1 << MUX3) | (1 << MUX2) | (1 << MUX1) | (1 << MUX0));
	if (continuous_carrier == CARRIER_2M)	{
		if (num == 0)	{
			/* activate adc 3 (2m forward) */
			ADMUX |= (1 << MUX1) | (1 << MUX0);
		} else {
			/* activate adc 2 (2m reflection) */
			ADMUX |= (1 << MUX1);
		}
	} else if (continuous_carrier == CARRIER_80M)	{
		if (num == 0)	{
			/* activate adc 5 (80m forward) */
			ADMUX |= (1 << MUX2) | (1 << MUX0);
		} else {
			/* activate adc 4 (80m reflection) */
			ADMUX |= (1 << MUX2);
		}
	}
	enable_adc_int();
}

/**
 * disable_adc - disable adc
 */
static void disable_adc(void)
{
	ADCSRA &= ~(1 << ADEN);
}

/**
 * read_adc - reads new value of adc
 * @data:	pointer to variable where read adc data will be stored
 *
 *		Return: FALSE if there is no new adc value, TRUE otherwise
 */
static uint8_t read_adc(uint8_t *data)
{
	if ((ADCSRA & (1 << ADIF)) != 0)	{
		*data = ADCH;
		return TRUE;
	}
	return FALSE;
}

/*
 * public functions
 */

/**
 * main_set_blinking - set if status led should blink
 * mode:	TRUE or FALSE
 */
void main_set_blinking(uint8_t mode)
{
	blinking = mode;
}

/**
 * main_reload - tells the main module that all modules should reload their settings
 */
void main_reload(void)
{
	should_reload = TRUE;
}

/**
 * main - init function of software
 */
int main(void)
{
	main_init();

	uart_init();
	commands_print_welcome_message();

	twi_init();

	rtc_init();

	rtc_start_oscillator();

	dds_init();
	dds_powerdown();
	while(1);
	//morse_init(); /* needs dds */
	startup_init(); /* startup needs rtc, morese and dds*/
	user_init();
	rfid_init();

	commands_print_prompt();

	sei();

	led_bar_set(0);

	while (1)	{
		commands_execute();

		if (should_reload == TRUE)	{
			reload_int();
			should_reload = FALSE;
		}

		rfid_loop();

#ifdef NEW_PROTOTYPE
		static uint8_t button_2m_off = FALSE;
		static uint8_t button_80m_off = FALSE;
		if ((BUTTON_PIN & (1 << BUTTON_80M)) == 0 && continuous_carrier != CARRIER_80M && button_80m_off == TRUE)	{
			continuous_carrier = CARRIER_80M;
			button_80m_off = FALSE;
			dds_enable_continuous_carrier_80m();
			enable_adc(CARRIER_80M, 0);
		}
		if ((BUTTON_PIN & (1 << BUTTON_2M)) == 0 && continuous_carrier != CARRIER_2M && button_2m_off == TRUE)	{
			continuous_carrier = CARRIER_2M;
			button_2m_off = FALSE;
			dds_enable_continuous_carrier_2m();
			enable_adc(CARRIER_2M, 0);
		}
		if ((BUTTON_PIN & (1 << BUTTON_2M)) == 0 && continuous_carrier == CARRIER_2M && button_2m_off == TRUE)	{
			continuous_carrier = CARRIER_OFF;
			button_2m_off = FALSE;
			dds_disable_continuous_carrier();
			disable_adc();
			led_bar_set(0);
		}
		if ((BUTTON_PIN & (1 << BUTTON_80M)) == 0 && continuous_carrier == CARRIER_80M && button_80m_off == TRUE)	{
			continuous_carrier = CARRIER_OFF;
			button_80m_off = FALSE;
			dds_disable_continuous_carrier();
			disable_adc();
			led_bar_set(0);
		}
		if ((BUTTON_PIN & (1 << BUTTON_2M)) != 0)	{
			button_2m_off = TRUE;
		}
		if ((BUTTON_PIN & (1 << BUTTON_80M)) != 0)	{
			button_80m_off = TRUE;
		}

		if (continuous_carrier == CARRIER_2M || continuous_carrier == CARRIER_80M)	{
			static uint8_t num = 0;
			static uint8_t data[2];
			if (read_adc(&data[num]) == TRUE)	{
				num++;
				if (num > 1)	{
					num = 0;
					float value;
					if (data[1] - data[0] == 0)	{
						value = 255;
					} else {
						value = ((float)(data[1] + data[0])) / (float)(data[0] - data[1]);
					}
					uint8_t i;
					for (i = 0; i < 7; i++)	{
						if (continuous_carrier == CARRIER_80M)	{
							if ((value * 10) < adc_2m_values[i])	{
								break;
							}
						} else if (continuous_carrier == CARRIER_2M)	{
							if ((value * 10) < adc_80m_values[i])	{
								break;
							}
						}
					}
					i = 8 - i;
					led_bar_set(i);
				}
				enable_adc(continuous_carrier, num);
				uint8_t temp;
				read_adc(&temp); /* to clear ready flag for adc, so that really the next enabled adc ist used */
			}
		}
#endif
	}
}

/**
 * main_init - initialise main module
 */
void main_init()
{
	RFID_PORT |= (1 << RFID_CS); /* otherwise spi is not seen as free by dds spi access */

#ifdef NEW_PROTOTYPE
	BUTTON_PORT |= (1 << BUTTON_80M); /* enable pull ups */
	BUTTON_PORT |= (1 << BUTTON_2M);
#endif

	LED_DDR |= (1 << LED);
	LED_PORT &= ~(1 << LED);

#ifdef NEW_PROTOTYPE
	LED_BAR_PORT |= (1 << LED_BAR_CE);
	LED_BAR_DDR |= (1 << LED_BAR_CE);
#endif

	TCCR1B |= (1 << CS12) | (1 << CS10); /* prescaler 1024 of timer1 */
	TCNT1H = (TIMER1_PRELOAD >> 8) & 0xff;
	TCNT1L = TIMER1_PRELOAD & 0xff;
	TIMSK1 |= (1 << TOIE1);

	adc_init();
}

/**
 * main_start_time - called to indicate that event has started
 */
void main_start_time()
{
	is_started = TRUE;
}

/**
 * main_stop_time - called to indicate that event has stopped
 */
void main_stop_time()
{
	is_started = FALSE;
}

ISR(TIMER1_OVF_vect)
{
	TCNT1H = (TIMER1_PRELOAD >> 8) & 0xff;
	TCNT1L = TIMER1_PRELOAD & 0xff;

	user_time_tick();

	static uint8_t count;
	static uint8_t was_blinking;
	count++;

	if (count == 5)	{
#ifdef BLINKING_IS_LED_STATE
		if (blinking != FALSE)	{
#ifdef NEW_PROTOTYPE
			RFID_LED_TOGGLE();
#else
			LED_TOGGLE();
#endif
			was_blinking = TRUE;
		} else {
			if (was_blinking == TRUE)	{
				was_blinking = FALSE;
#ifdef NEW_PROTOTYPE
				RFID_LED_OFF();
#else
				LED_OFF();
#endif
			}
		}
		count = 0;
#endif
	}
}
