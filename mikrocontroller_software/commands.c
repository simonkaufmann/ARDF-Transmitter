/*
 *  commands.c - the uart command parser
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

#include <avr/pgmspace.h> /* documentation see: http://www.nongnu.org/avr-libc/user-manual/pgmspace.html */
#include <avr/io.h>

#include "morse.h"
#include "main.h"
#include "startup.h"
#include "commands.h"
#include "rtc.h"
#include "uart.h"
#include "utils.h"
#include "dds.h"
#include "user.h"

#define START_TIME			0
#define STOP_TIME			1
#define TIME				2
#define START_DATE			0
#define STOP_DATE			1
#define DATE				2

#define NO_MODE				0
#define SET_ID_MODE			1 /* set tag id mode */
#define MODE_MAX			1

#define CMD_STATUS_OK				0
#define CMD_STATUS_NOT_RECOGNIZED	1
#define CMD_STATUS_ERR				2
#define CMD_STATUS_EXIT				3
#define CMD_STATUS_OK_NO_OK			4 /* do not return "OK" at ending of command */

/*
* indices for the commands array
* they must match with the array declartions beneath
*/
#define CMD_SET_TIME			0
#define CMD_SET_DATE			1
#define CMD_SET_STARTUP_TIME	2
#define CMD_SET_STOP_TIME		3
#define CMD_SET_STARTUP_DATE	4
#define CMD_SET_STOP_DATE		5
#define CMD_SET_WPM				6
#define CMD_SET_CALL			7
#define CMD_SET_FREQUENCY		8
#define CMD_SET_CRYSTAL_FREQUENCY	9
#define CMD_SET_FOX_NUMBER		10
#define CMD_SET_FOX_MAX			11
#define CMD_SET_MODULATION		12
#define CMD_SET_MORSING		13
#define CMD_RELOAD				14
#define CMD_EXIT				15
#define CMD_SET_ID				16
#define CMD_HELP				17
#define CMD_SHOW_CONFIGURATION	18
#define CMD_SET_AMPLITUDE		19
#define CMD_SET_SECRET			20
#define CMD_RESET_HISTORY		21
#define CMD_SET_TRANSMIT_MINUTE	22
#define CMD_GET_FOX_NUMBER		23
#define CMD_SET_BLINK			24
#define CMD_GET_CRYSTAL_FREQUENCY	25
#define CMD_PRINT_FOX_HISTORY	26
#define CMD_SET_RELOAD			27
#define CMD_MAX					27 /* highest index in array command */

const char PROGMEM cmd_set_time[] = "set time";
const char PROGMEM cmd_set_date[] = "set date";
const char PROGMEM cmd_set_startup_time[] = "set start time";
const char PROGMEM cmd_set_startup_date[] = "set start date";
const char PROGMEM cmd_set_stop_time[] = "set stop time";
const char PROGMEM cmd_set_stop_date[] = "set stop date";
const char PROGMEM cmd_set_wpm[] = "set wpm";
const char PROGMEM cmd_set_call[] = "set call sign";
const char PROGMEM cmd_set_frequency[] = "set frequency";
const char PROGMEM cmd_set_crystal_frequency[] = "set crystal frequency";
const char PROGMEM cmd_set_fox_number[] = "set fox number";
const char PROGMEM cmd_set_fox_max[] = "set fox max";
const char PROGMEM cmd_set_modulation[] = "set modulation";
const char PROGMEM cmd_set_morsing[] = "set morsing";
const char PROGMEM cmd_reload[] = "reload";
const char PROGMEM cmd_exit[] = "exit";
const char PROGMEM cmd_set_id[] = "set id";
const char PROGMEM cmd_help[] = "help";
const char PROGMEM cmd_show_configuration[] = "show config";
const char PROGMEM cmd_set_amplitude[] = "set amplitude";
const char PROGMEM cmd_set_secret[] = "set secret";
const char PROGMEM cmd_reset_history[] = "reset history";
const char PROGMEM cmd_set_transmit_minute[] ="set transmit minute";
const char PROGMEM cmd_get_fox_number[] = "get fox number";
const char PROGMEM cmd_set_blink[] = "set blinking";
const char PROGMEM cmd_get_crystal_frequency[] = "get crystal frequency";
const char PROGMEM cmd_print_fox_history[] = "print fox history";
const char PROGMEM cmd_set_reload[] = "set reload";

/*
 * arrays in flash memory have to be declared like this
 * refer to: http://www.nongnu.org/avr-libc/user-manual/pgmspace.html
 */
const PGM_P const commands[CMD_MAX + 1] =	{
	cmd_set_time,
	cmd_set_date,
	cmd_set_startup_time,
	cmd_set_stop_time,
	cmd_set_startup_date,
	cmd_set_stop_date,
	cmd_set_wpm,
	cmd_set_call,
	cmd_set_frequency,
	cmd_set_crystal_frequency,
	cmd_set_fox_number,
	cmd_set_fox_max,
	cmd_set_modulation,
	cmd_set_morsing,
	cmd_reload,
	cmd_exit,
	cmd_set_id,
	cmd_help,
	cmd_show_configuration,
	cmd_set_amplitude,
	cmd_set_secret,
	cmd_reset_history,
	cmd_set_transmit_minute,
	cmd_get_fox_number,
	cmd_set_blink,
	cmd_get_crystal_frequency,
	cmd_print_fox_history,
	cmd_set_reload,
};

