// /*
//  *  dds_modulation.c - functions for using AD9833 DDS chip (for amplitude modulation)
//  *  Copyright (C) 2020  Simon Kaufmann, HeKa
//  *
//  *  This file is part of ADRF transmitter firmware.
//  *
//  *  ADRF transmitter firmware is free software: you can redistribute it and/or
//  *  modify it under the terms of the GNU General Public License as published by
//  *  the Free Software Foundation, either version 3 of the License, or
//  *  (at your option) any later version.
//  *
//  *  ADRF transmitter firmware is distributed in the hope that it will be useful,
//  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  *  GNU General Public License for more details.
//  *
//  *  You should have received a copy of the GNU General Public License
//  *  along with ADRF transmitter firmware.
//  *  If not, see <http://www.gnu.org/licenses/>.
//  */

// #include <avr/io.h>

// #include "dds_modulation.h"
// #include "dds.h"
// #include "main.h"
// #include "pins.h"
// #include "SPI.h"

// #define MODULATION_FREQUENCY	600
// #define FREQ_TO_FTW_MODULATION(freq)		((uint32_t) ((double)freq / (dds_get_crystal_frequency()) * 0x10000000UL))

// /*
//  * modulation dds register addresses
//  */
// #define MOD_B28         13      // needs to be at 1 to write high and low FTW at once
// #define MOD_FSELECT     11      // we only use FREG0
// #define MOD_PSELECT     10      // we only use PREG0
// #define MOD_RESET       8       // device is held in reset when reset = 1
// #define MOD_SLEEP1      7       // needs to be at 0 to be active
// #define MOD_SLEEP12     6       // same here
// #define MOD_OPBITEN     5       // needs to be 0, otherwise it will bypass the DAC and output square
// #define MOD_MODE        1       // needs to be 0 to output sinusoidal waveform

// /*
//  * internal functions
//  */

// /**
//  * wait_spi - wait until SPI transmit is ready
//  */
// static inline void wait_spi(void)	{
// 	while ((SPSR & (1 << SPIF)) == 0);
// }

// static uint8_t dds_modulation_write_register(uint8_t byte_1, uint8_t byte_0)
// {
// 	if (SPI_is_free() == FALSE)	{
// 		return FALSE;
// 	}

// 	DDS_PORT &= ~(1 << DDS_MODULATION_CS);
// 	SPDR = byte_1;
// 	wait_spi();
// 	SPDR = byte_0;
// 	wait_spi(); 
// 	DDS_PORT |= (1 << DDS_MODULATION_CS);
	
// 	return TRUE; 
// }

// static uint8_t dds_modulation_set_frequency(uint32_t frequency) {
// 	uint8_t b0 = 0x00, b1 = 0x00; 

// 	uint32_t ftw = FREQ_TO_FTW_MODULATION(frequency);
// 	// Write freqency tuning word 
// 	// FTW needs to be 6442 to result in approx 600Hz modulation freq at 25MHz clock
// 	// This means: 0 in the upper 14-bit part, and 6442 in the lower 14-bit part (192A)
// 	// Writing lower part of FTW: 
// 	b1 = (1 << 6) | ((ftw >> 8) & 0x3f); // means 25 in the upper bits
// 	b0 = ftw & 0xff;
// 	dds_modulation_write_register(b1, b0);
		
// 	// Writing upper part of FTW: 
// 	b1 = (1 << 6) | ((ftw >> 22) & 0x3f);
// 	b0 = (ftw >> 14) & 0xff;
// 	dds_modulation_write_register(b1, b0);  

// 	return TRUE;	
// }

// static uint8_t dds_modulation_test_on(void)
// {
// 		uint8_t b0 = 0x00, b1 = 0x00; 

// 		// Needs other SPI settings
// 		// these are: SPI Clock needs to be high in idle, data is read on the falling edge, shifted out on the rising edge
// 		// In AVR-terms, this means: CPOL = 1, CPHA = 0. 
// 		// The standard setting (clock idle low, data read on rising) does not work!
// 		SPCR |= (1 << SPE) | (1 << MSTR) | (1 << CPOL);
	
// 		// Write control regs with reset on (1)
// 		b1 = (1<<(MOD_B28-8))|(1<<(MOD_RESET-8));
// 		b0 = 0x00; // No sleeping etc. here
// 		dds_modulation_write_register(b1, b0); 
			  
// 		dds_modulation_set_frequency(MODULATION_FREQUENCY);

// 		// Write 0 to the phase register (phase does not matter here)
// 		b1 = (1<<7)|(1<<6); 
// 		b0 = 0x00; 
// 		dds_modulation_write_register(b1, b0);  
		
// 		// Disable reset (0)
// 		b1 = (1<<(MOD_B28-8));
// 		b0 = 0x00; // All set to 0 here
// 		dds_modulation_write_register(b1, b0); 
		
// 		// Restore old SPI settings
// 		SPCR |= (1 << SPE) | (1 << MSTR);
		
// 	return TRUE;
// }

// static uint8_t dds_modulation_test_off(void)
// {
// 		uint8_t b0 = 0x00, b1 = 0x00; 

// 		// Needs other SPI settings (see dds_mod_test_on())
// 		SPCR |= (1 << SPE) | (1 << MSTR) | (1 << CPOL);

// 		// Write control regs with reset on (1)
// 		b1 = (1<<(MOD_B28-8))|(1<<(MOD_RESET-8));
// 		b0 = (1<<MOD_SLEEP1)|(1<<MOD_SLEEP12); // Clock disabled and DAC disabled
	
// 		dds_modulation_write_register(b1, b0);

// 		// Restore old SPI settings
// 		SPCR |= (1 << SPE) | (1 << MSTR);
		
// 		return TRUE;
// }

// void dds_modulation_init() {
// 	// The modulation-dds is off by default
// 	dds_modulation_test_off();

// 	if (dds_get_modulation()==TRUE)
// 	{
// 		dds_modulation_test_on(); 
// 	}
// }