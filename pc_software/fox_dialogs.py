#!/usr/bin/python
# -*- coding: utf-8 -*-

#
#  fox_dialogs.py - Some dialogs used in ARDF transmitter firmware
#  Copyright (C) 2016  Simon Kaufmann, HeKa
# 
#  This file is part of ADRF transmitter firmware.
# 
#  ADRF Transmitter is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
# 
#  ADRF transmitter firmware is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with ADRF Transmitter.  
#  If not, see <http://www.gnu.org/licenses/>.
#

#
# system imports
#
import time
import wx
import wx.lib.masked.timectrl
import serial
import serial.tools.list_ports
from lxml import etree as ET
import datetime

#
# own imports
#
from main import *

class SetFoxNumberDialog(wx.Dialog):

    def __init__(self, parent):
        wx.Dialog.__init__(self, parent, title="Set Fox Number")

        box_sizer = wx.BoxSizer(wx.VERTICAL)
        flex_sizer = wx.FlexGridSizer(rows=3, cols=0, hgap=20, vgap=3)
        flex_sizer.AddGrowableCol(0, 1) 
        flex_sizer.AddGrowableCol(1, 1)
        box_sizer.Add(flex_sizer, proportion=1, border=10, flag=wx.ALL|wx.EXPAND)

        flex_sizer.Add(wx.StaticText(self, label="COM Port: "), flag=wx.ALIGN_CENTER_VERTICAL)
        serial_ports_string = fox_serial.GetPorts()
        self.combobox_com = wx.ComboBox(self, value=serial_ports_string[0], choices=serial_ports_string)
        flex_sizer.Add(self.combobox_com, flag=wx.ALIGN_CENTER_VERTICAL|wx.EXPAND)
        
        flex_sizer.Add(wx.StaticText(self, label="Fox Number: "), flag=wx.ALIGN_CENTER_VERTICAL)
        fox_numbers = ["1", "2", "3", "4", "5", settings.DEMO_FOX_STRING]
        self.combobox_fox_number = wx.ComboBox(self, value=fox_numbers[0], choices=fox_numbers)
        flex_sizer.Add(self.combobox_fox_number, flag=wx.ALIGN_CENTER_VERTICAL|wx.EXPAND)
        
        self.checkbox_blink = wx.CheckBox(self, label="Blinking")
        button_read_number = wx.Button(self, label="Read Number")
        box_sizer_horizontal = wx.BoxSizer(wx.HORIZONTAL)
        box_sizer_horizontal.Add(self.checkbox_blink, flag=wx.ALIGN_CENTER_VERTICAL|wx.EXPAND)
        box_sizer_horizontal.Add(button_read_number, flag=wx.ALIGN_CENTER_VERTICAL|wx.EXPAND)
        box_sizer.Add(box_sizer_horizontal, border=10, flag=wx.ALL)

        button_dialog = self.CreateButtonSizer(wx.OK|wx.CANCEL)
        box_sizer.Add(button_dialog, border=10, flag=wx.BOTTOM|wx.EXPAND)

        self.SetBackgroundColour(wx.NullColour)

        self.Bind(wx.EVT_CHECKBOX, self.TestBlink, self.checkbox_blink)
        self.Bind(wx.EVT_BUTTON, self.ReadNumber, button_read_number)

        self.ReadNumber(None, with_error=False)

        self.SetSizerAndFit(box_sizer)
        self.Center()

    def TestBlink(self, event):
        if self.checkbox_blink.GetValue() == False:
            self.SetBlink(False)
        else:
            self.SetBlink(True)
        return

    def SetBlink(self, mode, with_error=True):
        port = self.combobox_com.GetValue()
        con = fox_serial.Open(port, with_error)
        if con != None:
            if mode == False:
                fox_serial.Writeln(con, "set blinking off")
            else:
                fox_serial.Writeln(con, "set blinking on")
            allstr = fox_serial.ReadToPrompt(con)
            fox.CheckRespond(allstr)
            fox_serial.Close(con)
        return

    def ReadNumber(self, event, with_error=True):
        port = self.combobox_com.GetValue()
        fox_number = fox.ReadNumber(port, with_error=with_error)
        if fox_number[0] == False:
            return 
        if fox_number[1] == settings.FOX_NUMBER_DEMO:
            fox_number[1] = "Demo"
        else:
            fox_number[1] = str(fox_number[1])
        self.combobox_fox_number.SetValue(fox_number[1])
        return 

    def SetNumber(self, event):
        foxnum = self.combobox_fox_number.GetValue()
        if foxnum == settings.DEMO_FOX_STRING:
            foxnum = str(settings.FOX_NUMBER_DEMO)
        sendstr = (settings.COMMAND_SET_FOX_NUMBER + foxnum).encode('utf-8')
        port = self.combobox_com.GetValue()
        con = fox_serial.Open(port)
        if con != None:
            fox_serial.Writeln(con, sendstr)
            allstr = fox_serial.ReadToPrompt(con)
            fox.CheckRespond(allstr)
            fox_serial.Close(con)
        return