/* help texts for each command */

const char PROGMEM help_cmd_set_time[] =
	"\"set time\" command:\r\n"
	"give current time in format HH:MM:SS\r\n"
	"hours have to be given in 24-hour format\r\n"
	"\r\n"
	"example: set time 16:59:59";
const char PROGMEM help_cmd_set_date[] =
	"\"set date\" command:\r\n"
	"give current date in format YYYY-MM-DD\r\n"
	"\r\n"
	"example: set date 2016-04-08";
const char PROGMEM help_cmd_set_startup_time[] =
	"\"set start time\" command:\r\n"
	"give start time of event in format HH:MM:SS\r\n"
	"hours have to be given in 24-hour format\r\n"
	"\r\n"
	"example: set start time 10:00:00";
const char PROGMEM help_cmd_set_startup_date[] =
	"\"set start date\" command:\r\n"
	"give start date of event in format YYYY-MM-DD\r\n"
	"\r\n"
	"example: set start date 2016-04-08";
const char PROGMEM help_cmd_set_stop_time[] =
	"\"set stop time\" command:\r\n"
	"give end time of event in format HH:MM:SS\r\n"
	"\r\n"
	"example: set stop time 23:59:59";
const char PROGMEM help_cmd_set_stop_date[] =
	"\"set stop date\" command:\r\n"
	"give end date of event in format YYYY-MM-DD\r\n"
	"\r\n"
	"example: set stop date 2016-04-08";
const char PROGMEM help_cmd_set_wpm[] =
	"\"set wpm\" command:\r\n"
	"set morsing speed in words per minute\r\n"
	"wpm value must be integer value and\r\n"
	"and should not be bigger than 60\r\n"
	"\r\nexample: set wpm 10";
const char PROGMEM help_cmd_set_call[] =
	"\"set call sign\" command:\r\n"
	"give call sign that should be used by the fox\r\n"
	"call sign must be either moe, moi, mos, moh, mo5 or mo\r\n"
	"\r\n"
	"example: set call sign moe";
const char PROGMEM help_cmd_set_frequency[] =
	"\"set frequency\" command:\r\n"
	"give frequency of output signal in hertz\r\n"
	"frequency should be between 3.5 to 3.8 MHz (80m band)\r\n"
	"or between 144 and 146 MHz (2m band)\r\n"
	"\r\n"
	"example (3.5MHz): set frequency 3500000";
const char PROGMEM help_cmd_set_crystal_frequency[] =
	"\"set crystal frequency\" command:\r\n"
	"give exact frequency of dds crystal for calibration in hertz\r\n"
	"\r\n"
	"example: set crystal frequency 25000123";
const char PROGMEM help_cmd_set_fox_number[] =
	"\"set fox number\" command:\r\n"
	"give number of fox between 0 and 5\r\n"
	"0 is for baken/demo transmitter\r\n"
	"1 - 5 is for all other transmitters\r\n"
	"setting is used to identify transmitters in pc software\r\n"
	"\r\n"
	"example: set fox number 1";
const char PROGMEM help_cmd_set_fox_max[] =
	"\"set fox max\" command:\r\n"
	"give maximum number of transmitters used in event\r\n"
	"(excluding baken/demo fox)\r\n"
	"setting is used as interval for transmit minutes\r\n"
	"5 means that fox will transmit every 5 minutes\r\n"
	"4 -> transmit every 4 minutes and so on\r\n"
	"\r\n"
	"example (all foxes used): set fox max 5";
const char PROGMEM help_cmd_set_modulation[] =
	"\"set modulation\" command:\r\n"
	"give either on or off to enable or disable amplitude modulation\r\n"
	"\r\nexample: set modulation on";
const char PROGMEM help_cmd_set_morsing[] =
	"\"set morsing\" command:\r\n"
	"give either on or off to disable or enable morsing\r\n"
	"if morsing is disabled continuous carrier is transmitted instead\r\n"
	"\r\n"
	"example: set morsing on";
const char PROGMEM help_cmd_reload[] =
	"\"reload\" command:\r\n"
	"no parameter\r\n"
	"reloads fox settings\r\n"
	"\r\n"
	"example: reload";
const char PROGMEM help_cmd_exit[] =
	"\"exit\" command:\r\n"
	"exits the set id mode after command \"set id\"\r\n"
	"set id mode is also exited if tag is written with id\r\n"
	"so exit is only needed if one want to cancel the id writing\r\n"
	"\r\nexample: exit";
