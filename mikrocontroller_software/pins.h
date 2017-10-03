/*
 *  pins.h - defines the pin assignment of the avr microcontroller
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

#ifndef PINS_H
#define PINS_H

#define DDS_PORT			PORTB
#define DDS_DDR				DDRB
#define DDS_PWRDWNCTL		PB3
#define DDS_IO_UPDATE		PB2
#define DDS_IOSYNC			PB1
#define DDS_CS				PB4
#define DDS_MOSI			PB5
#define DDS_SCK				PB7
#define DDS_MISO			PB6
#define DDS_RESET			PB0

#ifdef NEW_PROTOTYPE
	#define LED_PORT			PORTC
	#define LED_DDR				DDRC
	#define LED					PC6
#else
	#define LED_PORT			PORTC
	#define LED_DDR				DDRC
	#define LED					PC3
#endif

#ifdef NEW_PROTOTYPE
#define BUTTON_PORT			PORTC
#define BUTTON_PIN			PINC
#define BUTTON_DDR			DDRC
#define BUTTON_2M			PC3 /* button is active low */
#define BUTTON_80M			PC2 /* button is active low */
#endif

#define RTC_MFP				PD3
#define RTC_MFP_PORT		PORTD
#define RTC_MFP_DDR			DDRD
#define RTC_MFP_PIN			PIND

#ifdef NEW_PROTOTYPE
#define RFID_PORT			PORTD
#define RFID_DDR			DDRD
#define RFID_PIN			PIND
#define RFID_CS				PD4
#define RFID_RESET			PD6
#define RFID_LED			PD5
#else
#define RFID_PORT			PORTA
#define RFID_DDR			DDRA
#define RFID_PIN			PINA
#define RFID_CS				PA0
#define RFID_RESET			PA1
/* RFID_IRQ is on INT0 */
#endif


#ifdef NEW_PROTOTYPE
#define PA_PORT				PORTA	/* enable or disable the hf-amplifiers */
#define PA_DDR				DDRA
#define PA_80M				PA1		/* high active */
#define PA_2M				PA0		/* high active */
#endif

#define SPI_PORT			PORTB
#define SPI_DDR				DDRB
#define MOSI				PB5
#define MISO				PB6
#define SCK					PB7

#ifdef NEW_PROTOTYPE
#define LED_BAR_PORT		PORTD
#define LED_BAR_DDR			DDRD
#define LED_BAR_CE			PD7
#endif

#endif
