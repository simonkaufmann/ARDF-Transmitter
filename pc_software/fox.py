#!/usr/bin/python
# -*- coding: utf-8 -*-

#
# fox.py - Functions for communicating with fox
# Copyright (C) 2016  Simon Kaufmann, HeKa
#
# This file is part of ADRF transmitter firmware.
#
# ADRF Transmitter is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# ADRF transmitter firmware is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with ADRF Transmitter.  
# If not, see <http://www.gnu.org/licenses/>.
#

#
# system imports
#
import datetime

#
# own imports
#
import settings
import utils
import fox_serial

def CheckRespond(allstr, with_error=True):
    try:
        if (allstr[len(allstr) - 2].find(settings.FOX_PROMPT) >= 0 and allstr[len(allstr) - 3].find(settings.OK_PROMPT) >= 0) or\
           (allstr[len(allstr) - 1].find(settings.FOX_PROMPT) >= 0 and allstr[len(allstr) - 2].find(settings.OK_PROMPT) >= 0):
            pass
        else:
            if with_error:
                utils.MessageBox("Fox not reachable, maybe there is not transmitter connected to selected COM-port?")
            return False
        return True
    except IndexError:
        return False

#
# ReadNumber - read number of a fox
# @port: serial port name as string (e.g. COM4, /dev/ttyUSB0)
# @with_error: if True -> Show Messageboxes on error, if False do not show errorsc
#
# Returns False or number of fox as integer
#
def ReadNumber(port, with_error=True):

    def TestResponse(allstr, with_error):
        if CheckRespond(allstr, with_error) != True:
            return [False, 0]
        fox_number = allstr[0]
        try:
            if int(fox_number) < settings.FOX_NUMBER_MIN or int(fox_number) > settings.FOX_NUMBER_MAX:
                if with_error:
                    utils.MessageBox("Fox returned wrong number: \"" + fox_number + "\". Number must be between " + settings.FOX_NUMBER_MIN + " and " + settings.FOX_NUMBER_MAX + ".")
                return [False, 0]
        except ValueError:
            if with_error:
                utils.MessageBox("Fox returned wrong number: \"" + str(fox_number) + "\". Number must be between " + str(settings.FOX_NUMBER_MIN) + " and " + str(settings.FOX_NUMBER_MAX) + ".")
            return [False, 0]
        int_fox_num = int(fox_number)
        return [True, int_fox_num]
    
    con = fox_serial.Open(port, with_error=False)
    if con == None:
       if with_error:
           utils.MessageBox("Cannot open COM-port " + str(port))
       return [False, 0]
   
    # this is necessary to clear the uart buffer if the welcome message is sent
    # -> sent one command and receive the answer
    # first time uart will receive binary zeros, second command is then correct

    fox_serial.ClearBuffer(con)

    fox_serial.Writeln(con, settings.COMMAND_GET_FOX_NUMBER)
    allstr = fox_serial.ReadToPrompt(con, False)

    ret = TestResponse(allstr, False)

    if ret[0] != True:
        fox_serial.ClearBuffer(con)
        fox_serial.ReadAll(con)
        fox_serial.Writeln(con, "exit")
        allstr3 = fox_serial.ReadToPrompt(con, False)

        #fox_serial.Writeln(con, "exit")
        #allstr3 = fox_serial.ReadToPrompt(con, False)
    
        fox_serial.Writeln(con, settings.COMMAND_GET_FOX_NUMBER)
        allstr = fox_serial.ReadToPrompt(con, False)

        ret = TestResponse(allstr, with_error)

    fox_serial.Close(con)

    return ret

def ReadCrystalFrequency(port, with_error=True):
    con = fox_serial.Open(port, with_error=False)
    if con == None:
        return [False, 0]
    fox_serial.Writeln(con, settings.COMMAND_GET_CRYSTAL_FREQUENCY)
    allstr = fox_serial.ReadToPrompt(con)
    fox_serial.Close(con)
    if CheckRespond(allstr, with_error) != True:
        return [False, 0]
    crystal_frequency = allstr[0]
    try:
        int_crystal_frequency = int(crystal_frequency)
    except ValueError:
        if with_error:
            utils.MessageBox("Fox returned wrong crystal frequency: \"" + crystal_frequency) 
        return [False, 0]
    return [True, int_crystal_frequency]

def SendCommand(con, command, parameter, with_error=True):
    # update changes here also to SyncTime
    fox_serial.Writeln(con, command + parameter)
    return True

def SyncTime(con, with_error=True):
    try:
        # write time
        current_time = datetime.datetime.now()
        current_time = current_time.replace(microsecond=0)
        con.write(settings.COMMAND_SET_TIME + current_time.time().isoformat())
        con.write("\r\n".encode('utf-8'))
    except serial.SerialException:
        utils.MessageBox("Could not write to Serial Port")
    allstr = fox_serial.ReadToPrompt(con)
    if (allstr[len(allstr) - 2].find(settings.FOX_PROMPT) < 0 or allstr[len(allstr) - 3].find(settings.OK_PROMPT) < 0) and \
        (allstr[len(allstr) - 1].find(settings.FOX_PROMPT) < 0 or allstr[len(allstr) - 2].find(settings.OK_PROMPT) < 0):
        if with_error:
            utils.MessageBox("Fox does not respond correctly")
        return False
    
    try:
        # write date
        current_time = datetime.datetime.now()
        con.write(settings.COMMAND_SET_DATE + current_time.date().isoformat())
        con.write("\r\n".encode('utf-8'))
    except serial.SerialException:
        utils.MessageBox("Could not write to Serial Port")
    allstr = fox_serial.ReadToPrompt(con)
    if (allstr[len(allstr) - 2].find(settings.FOX_PROMPT) < 0 or allstr[len(allstr) - 3].find(settings.OK_PROMPT) < 0) and \
        (allstr[len(allstr) - 1].find(settings.FOX_PROMPT) < 0 or allstr[len(allstr) - 2].find(settings.OK_PROMPT) < 0):
        if with_error:
            utils.MessageBox("Fox does not respond correctly")
        return False
    return True