const char PROGMEM help_cmd_set_id[] =
	"\"set id\" command:\r\n"
	"give id between 1 and 65535 to write onto the next rfid tag\r\n"
	"id is written as soon as the next tag is recognised\r\n"
	"while waiting for the tag no other commands are recognised\r\n"
	"type \"exit\" to cancel the process\r\n"
	"\r\n"
	"example: set id 100";
const char PROGMEM help_cmd_help[] = ""; /* this help text is never shown
										  *	because help_message is shown instead */
const char PROGMEM help_cmd_show_configuration[] =
	"\"show config\" commands:\r\n"
	"output configuartion of fox\r\n"
	"\r\n"
	"example: show config";
const char PROGMEM help_cmd_set_amplitude[] =
	"\"set amplitude\" command:\r\n"
	"give amplitude of transmitter output in percent\r\n"
	"\r\n"
	"example: set amplitude 100";
const char PROGMEM help_cmd_set_secret[] =
	"\"set secret\" command:\r\n"
	"give (random) secret key of fox between 1 and 65535\r\n"
	"key is written onto tags as proof that fox was really foundc\r\n"
	"\r\n"
	"example: set secret 34211";
const char PROGMEM help_cmd_reset_history[] =
	"\"reset history\" command:\r\n"
	"delete user history inside eeprom of transmitter\r\n"
	"\r\n"
	"example: reset history";
const char PROGMEM help_cmd_set_transmit_minute[] =
	"\"set transmit minute\" command:\r\n"
	"give transmit minute of fox between 0\r\n"
	"and the value set by \"set fox max\" minus 1\r\n"
	"0 means that fox sends at beginning of event\r\n"
	"1 means that fox sends one minute after beginning\r\n"
	"and so on\r\n"
	"\r\n"
	"example: set transmit minute 0";
const char PROGMEM help_cmd_get_fox_number[] =
	"\"get fox number\" command:\r\n"
	"fox returns its fox number set by \"set fox number\"\r\n"
	"\r\n"
	"example: get fox number";
const char PROGMEM help_cmd_set_blink[] =
	"\"set blinking\" command:\r\n"
	"give either on or off to let led of fox blink or not\r\n"
	"blinking can be used to check if correct fox is configured at the moment\r\n"
	"\r\n"
	"example: set blinking on";
const char PROGMEM help_cmd_get_crystal_frequency[] =
	"\"get crystal frequency\" command:\r\n"
	"fox outputs the frequency of the dds crystal calibrated\r\n"
	"by \"set crystal frequency\"\r\n"
	"\r\n"
	"example: get crystal frequency";
const char PROGMEM help_cmd_print_fox_history[] =
	"\"print fox history\" command:\r\n"
	"fox outputs the saved user history in eeprom\r\n"
	"\r\n"
	"example: print fox history";
const char PROGMEM help_cmd_set_reload[] =
	"\"set reload\" command:\r\n"
	"give either on or off to set if fox shall reload\r\n"
	"configuration after each command or not\r\n"
	"\r\n"
	"example: set reload on";

const PGM_P const help_commands[CMD_MAX + 1] =	{
	help_cmd_set_time,
	help_cmd_set_date,
	help_cmd_set_startup_time,
	help_cmd_set_stop_time,
	help_cmd_set_startup_date,
	help_cmd_set_stop_date,
	help_cmd_set_wpm,
	help_cmd_set_call,
	help_cmd_set_frequency,
	help_cmd_set_crystal_frequency,
	help_cmd_set_fox_number,
	help_cmd_set_fox_max,
	help_cmd_set_modulation,
	help_cmd_set_morsing,
	help_cmd_reload,
	help_cmd_exit,
	help_cmd_set_id,
	help_cmd_help,
	help_cmd_show_configuration,
	help_cmd_set_amplitude,
	help_cmd_set_secret,
	help_cmd_reset_history,
	help_cmd_set_transmit_minute,
	help_cmd_get_fox_number,
	help_cmd_set_blink,
	help_cmd_get_crystal_frequency,
	help_cmd_print_fox_history,
	help_cmd_set_reload,
};

const char PROGMEM prompt_no_mode[] = "ARDF Transmitter# ";

const PGM_P const prompt[MODE_MAX + 1]	=	{
	prompt_no_mode
};

const char PROGMEM string_on[] = "on";
const char PROGMEM string_off[] = "off";

/*
 * if changing order -> change also in morse.h
 * definitions CALL_MOE etc.
 */
const char PROGMEM call_moe[] = "MOE";
const char PROGMEM call_moi[] = "MOI";
const char PROGMEM call_mos[] = "MOS";
const char PROGMEM call_moh[] = "MOH";
const char PROGMEM call_mo5[] = "MO5";
const char PROGMEM call_mo[] = "MO";

const char PROGMEM frequency_set_to[] = "Frequency set to: ";


const PGM_P const calls[CALL_MAX + 1] = {
	call_moe,
	call_moi,
	call_mos,
	call_moh,
	call_mo5,
	call_mo
};

uint8_t mode = NO_MODE;

