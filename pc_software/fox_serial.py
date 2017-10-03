#!/usr/bin/python
# -*- coding: utf-8 -*-

#
# fox_serial.py - Serial communication helper functions
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

import serial
import serial.tools.list_ports

import settings
import utils

def GetPorts():
    serial_ports = serial.tools.list_ports.comports()
    serial_ports_string = []
    for i in serial_ports:
        serial_ports_string.append(i.device)
    if len(serial_ports_string) == 0:
        serial_ports_string.append("")
    return serial_ports_string

# read lines until timeout is reached
def ReadAll(con, with_error=True):
    retstr = "a"
    allstr = []
    try:
        while retstr != "":
            retstr = con.readline()
            retstr2 = retstr[:retstr.find("\r")]
            allstr.append(retstr2)
    except serial.SerialException:
        if with_error:
            utils.MessageBox("Could not read from Serial Port")
    return allstr

def ClearBuffer(con):
    con.flushInput()

# read lines until the fox prompt is found
def ReadToPrompt(con, with_error=True):
    retstr = "a"
    allstr = []
    try:
        while retstr != "" and retstr.find(settings.FOX_PROMPT) < 0:
            retstr = con.readline()
            retstr2 = retstr[:retstr.find("\r")]
            allstr.append(retstr2)
    except serial.SerialException:
        if with_error:
            utils.MessageBox("Could not read from Serial Port")
    return allstr

# read lines until the end tag prompt is found
def ReadToEndTag(con, with_error=True):
    retstr = "a"
    allstr = []
    try:
        while retstr != "" and retstr.find(settings.MESSAGE_END_TAG) < 0 and retstr.find(settings.MESSAGE_ERROR_TAG) < 0:
            retstr = con.readline()
            retstr2 = retstr[:retstr.find("\r")]
            allstr.append(retstr2)
    except serial.SerialException:
        if with_error:
            utils.MessageBox("Could not read from Serial Port")
    return allstr


def Open(port, with_error=True):
    con = None
    try:
        con = serial.Serial(port, settings.BAUDRATE, timeout=settings.UART_TIMEOUT)
    except serial.SerialException:
        if with_error:
            utils.MessageBox("Could not open Serial Port " + port)
    return con

def Writeln(con, string, with_error=False):
    # Update Changes also to fox.SyncTime!
    try:
        con.write(string.encode('utf-8'))
        con.write("\r\n".encode('utf-8'))
        return True
    except serial.SerialException:
        if with_error:
            utils.MessageBox("Could not write to Serial Port")
        return False

def Readln(con, with_error=True):
    retstr = ""
    try:
        retstr = con.readline()
        retstr = retstr[:retstr.find("\r")]
    except serial.SerialException:
        if with_error:
            utils.MessageBox("Could not read from Serial Port")
    return retstr
        

def Close(con):
    try:
        con.close()
    except serial.SerialException:
        utils.MessageBox("Serial Port Error")