class CalibrateCrystalDialog(wx.Dialog):

    def __init__(self, parent, com_init, com_act_init):
        wx.Dialog.__init__(self, parent, title="Calibrate Oscillator Frequency")

        self.com = com_init
        self.com_act = com_act_init

        box_sizer = wx.BoxSizer(wx.VERTICAL)
        flex_sizer = wx.FlexGridSizer(rows=3, cols=3, hgap=8, vgap=3)
        box_sizer.Add(flex_sizer, proportion=1, border=10, flag=wx.ALL|wx.EXPAND)

        flex_sizer.Add(wx.StaticText(self, label="Fox Number: "), flag=wx.ALIGN_CENTER_VERTICAL)

        fox_number_string = []
        for i in range(0, len(self.com_act) - 1):
            if self.com_act[i] == True:
                fox = str(i)
                if i == settings.FOX_NUMBER_DEMO:
                    fox = "Demo"
                fox_number_string.append(fox)

        if len(fox_number_string) == 0:
            fox_number_string.append(settings.NO_FOX_MESSAGE)

        self.combobox_fox_number = wx.ComboBox(self, value=fox_number_string[0], choices=fox_number_string, style=wx.CB_READONLY)
        flex_sizer.Add(self.combobox_fox_number, flag=wx.ALIGN_CENTER_VERTICAL|wx.EXPAND)
        flex_sizer.Add(wx.StaticText(self, label=""), flag=wx.ALIGN_CENTER_VERTICAL)

        flex_sizer.Add(wx.StaticText(self, label="Oscillator Frequency: "), flag=wx.ALIGN_CENTER_VERTICAL)
        self.text_crystal_frequency = wx.TextCtrl(self, value="25.0")
        flex_sizer.Add(self.text_crystal_frequency, flag=wx.ALIGN_CENTER_VERTICAL|wx.EXPAND)
        flex_sizer.Add(wx.StaticText(self, label="MHz"), flag=wx.ALIGN_CENTER_VERTICAL)

        self.checkbox_blink = wx.CheckBox(self, label="Blinking")
        button_read_frequency = wx.Button(self, label="Read Frequency")
        box_sizer_horizontal = wx.BoxSizer(wx.HORIZONTAL)
        box_sizer_horizontal.Add(self.checkbox_blink, flag=wx.ALIGN_CENTER_VERTICAL|wx.EXPAND)
        box_sizer_horizontal.Add(button_read_frequency, flag=wx.ALIGN_CENTER_VERTICAL|wx.EXPAND)
        box_sizer.Add(box_sizer_horizontal, border=10, flag=wx.ALL)

        button_dialog = self.CreateButtonSizer(wx.OK|wx.CANCEL)
        box_sizer.Add(button_dialog, border=10, flag=wx.BOTTOM|wx.EXPAND)

        self.SetBackgroundColour(wx.NullColour)

        self.Bind(wx.EVT_CHECKBOX, self.TestBlink, self.checkbox_blink)
        self.Bind(wx.EVT_BUTTON, self.ReadCrystalFrequency, button_read_frequency)

        self.ReadCrystalFrequency(None, with_error=False)

        self.SetSizerAndFit(box_sizer)
        self.Center()

    def TestBlink(self, event):
        if self.checkbox_blink.GetValue() == False:
            self.SetBlink(False)
        else:
            self.SetBlink(True)
        return

    def SetBlink(self, mode, with_error=True):
        ret = self.GetPort(with_error)
        if ret[0] == True:
            port = ret[1]
        else:
            return
        con = fox_serial.Open(port, with_error)
        if con != None:
            if mode == False:
                fox_serial.Writeln(con, "set blinking off")
            else:
                fox_serial.Writeln(con, "set blinking on")
            allstr = fox_serial.ReadToPrompt(con)
            fox.CheckRespond(allstr)
            fox_serial.Close(con)
        return

    def ReadCrystalFrequency(self, event, with_error=True):
        ret = self.GetPort(with_error)
        if ret[0] == True:
            port = ret[1]
        else:
            return
        crystal_frequency = fox.ReadCrystalFrequency(port, with_error=with_error)
        if crystal_frequency[0] == False:
            return 
        self.text_crystal_frequency.SetValue(str(float(crystal_frequency[1])/1000000))
        return 

    def SetCrystalFrequency(self, event):
        try:
            freq = int(float(self.text_crystal_frequency.GetValue()) * 1000000)
        except ValueError: 
            utils.MessageBox("Crystal frequency must be floating number or integer (e.g. 3.5)")
            return False
        ret = self.GetPort()
        if ret[0] == True:
            port = ret[1]
        else:
            return True # return true because otherwise dialog is shown again and again
        con = fox_serial.Open(port)
        sendstr = settings.COMMAND_SET_CRYSTAL_FREQUENCY + str(freq)
        if con != None:
            fox_serial.Writeln(con, sendstr)
            allstr = fox_serial.ReadToPrompt(con)
            fox.CheckRespond(allstr)
            fox_serial.Close(con)
        return True

    def GetPort(self, with_error=True):
        fox_number = self.combobox_fox_number.GetValue()
        if fox_number == settings.DEMO_FOX_STRING:
            fox_number = 0
        elif fox_number == settings.NO_FOX_MESSAGE:
            if with_error == True:
                utils.MessageBox("No fox connected")
            return [False, ""]
        else:
            try:
                fox_number = int(fox_number)
            except ValueError:
                if with_error == True:
                    utils.MessageBox("Invalid Fox Number: " + fox_number)
                return [False, ""]
        port = self.com[fox_number]
        return [True, port]