const char PROGMEM welcome_message[] =
	"ARDF Transmitter Firmware\r\n"
	"\r\n"
	"Copyright (C) 2016 Simon Kaufmann, Josef Heel, HeKa\r\n"
	"\r\n"
	"This program comes with ABSOLUTELY NO WARRANTY\r\n"
	"This is free software, you are welcome to redistribute it\r\n"
	"under the terms of the GNU General Public License version 3.\r\n"
	"\r\n"
	"For further information and documentation related to this project visit:\r\n"
	"http://ardf.heka.or.at/\r\n"
	"\r\n"
	"Type \"help\" for a list of available commands\r\n"
	"\r\n";

const char PROGMEM help_message_beginning[] =
	"Commands available:\r\n\r\n";
const char PROGMEM help_message_ending[] =
	"Type \"help command_name\" to get description for specific command\r\n";

const char PROGMEM cmd_not_rec[] = "Command not recognised";
const char PROGMEM err[] = "Error";

uint8_t reload_each_command = TRUE;

/*
 * internal functions
 */

/**
 * get_command_number - returns the command number of the given command string
 * @string:			line read from uart with a command for transmitter
 * @command_number:	points to integer where the command number will be stored
 *
 *		Return: CMD_STATUS_OK if the command was recognized otherwise CMD_STATUS_NOT_RECOGNIZED
 */
static uint8_t get_command_number(char *string, uint8_t *command_number)	{
	*command_number = 0;
	while (*command_number <= CMD_MAX)	{
		if (str_compare_progmem(string, (uint16_t)commands[*command_number]) == UTILS_STR_CONTAINS ||
				str_compare_progmem(string, (uint16_t)commands[*command_number]) == UTILS_STR_EQUAL) {
			return CMD_STATUS_OK;
		}
		(*command_number)++;
	}
	return CMD_STATUS_NOT_RECOGNIZED;
}

/**
 * command_set_reload_each - set if each command should cause a setting reload
 * @reload:		TRUE if reload should be made after eache command, FALSE otherwise
 */
static void commands_set_reload_each(uint8_t reload)
{
	reload_each_command = reload;
}

/**
 * command_get_parameter - get a string with parameter of a command line
 * @command_number:		number of the command that was recognized in this
 *						command line
 * @string_in:			pointer to string that contains the command line
 *						note: this string might be modified (cut off anything
 *						after the first parameter)
 * @string_out:			pointer to a pointer to a string. If the function returns
 *						TRUE, this pointer will point to the string with the
 *						command-parameter. After the ending of the parameter
 *						(Parameter is ended with or '\r' or '\n') is 0
 *						is added so that string is zero terminated.
 *
 *		Return: CMD_STATUS_ERR if getting parameter failed, CMD_STATUS_OK if parameter was
 *		successful saved in string_out
 */
static uint8_t command_get_parameter(uint8_t command_number, char *string_in, char **string_out)
{
	uint8_t length = 0;
	uint8_t byte = pgm_read_byte(commands[command_number]);
	while (byte != 0)	{
		length++;
		byte = pgm_read_byte(commands[command_number] + length);
		if (length == 255)	{
			return CMD_STATUS_ERR; /* commmand-string is too long */
		}
	}

	*string_out = string_in + length;
	while (**string_out == ' ')	{
		(*string_out)++;
	}

	length = 0;
	while ((*string_out)[length] != 0 &&
			(*string_out)[length] != '\r' && (*string_out)[length] != '\n')	{
		length++;
		if (length == 255)	{
			return CMD_STATUS_ERR; /* overflow happened */
		}
	}
	(*string_out)[length] = 0; /* zero terminate at end of string */
	return CMD_STATUS_OK;
}

/**
 * execute_time - execute command for setting startup and stop date and time
 * @command:		the command number that was recognized
 * @parameter:		the parameter stringof the command
 * @separator:		the separator for time or date values (either ':' for time
 *					and '-' for date)
 * @start_type:		Either STARTUP_YEAR for date and STARTUP_HOUR for time
 *					or RTC_YEAR or RTC_HOUR
 * @start_stop:		Either START_TIME or STOP_TIME or TIME
 *
 *		Return: CMD_STATUS_OK if parsing was successful, CMD_STATUS_ERR on failure
 */
