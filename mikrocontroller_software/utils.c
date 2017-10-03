/*
 *  utils.c - Miscellaneous util functions used in the project
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
#include <avr/pgmspace.h>

#include "main.h"
#include "utils.h"
#include "uart.h"

/*
 * public functions
 */

/**
 * str_compare_progmem - compare a string in sram and in progmem (flash memory)
 * @string1:	pointer to the first string in SRAM
 * @string2:	uint16_t value that represents a address in PGMSPACE containing
 *				the second string
 *
 *		Return: UTILS_STR_CONTAINS if string2 is equal to beginning of string1
 *		or UTILS_STR_FALSE if string1 do not contain string2 at beginning or
 *		UTILS_STR_EQUAL if string1 is equal to string2
 */
uint8_t str_compare_progmem(const char *string1, uint16_t string2)
{
	uint8_t i = 0;
	char byte;
	byte = pgm_read_byte(string2 + i);
	while (byte != 0)	{
		if (string1[i] == 0)	{
			return UTILS_STR_FALSE;
		} else if (string1[i] != byte)	{
			return UTILS_STR_FALSE;
		}
		i++;
		if (i == 255)	{
			return UTILS_STR_FALSE; /* overflow of index */
		}
		byte = pgm_read_byte(string2 + i);
	}
	if (string1[i] == 0)	{
		return UTILS_STR_EQUAL; /* string ends at same place like string2 -> they are complete equal */
	}
	return UTILS_STR_CONTAINS;
}

/**
 * str_to_int - converts a string to an integer value
 * @string:		pointer to the string to convert
 * @val:		pointer where the int value will be stored
 *
 *		Return: TRUE if the conversion was successful, FALSE if string is
 *		not a number or number ist too large
 */
uint8_t str_to_int(char *string, uint32_t *val)
{
	*val = 0;
	uint8_t i = 0;
	uint32_t val_new;
	while (string[i] != 0)	{
		if (string[i] >= '0' && string[i] <= '9')	{
			val_new = *val;
			val_new *= 10;
			val_new += string[i] - '0';
			if (val_new < *val)	{
				return FALSE; /* overflow of val has happened */
			}
			*val = val_new;
		} else	{
			return FALSE;
		}
		i++;
		if (i == 255)	{
			return FALSE; /* string is too long */
		}
	}
	return TRUE;
}

/**
 * int_to_string - convert an integer value to a string
 * @string:		pointer to the address where the converted string will be stored
 * @length:		size of the space usable for storing the string in bytes
 * @val:		integer value to convert
 *
 *		Supports only positive values
 *
 *		Return: TRUE if the conversion was successful and FALSE if
 *		the string buffer was too small
 */
uint8_t int_to_string(char *string, uint8_t length, uint32_t val)
{
	uint8_t i, ret = FALSE;
	for (i = 0; i < (length - 1); i++)	{
		string[i] = val % 10 + '0';
		val = val / 10;
		if (val == 0)	{
			ret = TRUE; /* no error because string-array is not too small! */
			break;
		}
	}

	string[i + 1] = 0;

	uint8_t j, temp;

	for (j = 0; j < i / 2 + 1; j++)	{
		temp = string[j];
		string[j] = string[i - j];
		string[i - j] = temp;
	}

	return ret;
}

/**
 * int_to_string_fixed_length - store an integer value as ascii in fixed lenght
 *	@string: pointer to char array where ascii will get stored
 *	@length: length of converted array (must be bigger than number of chars wanted
 *			 because of terminating zero)
 *	@val:	 integer value to convert
 *
 *		if integer ist too small for the whole string, zeros will be added
 *		at beginning of string
 *
 *		Return: TRUE if the conversion was successful and FALSE if the string
 *		buffer was too small
 */
uint8_t int_to_string_fixed_length(char *string, uint8_t length, uint32_t val)
{
	string[0] = 0;

	if (length < 2)	{
		return FALSE;
	}

	uint8_t ret = FALSE, i;
	for (i = 0; i < (length - 1); i++)	{
		string[i] = val % 10 + '0';
		val = val / 10;
		if (val == 0)	{
			ret = TRUE; /* no error because string-array is not too small! */
			break;
		}
	}
	if (ret == FALSE)	{
		return ret;
	}

	string[length - 1] = 0;

	uint8_t j, temp;

	for (j = 0; j < i / 2 + 1; j++)	{
		temp = string[j];
		string[j] = string[i - j];
		string[i - j] = temp;
	}

	int16_t k;
	for (k = length - 2; k > (length - 2) - i - 1; k--)	{
		string[k] = string[i - (length - 2 - k)];
	}
	for (j = 0; j < (length - 2) - i; j++)	{
		string[j] = '0';
	}
	string[length - 1] = 0;

	return ret;
}

/**
 * int_to_string_hex - convert an integer value to a string as hex number
 * @string:		pointer to the address where the converted string will be stored
 * @length:		size of space usable for storing the string in bytes
 * @val:		integer value to convert
 *
 *		Does only support positive values at the moment
 *
 *		Return: TRUE if the conversion was successful and FALSE if the string
 *		buffer was too small
 */
uint8_t int_to_string_hex(char *string, uint8_t length, uint32_t val)
{
	uint8_t i, ret = FALSE;
	for (i = 0; i < (length - 1); i++)	{
		string[i] = val % 16 + '0';
		val = val / 16;
		if (string[i] > '9')	{
			string[i] = string[i] - '9' - 1 + 'A';
		}
		if (val == 0)	{
			ret = TRUE; /* no error because string-array is not too small! */
			break;
		}
	}

	string[i+1] = 0;

	uint8_t j, temp;

	for (j = 0; j < i / 2 + 1; j++)	{
		temp = string[j];
		string[j] = string[i - j];
		string[i - j] = temp;
	}

	return ret;
}

/**
 * to_upper_case - convert all small letters to capital letters
 * @string:		points to a string that will get converted
 *
 *		String should not be longer than 256 byte (incuding terminating
 *		zero). Otherwise only the first 256 characters will get converted.
 */
void to_upper_case(char *string)
{
	uint8_t i = 0;
	while (string[i] != 0)	{
		if (string[i] >= 'a' && string[i] <= 'z')	{
			string[i] = string[i] - 'a' + 'A';
		}
		i++;
		if (i == 0)	{
			break; /* error string ist too long */
		}
	}
}
