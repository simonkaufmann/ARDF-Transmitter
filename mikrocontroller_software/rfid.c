/*
 *  rfid.c - routines for the rfid system and the participant registration system
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
#include <util/delay.h>

#include "main.h"
#include "pins.h"
#include "uart.h"
#include "rfid.h"
#include "user.h"

const char PROGMEM scan_picc_msg[] = "Scan PICC to see UID and type...\r\n";

/**
 * rfid_init - initialise rfid module
 */
void rfid_init(void)
{
	SPI_begin(); /* Init SPI bus */
	MFRC522_init(SS_PIN, RST_PIN); /* prepare output pins */
	PCD_Init();	/* Init MFRC522 card */
}

/**
 * rfid_loop - function gets call by main loop and checks for new transponders
 *
 *		If a new transponder is found a function in the user-software-module
 *		is called to communicate with the tag.
 */
void rfid_loop()
{
	if (PICC_IsNewCardPresent())	{
		if (PICC_ReadCardSerial())	{
			/* successfully recognised new tag */
			//DN("tag");
			user_new_tag();
		}
	}
	rfid_close_tag(); /* to make sure that communication is close */
}

/**
 * rfid_read_block - read and return a block from rfid tag
 * @block:	number of block between 0 and 15
 * @buff:	pointer to buffer where the block will be stored
 *			the first 16 bytes of the buffer are the 16 bytes of the block
 *			the last second bytes in buffer are checksum
 * @length:	pointer to byte with length of buffer in bytes (should be 18)
 *			bytes read are stored in this variable
 *
 *		Return: STATUS_OK on success and STATUS_??? error code on failure
 */
uint8_t rfid_read_block(uint8_t block, uint8_t *buffer, uint8_t *length)	{
	uint8_t ret;
	uint8_t i;
	MIFARE_Key key;
	for (i = 0; i < 6; i++)	{
		key.keyByte[i] = 0xFF; /* standard key 0x FF FF FF FF FF FF */
	}
	ret = PCD_Authenticate(PICC_CMD_MF_AUTH_KEY_A, block, &key, &(uid));
	if (ret != STATUS_OK)	{
		return ret;
	}
	ret = MIFARE_Read(block, buffer, length);
	return ret;
}

/**
 * rfid_write_block - write data to block of rfid tag
 * @block:	block number
 * @buffer: pointer to buffer where the data to write is stored
 * @length:	length of buffer (should be 16)
 *
 *		Return: STATUS_OK on success and STATUS_??? error code on failure
 */
uint8_t rfid_write_block(uint8_t block, uint8_t *buffer, uint8_t length)	{
	uint8_t i;
	MIFARE_Key key;
	for (i = 0; i < 6; i++)	{
		key.keyByte[i] = 0xFF; /* standard key 0x FF FF FF FF FF FF */
	}
	uint8_t ret = PCD_Authenticate(PICC_CMD_MF_AUTH_KEY_A, block, &key, &(uid));
	if (ret != STATUS_OK)	{
		return ret;
	}
	ret = MIFARE_Write(block, buffer, length);
	return ret;
}

/*
 * rfid_close_tag - close encrypted connection to tag
 *
 *		If programme authenticated at the tag this function must
 *		to be called before programme is able to communicate with another
 *		tag
 */
void rfid_close_tag(void)	{
	PCD_StopCrypto1();
}