static uint8_t execute_time(uint8_t command, char *parameter,
				char separator, uint8_t start_type, uint8_t start_stop)
{
	uint8_t return_code = CMD_STATUS_OK;
	char *par[3];
	uint8_t par_num;
	uint8_t i = 0;
	for (par_num = 0; par_num < 3; par_num++)	{
		par[par_num] = parameter + i;
		while (parameter[i] != separator && (par_num != 2 || parameter[i] != 0))	{
			i++;
			if (parameter[i] == 0 && par_num < 2)	{
				return CMD_STATUS_ERR; /* parameter is not in right form */
			}
		}
		parameter[i] = 0;
		i++;
	}

	uint8_t type, ret;
	uint32_t time;
	i = 0;
	rtc_disable_avr_interrupt();
	for (type = start_type; type >= (start_type - 2) && type <= start_type; type--)	{
		ret = str_to_int(par[i], &time);
		if (ret != TRUE)	{
			return_code = CMD_STATUS_ERR;
			goto execute_time_return;
		}
		if (type == STARTUP_YEAR)	{
			time = time % 100;
		}
		if (start_stop == START_TIME)	{
			if (startup_set_start_time(type, time) != TRUE)	{
				return_code = CMD_STATUS_ERR;
				goto execute_time_return;
			}
		} else if (start_stop == STOP_TIME)	{
			if (startup_set_stop_time(type, time) != TRUE)	{
				return_code = CMD_STATUS_ERR;
				goto execute_time_return;
			}
		} else	{
			rtc_set_time(type, time);
		}
		i++;
		if (i > 3) {
			return_code = CMD_STATUS_ERR;
			goto execute_time_return;
		}
	}
	if (start_stop == START_TIME)	{
		startup_set_start_time(STARTUP_WEEKDAY, 0);
	} else if (start_stop == STOP_TIME)	{
		startup_set_stop_time(STARTUP_WEEKDAY, 0);
	} else	{
		rtc_set_time(RTC_WEEKDAY, 0);
	}

execute_time_return:
	rtc_enable_avr_interrupt();
	return return_code;
}

/**
 * execute_set_wpm - save words per minute
 * @parameter:	the string with the commands parameter
 *				(should contain the ascii encoded number for wpm)
 *
 *		Return: CMD_STATUS_OK on success, CMD_STATUS_ERR on failure
 */
static uint8_t execute_set_wpm(char *parameter)
{
	uint32_t wpm;
	uint8_t ret;
	ret = str_to_int(parameter, &wpm);
	if (ret != TRUE)	{
		return CMD_STATUS_ERR;
	}
	if (wpm > MORSE_WPM_MAX || wpm < MORSE_WPM_MIN)	{
		return CMD_STATUS_ERR;
	}
	uint16_t morse_unit = morse_convert_wpm_morse_unit(wpm);
	morse_set_morse_unit(morse_unit);
	return CMD_STATUS_OK;
}

/**
 * execute_set_call - process the set call command
 * @parameter:	call sign like "moe", "moi" and so on
 *
 *		Return: CMD_STATUS_OK on success, CMD_STATUS_ERR on failure
 */
static uint8_t execute_set_call(char *parameter)
{
	to_upper_case(parameter);
	uint8_t call = 0;
	while (call <= CALL_MAX)	{
		if (str_compare_progmem(parameter, (uint16_t)calls[call]) == UTILS_STR_EQUAL)	{
			morse_set_call_sign(call);
			return CMD_STATUS_OK;
		}
		call++;
	}
	return CMD_STATUS_ERR;
}

/**
 * execute_set_frequency - process set frequency command
 * @parameter:	frequency of output signal in hertz
 *
 *		Return: CMD_STATUS_OK on success, CMD_STATUS_ERR on failure
 */
static uint8_t execute_set_frequency(char *parameter)
{
	uint32_t freq;
	uint8_t ret = str_to_int(parameter, &freq);
	if (ret != TRUE)	{
		return CMD_STATUS_ERR;
	}
	if (dds_set_frequency(freq) != TRUE)	{
		return CMD_STATUS_ERR;
	}
	return CMD_STATUS_OK;
}

/**
 * execute_set_crystal_frequency - process crystal calibration command
 * @parameter:	string with frequency of crystal of dds in hertz
 *
 *		Return: CMD_STATUS_OK on success, CMD_STATUS_ERR on failure
 */
static uint8_t execute_set_crystal_frequency(char *parameter)
{
	uint32_t freq;
	uint8_t ret = str_to_int(parameter, &freq);
	if (ret != TRUE)	{
		return CMD_STATUS_ERR;
	}
	if (dds_set_crystal_frequency(freq) != TRUE)	{
		return CMD_STATUS_ERR;
	}
	return CMD_STATUS_OK;
}

/**
 * execute_set_fox_number - process set fox number command
 * @parameter:	string with number of this fox
 *
 *		Return: CMD_STATUS_OK on success, CMD_STATUS_ERR on failure
 */
static uint8_t execute_set_fox_number(char *parameter)
{
	uint32_t fox_number;
	uint8_t ret = str_to_int(parameter, &fox_number);
	if (ret != TRUE)	{
		return CMD_STATUS_ERR;
	}
	if (morse_set_fox_number(fox_number) != TRUE)	{
		return CMD_STATUS_ERR;
	}
	return CMD_STATUS_OK;
}

/**
 * execute_set_fox_max - process set fox max command
 * @parameter: string with number of foxes used in event
 *
 *		Return: CMD_STATUS_OK on success, CMD_STATUS_ERR on failure
 */
static uint8_t execute_set_fox_max(char *parameter)
{
	uint32_t fox_max;
	uint8_t ret = str_to_int(parameter,  &fox_max);
	if (ret != TRUE)	{
		return CMD_STATUS_ERR;
	}
	if (morse_set_fox_max(fox_max) != TRUE)	{
		return CMD_STATUS_ERR;
	}
	return CMD_STATUS_OK;
}