class UserDialog(wx.Dialog):

    def __init__(self, parent, tagid, name=""):
        wx.Dialog.__init__(self, parent, title="User")

        box_sizer = wx.BoxSizer(wx.VERTICAL)

        flex_grid_sizer = wx.FlexGridSizer(rows=2, cols=0, hgap=20, vgap=3)
        flex_grid_sizer.AddGrowableCol(0, 1)
        flex_grid_sizer.AddGrowableCol(1, 2)

        self.text_name = wx.TextCtrl(self, value=str(name))
        flex_grid_sizer.Add(wx.StaticText(self, label="Name"))
        flex_grid_sizer.Add(self.text_name, flag=wx.EXPAND)
        self.text_tag_id = wx.TextCtrl(self, value=str(tagid))
        flex_grid_sizer.Add(wx.StaticText(self, label="Tag ID"))
        flex_grid_sizer.Add(self.text_tag_id, flag=wx.EXPAND)

        button_sizer = self.CreateButtonSizer(wx.OK|wx.CANCEL)
        box_sizer.Add(flex_grid_sizer, proportion=1, border=10, flag=wx.ALL|wx.EXPAND)
        box_sizer.Add(button_sizer, border=10, flag=wx.ALL)

        self.SetSizerAndFit(box_sizer)

    def GetName(self):
        return self.text_name.GetValue()

    def GetTagId(self):
        return self.text_tag_id.GetValue()

class SetTimeDialog(wx.Dialog):

    def __init__(self, parent):
        wx.Dialog.__init__(self, parent, title="Set Finish Time")

        box_sizer = wx.BoxSizer(wx.VERTICAL)

        box_sizer_horizontal = wx.BoxSizer(wx.HORIZONTAL)

        box_sizer_horizontal.Add(wx.StaticText(self, label="Finish Time:"), border=10, flag=wx.ALL|wx.ALIGN_CENTER_VERTICAL)
        self.finish_time_picker = wx.lib.masked.TimeCtrl(self, -1, fmt24hr=True)
        box_sizer_horizontal.Add(self.finish_time_picker, border=10, flag=wx.ALL|wx.ALIGN_CENTER_VERTICAL)

        box_sizer.Add(box_sizer_horizontal)
        button_sizer = self._CreateButtonSizer(wx.OK|wx.CANCEL)
        box_sizer.Add(button_sizer, border=10, flag=wx.ALL)

        self.SetSizerAndFit(box_sizer)

    def GetFinishingTime(self):
        return self.finish_time_picker.GetValue(as_wxDateTime=True).FormatISOTime()

