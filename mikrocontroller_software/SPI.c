/*
 *  SPI.c - provide some arduino like spi functions - compatibility layer for
 *		rfid arduino library, functions are ported from C++ to C style.
 *  Copyright (C) 2016  Simon Kaufmann, HeKa
 *
 *  This file is part of ADRF transmitter firmware.
 *
 *  ADRF transmitter firmware is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ADRF transmitter firmware is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ADRF transmitter firmware.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <avr/io.h>

#include "main.h"
#include "pins.h"
#include "SPI.h"
volatile uint8_t spi_is_on = SPI_ON; /* on at beginnning so that rfid can be initialized, on means on for rfid and off for dds */
	/* dds interrupts are activated after intializing rfid so it is no problem
	 * if spi is on for rfid at beginning
	 */

/* standard instance for SPI */

/* SPI functions */

/**
 * SPI_transfer - transmit and receive byte via SPI interface
 * @data:	byte to transmit
 *
 *		Return: Received byte
 */
uint8_t SPI_transfer(uint8_t data)
{
	SPDR = data;
	while ((SPSR & (1 << SPIF)) == 0);
	return SPDR;
}

/*
 * these three functions SPI_begin, SPI_setBitOrder and SPI_setDataMode are
 * ignored -> SPI initialisation is done in dds.c
 */
void SPI_begin(void)
{
}

void SPI_setBitOrder(int x)
{
}

void SPI_setDataMode(int x)
{
}

/**
 * SPI_set_state - set if the spi is free at the moment or not
 * @state: either SPI_ON if spi can be accessed or SPI_OFF is spi cannot be accessed
 *
 *		If the state of spi is set to SPI_OFF then the function SPI_transfer will
 *		block until the state of spi is free again. Be careful not set the spi
 *		to SPI_OFF without setting it to SPI_ON again. Otherwise rfid library
 *		might freece.
 */
void SPI_set_state(uint8_t state)
{
	if (state == SPI_ON)	{
		spi_is_on = SPI_ON;
	} else {
		spi_is_on = SPI_OFF;
	}
}

/**
 * SPI_get_state - returns if spi was set to free for the moment
 *
 *		Return: either SPI_ON if spi is free or SPI_OFF if spi
 *		is not free. It is not the real state of spi access but only
 *		if rfid library is allowed to send or not.
 */
uint8_t SPI_get_state(void)
{
	return spi_is_on;
}

/**
 * SPI_is_free - functions checks if all CS lines are high
 *
 *		Return: TRUE if all CS lines are high and FALSE if one is
 *		low
 */
uint8_t SPI_is_free(void)
{
	uint8_t free = TRUE;
	if ((DDS_PORT & (1 << DDS_CS)) == 0)	{
		free = FALSE;
	} else if ((RFID_PORT & (1 << RFID_CS)) == 0)	{
		free = FALSE;
	}
	return free;
}