/**
 * execute_set_modulation - process set modulation command
 * @parameter: either "on" or "off"
 *
 *		Return: CMD_STATUS_OK on success, CMD_STATUS_ERR on failure
 */
static uint8_t execute_set_modulation(char *parameter)
{
	if (str_compare_progmem(parameter, (uint16_t)&string_on) != UTILS_STR_FALSE)	{
		dds_set_modulation(TRUE);
		return CMD_STATUS_OK;
	} else if (str_compare_progmem(parameter, (uint16_t)&string_off) != UTILS_STR_FALSE)	{
		dds_set_modulation(FALSE);
		return CMD_STATUS_OK;
	}
	return CMD_STATUS_ERR;
}

/**
 * execute_set_morsing - execute the command for morsing on or off
 * @parameter: either "on" or "off"
 *
 *		if morsing is set to off the device will transmit a continuous
 *		carrier without morse signals
 *
 *		Return: CMD_STATUS_OK if parameter was "on" or "off", returns CMD_STATUS_ERR otherwise
 */
static uint8_t execute_set_morsing(char *parameter)
{
	if (str_compare_progmem(parameter, (uint16_t)&string_on) != UTILS_STR_FALSE)	{
		morse_set_morse_mode(TRUE);
		return CMD_STATUS_OK;
	} else if (str_compare_progmem(parameter, (uint16_t)&string_off) != UTILS_STR_FALSE)	{
		morse_set_morse_mode(FALSE);
		return CMD_STATUS_OK;
	}
	return CMD_STATUS_ERR;
}

/**
 * execute_reload - execute the command for reloading the configuration
 * @parameter: any string
 *
 *		Return: CMD_STATUS_OK
 */
static uint8_t execute_reload(char *parameter)
{
	main_reload();
	return CMD_STATUS_OK;
}

/**
 * execute_set_id - execute the set id command
 * @parameter: string with id
 *
 *		This command is to enter the set id mode.
 *		Give no parameter to set id. Specify the tag ids
 *		when in set id mode with the id <tagid> command.
 *
 *		Return: CMD_STATUS_OK
 */
static uint8_t execute_set_id(char *parameter)
{
	uint32_t tagid;
	if (str_to_int(parameter, &tagid) != TRUE)	{
		return CMD_STATUS_ERR;
	}
	if (tagid > 0xffff || tagid == 0x00)	{
		return CMD_STATUS_ERR;
	}
	if (user_set_id((uint16_t)tagid) != TRUE)	{
		return CMD_STATUS_ERR;
	}
	return CMD_STATUS_OK;
}

/**
 * send_help_text - print standard help text to uart
 */
static void send_help_text(void)
{
		uart_send_text_flash((uint16_t)help_message_beginning);
		uint8_t i = 0;
		for (i = 0; i <= CMD_MAX; i++)	{
			uart_send_text_flash((uint16_t)commands[i]);
			UART_NEWLINE();
		}
		UART_NEWLINE();
		uart_send_text_flash((uint16_t)help_message_ending);
}

/**
 * execute_help - show the help message
 * @parameter:	help text of the given command will be sent
 *
 *		Return: CMD_STATUS_OK
 */
static uint8_t execute_help(char *parameter)
{
	uint8_t command_number = 0;
	if (get_command_number(parameter, &command_number) == CMD_STATUS_OK)	{
		if (command_number == CMD_HELP)	{
			send_help_text();
		} else {
			uart_send_text_flash((uint16_t)help_commands[command_number]);
			UART_NEWLINE();
		}
	} else {
		send_help_text();
	}
	return CMD_STATUS_OK_NO_OK;
}

/**
 * execute_show_configuration - print current configuration of eeprom
 * @parameter: any string
 *
 *		Return: always CMD_STATUS_OK_NO_OK
 */
static uint8_t execute_show_configuration(char *parameter)
{
	UART_NEWLINE();
	dds_show_configuration();
	rtc_show_configuration();
	startup_show_configuration();
	morse_show_configuration();
	user_show_configuration();
	return CMD_STATUS_OK_NO_OK;
}

/**
 * execute_set_amplitude - set amplitude of dds
 * @parameter:	string containing amplitude in percent
 *
 *		Return: CMD_STATUS_OK on success, CMD_STATUS_ERR on failure
 */
static uint8_t execute_set_amplitude(char *parameter)
{
	uint32_t amplitude;
	if (str_to_int(parameter, &amplitude) == TRUE)	{
		if (dds_set_amplitude_percentage(amplitude) != TRUE)	{
			return CMD_STATUS_ERR;
		}
		return CMD_STATUS_OK;
	}
	return CMD_STATUS_ERR;
}

/**
 * execute_set_secret - save the given secret key
 * @parameter: string with secret key of fox between 1 and 65535
 *
 *		Return: CMD_STATUS_OK on success, CMD_STATUS_ERR otherwise
 */
