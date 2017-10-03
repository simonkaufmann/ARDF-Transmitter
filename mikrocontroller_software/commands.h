/*
 *  commands.h - definitions of functions for parsing the received uart commands
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

#ifndef COMMANDS_H
#define COMMANDS_H

uint8_t commands_execute(void);

void commands_print_welcome_message(void);
void commands_print_prompt(void);

#endif
