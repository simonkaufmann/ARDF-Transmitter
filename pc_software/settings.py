#!/usr/bin/python
# -*- coding: utf-8 -*-

#
# settings.py - Definitions and Settings for ARDF transmitter firmware
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

import wx

VERSION = "1.00"

APP_NAME = "ardf_transmitter_firmware"

DEBUG = False

BAUDRATE = 38400

FOX_NUMBER_DEMO = 0
FOX_NUMBER_MIN = 0
FOX_NUMBER_FIRST = 1
FOX_NUMBER_MAX = 5

USER_MANUAL_RELATIVE_PATH = "user_manual.pdf"
ICON_RELATIVE_PATH = "oevsv_logo_med.ico"

# number for the widget-array
ARRAY_FOX_NUMBER_DEMO = 5
ARRAY_FOX_NUMBER_MIN = 0
ARRAY_FOX_NUMBER_FIRST = 0
ARRAY_FOX_NUMBER_MAX = 5

TAG_ID_MAX = pow(2, 16) - 1
TAG_ID_MIN = 1

UART_TIMEOUT = 0.5

DEMO_FOX_STRING = "Demo"

FOX_PROMPT = "ARDF Transmitter#"
OK_PROMPT = "OK"

PANEL_COLOR_DISABLED = "GRAY"
PANEL_COLOR_ENABLED = "GREEN"

ID_HELP_ABOUT = wx.NewId() # for menus -> refer to: http://wiki.wxpython.org/WorkingWithMenus
ID_HELP_USER_MANUAL = wx.NewId()
ID_SETTINGS_SEARCH_FOXES_START = wx.NewId()

COMMAND_SET_CALL_SIGN = "set call sign "
COMMAND_SET_TRANSMIT_MINUTE = "set transmit minute "
COMMAND_SET_FOX_MAX = "set fox max "
COMMAND_SET_START_TIME = "set start time "
COMMAND_SET_START_DATE = "set start date "
COMMAND_SET_STOP_TIME = "set stop time "
COMMAND_SET_STOP_DATE = "set stop date "
COMMAND_SET_FREQUENCY = "set frequency "
COMMAND_SET_CRYSTAL_FREQUENCY = "set crystal frequency "
COMMAND_GET_CRYSTAL_FREQUENCY = "get crystal frequency "
COMMAND_SET_FOX_NUMBER = "set fox number "
COMMAND_GET_FOX_NUMBER = "get fox number "
COMMAND_SET_DATE = "set date "
COMMAND_SET_TIME = "set time "
COMMAND_SET_WPM = "set wpm "
COMMAND_SET_MODULATION = "set modulation "
COMMAND_SET_MORSING = "set morsing "
COMMAND_SET_AMPLITUDE = "set amplitude "
COMMAND_RESET_HISTORY = "reset history "
COMMAND_SET_ID = "set id "
COMMAND_SET_RELOAD = "set reload "
COMMAND_RELOAD = "reload "
COMMAND_SET_SECRET = "set secret "

STRING_TRUE = "True"
STRING_FALSE = "False"
STRING_ON = "on"
STRING_OFF = "off"

NO_FOX_MESSAGE = "No Fox"

SEARCH_FOXES_START_KEY = "search_foxes_at_start"

MESSAGE_NEW_TAG = "--- NEW TAG 0x55005500 ---"
MESSAGE_END_TAG = "--- END TAG 0x55005500 ---"
MESSAGE_ERROR_TAG = "ERROR TAG 0x55005500"
TAG_TAG_ID = "Tag ID: "
TAG_FOX = ["Fox 1 Secret: ", "Fox 2 Secret: ", "Fox 3 Secret: ", "Fox 4 Secret: ", "Fox 5 Secret: "]
TAG_TIMESTAMP = "Timestamp: "

TAG_HISTORY_ENTRIES_PER_FOX = 12