static uint8_t execute_set_secret(char *parameter)
{
	uint32_t secret;
	if (str_to_int(parameter, &secret) != TRUE)	{
		return CMD_STATUS_ERR;
	}
	if (user_set_secret(secret) != TRUE)	{
		return CMD_STATUS_ERR;
	}
	return CMD_STATUS_OK;
}

/**
 * execute_reset_history - reset the ext_eeprom pointer to start a new user database
 * @parameter: any string
 *
 *		Return: always CMD_STATUS_OK
 */
static uint8_t execute_reset_history(char *parameter)
{
	user_clear_history();
	return CMD_STATUS_OK;
}

/**
 * execute_set_transmit_minute - set the minute where the fox will transmit
 * @parameter: number of minute where fox shall transmit
 *
 *		Return: CMD_STATUS_OK or CMD_STATUS_ERR
 */
static uint8_t execute_set_transmit_minute(char *parameter)
{
	uint32_t val;
	if (str_to_int(parameter, &val) != TRUE)	{
		return CMD_STATUS_ERR;
	}
	if (morse_set_transmit_minute(val) != TRUE)	{
		return CMD_STATUS_ERR;
	}
	return CMD_STATUS_OK;
}

/**
 * execute_get_fox_number - output the fox number
 * @parameter: any string
 *
 *		Return: always CMD_STATUS_OK
 */
static uint8_t execute_get_fox_number(char *parameter)
{
	uart_send_int(morse_get_fox_number());
	UART_NEWLINE();
	return CMD_STATUS_OK;
}

/**
 * execute_get_crystal_frequency - output the crystal frequency
 * @parameter: any string
 *
 *		Return: always CMD_STATUS_OK
 */
static uint8_t execute_get_crystal_frequency(char *parameter)
{
	uart_send_int(dds_get_crystal_frequency());
	UART_NEWLINE();
	return CMD_STATUS_OK;
}

/**
 * execute_set_blink - set status led to blinking or to off
 * @parameter: either "on" or "off"
 *
 *		Return: CMD_STATUS_OK on success, CMD_STATUS_ERR otherwise
 */
static uint8_t execute_set_blink(char *parameter)
{
	if (str_compare_progmem(parameter, (uint16_t)&string_on) != UTILS_STR_FALSE)	{
		main_set_blinking(TRUE);
		return CMD_STATUS_OK;
	} else if (str_compare_progmem(parameter, (uint16_t)&string_off) != UTILS_STR_FALSE)	{
		main_set_blinking(FALSE);
		return CMD_STATUS_OK;
	}
	return CMD_STATUS_ERR;
}

/**
 * execute_print_fox_history - print history of fox saved in eeprom via uart
 * @parameter: any string
 *
 *		Return: always CMD_STATUS_OK_NO_OK
 */
static uint8_t execute_print_fox_history(char *parameter)	{
	user_print_fox_history();
	return CMD_STATUS_OK_NO_OK;
}

/**
 * execute_set_reload - enable or disable reloading after command
 * @parameter: either "on" or "off"
 *
 *		This setting is only saved in ram and therefore cleared after
 *		a new boot
 *
 *		"set reload on" also does a reload at the same time!
 *
 *		Return: CMD_STATUS_OK on success, CMD_STATUS_ERR otherwise
 */
static uint8_t execute_set_reload(char *parameter)
{
	if (str_compare_progmem(parameter, (uint16_t)&string_on) != UTILS_STR_FALSE)	{
		commands_set_reload_each(TRUE);
		return CMD_STATUS_OK;
	} else if (str_compare_progmem(parameter, (uint16_t)&string_off) != UTILS_STR_FALSE)	{
		commands_set_reload_each(FALSE);
		return CMD_STATUS_OK;
	}
	return CMD_STATUS_ERR;
}

/*
 * public functions
 */

/**
 * commands_print_welcome_message - print the welcome message for beginning of session
 */
void commands_print_welcome_message(void)
{
	uart_send_text_flash((uint16_t)welcome_message);
}

/**
 * commands_print_prompt - output prompt to uart depending on current command mode
 */
void commands_print_prompt(void)
{
	uart_send_text_flash((uint16_t)prompt[mode]);
	UART_NEWLINE();
}

/**
 * commands_execute - called by main loop to check for new commands
 *					  and to process any new commands
 */
uint8_t commands_execute()
{
	static uint8_t during_set_id = FALSE;
	uint8_t ret, cmd;

	/*
	 * if module is to write id to tag -> do not execute any other commands
	 * at the moment, just wait until tag is written or "exit" ist received
	 * over uart.
	 * This means that all other commands that are received at the moment
	 * are thrown away.
	 */
	if (during_set_id == TRUE)	{
		if (user_check_set_id_state() == TRUE)	{
			during_set_id = FALSE;
		} else {
			char *string;
			if (uart_receive_buffer_text(&string) != TRUE)	{
				return FALSE;
			}
			if (str_compare_progmem(string, (uint16_t)cmd_exit) == UTILS_STR_FALSE)	{
				return FALSE;
			}
			/* "exit" command was received */
			user_cancel_set_id();
			during_set_id = FALSE;
		}
		if (during_set_id == FALSE)	{
			ret = CMD_STATUS_EXIT;
			goto execute_command_end;
		}
	}

	char *string;
	if (uart_receive_buffer_text(&string) == FALSE)	{
		return FALSE;
	}

	ret = get_command_number(string, &cmd);
	if (ret != CMD_STATUS_OK)	{
		goto execute_command_end;
	}

	char *parameter;
	parameter = string;
	ret = command_get_parameter(cmd, string, &parameter);
	if (ret != CMD_STATUS_OK)	{
		goto execute_command_end;
	}

	if (mode == NO_MODE)	{
		if (cmd == CMD_SET_TIME)	{
			ret = execute_time(cmd, parameter, ':', RTC_HOUR, TIME);
		} else if (cmd == CMD_SET_DATE)	{
			ret = execute_time(cmd, parameter, '-', RTC_YEAR, TIME);
		} else if (cmd == CMD_SET_STARTUP_TIME)	{
			ret = execute_time(cmd, parameter, ':', STARTUP_HOUR, START_TIME);
		} else if (cmd == CMD_SET_STOP_TIME)	{
			ret = execute_time(cmd, parameter, ':', STARTUP_HOUR, STOP_TIME);
		} else if (cmd == CMD_SET_STARTUP_DATE)	{
			ret = execute_time(cmd, parameter, '-', STARTUP_YEAR, START_TIME);
		} else if (cmd == CMD_SET_STOP_DATE)	{
			ret = execute_time(cmd, parameter, '-', STARTUP_YEAR, STOP_TIME);
		} else if (cmd == CMD_SET_WPM)	{
			ret = execute_set_wpm(parameter);
		} else if (cmd == CMD_SET_CALL)	{
			ret = execute_set_call(parameter);
		} else if (cmd == CMD_SET_FREQUENCY)	{
			ret = execute_set_frequency(parameter);
		} else if (cmd == CMD_SET_CRYSTAL_FREQUENCY)	{
			ret = execute_set_crystal_frequency(parameter);
		} else if (cmd == CMD_SET_FOX_NUMBER)	{
			ret = execute_set_fox_number(parameter);
		} else if (cmd == CMD_SET_FOX_MAX)	{
			ret = execute_set_fox_max(parameter);
		} else if (cmd == CMD_SET_MODULATION)	{
			ret = execute_set_modulation(parameter);
		} else if (cmd == CMD_SET_MORSING)	{
			ret = execute_set_morsing(parameter);
		} else if (cmd == CMD_RELOAD)	{
			ret = execute_reload(parameter);
		} else if (cmd == CMD_SET_ID)	{
			ret = execute_set_id(parameter);
			during_set_id = TRUE;
			return FALSE;
		} else if (cmd == CMD_HELP)	{
			ret = execute_help(parameter);
		} else if (cmd == CMD_SHOW_CONFIGURATION)	{
			ret = execute_show_configuration(parameter);
		} else if (cmd == CMD_SET_AMPLITUDE)	{
			ret = execute_set_amplitude(parameter);
		} else if (cmd == CMD_SET_SECRET)	{
			ret = execute_set_secret(parameter);
		} else if (cmd == CMD_RESET_HISTORY)	{
			ret = execute_reset_history(parameter);
		} else if (cmd == CMD_SET_TRANSMIT_MINUTE)	{
			ret = execute_set_transmit_minute(parameter);
		} else if (cmd == CMD_GET_FOX_NUMBER)	{
			ret = execute_get_fox_number(parameter);
		} else if (cmd == CMD_SET_BLINK)	{
			ret = execute_set_blink(parameter);
		} else if (cmd == CMD_GET_CRYSTAL_FREQUENCY)	{
			ret = execute_get_crystal_frequency(parameter);
		} else if (cmd == CMD_PRINT_FOX_HISTORY)	{
			ret = execute_print_fox_history(parameter);
		} else if (cmd == CMD_SET_RELOAD)	{
			ret = execute_set_reload(parameter);
		} else {
			/* message for command not in this mode */
		}

		if (ret == CMD_STATUS_OK || CMD_STATUS_OK_NO_OK)	{
#ifdef EACH_COMMAND_RELOAD
			if (reload_each_command == TRUE)	{
				main_reload();
			}
#endif
		}
		if (ret == CMD_STATUS_OK)	{
			uart_send_text_sram("OK\r\n");
		}
	}

execute_command_end:
	if (ret == CMD_STATUS_NOT_RECOGNIZED)	{
		uart_send_text_flash((uint16_t)cmd_not_rec); /* output command not recognized */
		UART_NEWLINE();
	} else if (ret == CMD_STATUS_ERR)	{
		uart_send_text_flash((uint16_t)err); /* error */
		UART_NEWLINE();
	} else if (ret == CMD_STATUS_EXIT)	{
		uart_send_text_sram("OK");
		UART_NEWLINE();
	}

	commands_print_prompt();

	return ret;
}
