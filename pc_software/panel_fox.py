#!/usr/bin/python
# -*- coding: utf-8 -*-

#
# panel_fox.py - code for the fox-tab in the software
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
# system imports:
#
import time
import wx
import wx.lib.masked.timectrl
from wx.lib.agw import ultimatelistctrl as ULC
from lxml import etree as ET
import random

#
# own imports
#
import settings
import utils
import fox_serial
import fox
import fox_dialogs

CHECK_FOX_PROGRESS_STRING = "Check all COM-ports for connected foxes"

class PanelFox(wx.Panel):
    
    def __init__(self, parent, frame):
        wx.Panel.__init__(self, parent)
        self.frame = frame # frame needed for statusbar!

        self.SetDoubleBuffered(True) # no flickering in windows

        box_sizer = wx.BoxSizer(wx.VERTICAL)

        flex_sizer_frequency_timing = wx.FlexGridSizer(rows=6, cols=4, vgap=3, hgap=10)

        self.fox_secret = []
        for i in range(0, 5):
            self.fox_secret.append(random.randint(1, 65535))

        # frequency 
        flex_sizer_frequency_timing.Add(wx.StaticText(self, label="Frequency:"), flag=wx.ALIGN_CENTER_VERTICAL)
        self.text_frequency = wx.TextCtrl(self, value="3.5")
        flex_sizer_frequency_timing.Add(self.text_frequency, flag=wx.ALIGN_CENTER_VERTICAL|wx.EXPAND)
        flex_sizer_frequency_timing.Add(wx.StaticText(self, label="MHz"), flag=wx.ALIGN_CENTER_VERTICAL)
        self.checkbox_modulation = wx.CheckBox(self, label="Amplitude Modulation")
        flex_sizer_frequency_timing.Add(self.checkbox_modulation, flag=wx.ALIGN_CENTER_VERTICAL)
        flex_sizer_frequency_timing.Add(wx.StaticText(self, label="Frequency Demo Fox:"), flag=wx.ALIGN_CENTER_VERTICAL)
        self.text_demo_frequency_freq = wx.TextCtrl(self, value="3.5")
        flex_sizer_frequency_timing.Add(self.text_demo_frequency_freq, flag=wx.ALIGN_CENTER_VERTICAL|wx.EXPAND)
        flex_sizer_frequency_timing.Add(wx.StaticText(self, label="MHz"), flag=wx.ALIGN_CENTER_VERTICAL)
        flex_sizer_frequency_timing.Add(wx.StaticText(self))

        # start time 
        flex_sizer_frequency_timing.Add(wx.StaticText(self, label="Start Time:"), flag=wx.ALIGN_CENTER_VERTICAL)
        self.start_time_picker = wx.lib.masked.TimeCtrl(self, -1, fmt24hr=True)
        flex_sizer_frequency_timing.Add(self.start_time_picker, flag=wx.EXPAND)
        flex_sizer_frequency_timing.Add(wx.StaticText(self, label=""))
        self.start_date_picker = wx.DatePickerCtrl(self)
        flex_sizer_frequency_timing.Add(self.start_date_picker)

        # stop time
        flex_sizer_frequency_timing.Add(wx.StaticText(self, label="Stop Time:"), flag=wx.ALIGN_CENTER_VERTICAL)
        self.stop_time_picker = wx.lib.masked.TimeCtrl(self, -1, fmt24hr=True)
        flex_sizer_frequency_timing.Add(self.stop_time_picker, flag=wx.EXPAND)
        flex_sizer_frequency_timing.Add(wx.StaticText(self, label=""))
        self.stop_date_picker = wx.DatePickerCtrl(self)
        flex_sizer_frequency_timing.Add(self.stop_date_picker)

        # morse speed
        flex_sizer_frequency_timing.Add(wx.StaticText(self, label="Morse Speed:"), flag=wx.ALIGN_CENTER_VERTICAL)
        self.text_morse_speed = wx.TextCtrl(self, value="10")
        flex_sizer_frequency_timing.Add(self.text_morse_speed, flag=wx.EXPAND)
        flex_sizer_frequency_timing.Add(wx.StaticText(self, label="WPM"), flag=wx.ALIGN_CENTER_VERTICAL)
        flex_sizer_frequency_timing.Add(wx.StaticText(self))

        # amplitude
        flex_sizer_frequency_timing.Add(wx.StaticText(self, label="Amplitude:"), flag=wx.ALIGN_CENTER_VERTICAL)
        self.text_amplitude = wx.TextCtrl(self, value="100")
        flex_sizer_frequency_timing.Add(self.text_amplitude, flag=wx.EXPAND)
        flex_sizer_frequency_timing.Add(wx.StaticText(self, label="%"), flag=wx.ALIGN_CENTER_VERTICAL)
        flex_sizer_frequency_timing.Add(wx.StaticText(self))

        box_sizer.Add(flex_sizer_frequency_timing, border=10, flag=wx.ALL)

        # single fox settings
        flex_sizer_fox = wx.FlexGridSizer(rows=8, cols=0, vgap=3, hgap=20)
        flex_sizer_fox.AddGrowableCol(0, 1)
        flex_sizer_fox.AddGrowableCol(1, 1)
        flex_sizer_fox.AddGrowableCol(2, 1)
        flex_sizer_fox.AddGrowableCol(3, 1)
        flex_sizer_fox.AddGrowableCol(4, 1)
        box_sizer.Add(flex_sizer_fox, proportion=1, border=10, flag=wx.ALL|wx.EXPAND)

        flex_sizer_fox.Add(wx.StaticText(self, label="Fox:"), border=3, flag=wx.BOTTOM)
        flex_sizer_fox.Add(wx.StaticText(self, label="Con:"), border=3, flag=wx.BOTTOM)
        flex_sizer_fox.Add(wx.StaticText(self, label="Call Sign:"), border=3, flag=wx.BOTTOM)
        flex_sizer_fox.Add(wx.StaticText(self, label="Transmit Minute:"), border=3, flag=wx.BOTTOM)
        flex_sizer_fox.Add(wx.StaticText(self))

        self.checkbox_fox = []
        self.combobox_call = []
        combobox_call_options = ["MOE -- --- *", "MOI -- --- **", "MOS -- --- ***", "MOH -- --- ****", "MO5 -- --- ******", "MO -- ---"]
        combobox_minute_options = ["0", "1", "2", "3", "4"]
        self.combobox_minute = []
        self.panel_connected = []
        self.button_program_fox = []
        fox_name = []
        for i in range(0, 6):
            if i == 5:
                fox_name.append("Demo Fox")
            else:
                fox_name.append("Fox " + str(i + 1))
            self.checkbox_fox.append(wx.CheckBox(self, label=fox_name[i]))
            self.checkbox_fox[i].SetValue(True)
            self.Bind(wx.EVT_CHECKBOX, self.CheckboxFoxChanged, self.checkbox_fox[i])
            self.combobox_call.append(wx.ComboBox(self, value=combobox_call_options[i], choices=combobox_call_options, style=wx.CB_READONLY))
            flex_sizer_fox.Add(self.checkbox_fox[i], flag=wx.ALIGN_CENTER_VERTICAL)
            self.panel_connected.append(wx.Panel(self, size=(18, 18)))
            self.panel_connected[i].SetBackgroundColour(settings.PANEL_COLOR_DISABLED)
            self.panel_connected[i].Refresh()
            flex_sizer_fox.Add(self.panel_connected[i], flag=wx.ALIGN_CENTER_VERTICAL)
            flex_sizer_fox.Add(self.combobox_call[i], flag=wx.ALIGN_CENTER_VERTICAL)
            if i < 5:
                self.combobox_minute.append(wx.ComboBox(self, value=combobox_minute_options[i], choices=combobox_minute_options, style=wx.CB_READONLY))
                flex_sizer_fox.Add(self.combobox_minute[i], flag=wx.ALIGN_CENTER_VERTICAL)
            else:
                flex_sizer_fox.Add(wx.StaticText(self), flag=wx.ALIGN_CENTER_VERTICAL) # add empty static text
            self.button_program_fox.append(wx.Button(self, label="Program " + fox_name[i]))
            self.Bind(wx.EVT_BUTTON, self.ProgramFox, self.button_program_fox[i])

            flex_sizer_fox.Add(self.button_program_fox[i], flag=wx.EXPAND|wx.ALIGN_CENTER_VERTICAL)

        # repetition interval
        box_sizer_repetition = wx.BoxSizer(wx.HORIZONTAL)
        box_sizer.Add(box_sizer_repetition, border=10, flag=wx.LEFT|wx.BOTTOM)

        box_sizer_repetition.Add(wx.StaticText(self, label="Fox Repetition Interval:"), flag=wx.ALIGN_CENTER_VERTICAL)
        self.combobox_repetition_options = ["1", "2", "3", "4", "5"]
        self.combobox_repetition = wx.ComboBox(self, value=self.combobox_repetition_options[len(self.combobox_repetition_options) - 1], choices=self.combobox_repetition_options, style=wx.CB_READONLY)
        box_sizer_repetition.Add(self.combobox_repetition, border=5, flag=wx.LEFT|wx.ALIGN_CENTER_VERTICAL)
        box_sizer_repetition.Add(wx.StaticText(self, label="minutes"), border=5, flag=wx.LEFT|wx.ALIGN_CENTER_VERTICAL)

        # program buttons
        box_sizer_program = wx.BoxSizer(wx.HORIZONTAL)
        box_sizer.Add(box_sizer_program, border=10, flag=wx.LEFT|wx.BOTTOM)

        #button_auto_detect = wx.Button(self, label="Autodetect Foxes")
        button_search_foxes = wx.Button(self, label="Refresh Foxes")
        button_set_fox_number = wx.Button(self, label="Set Fox Number")
        button_program_all = wx.Button(self, label="   Program All   ")
        button_calibrate_oscillator = wx.Button(self, label="Calibrate Crystal Frequency")
        box_sizer_program.Add(button_search_foxes, border=3, flag=wx.LEFT|wx.ALIGN_CENTER_VERTICAL)
        box_sizer_program.Add(button_set_fox_number, border=3, flag=wx.LEFT|wx.ALIGN_CENTER_VERTICAL)
        box_sizer_program.Add(button_calibrate_oscillator, border=3, flag=wx.LEFT|wx.ALIGN_CENTER_VERTICAL)
        box_sizer_program.Add(button_program_all, border=20, flag=wx.LEFT|wx.ALIGN_CENTER_VERTICAL)

        self.Bind(wx.EVT_BUTTON, self.TestFoxes, button_search_foxes)
        self.Bind(wx.EVT_BUTTON, self.CalibrateCrystal, button_calibrate_oscillator)

        self.Bind(wx.EVT_BUTTON, self.SetFoxNumber, button_set_fox_number)
        self.Bind(wx.EVT_BUTTON, self.ProgramAll, button_program_all)
        
        self.SetSizer(box_sizer)

        self.com = []
        self.com_act = []

    def TestFoxes(self, event):
        self.com = []
        ser = fox_serial.GetPorts()
        nums = []
        self.com_act = []
        for i in range(settings.FOX_NUMBER_MIN, settings.FOX_NUMBER_MAX + 1):
            nums.append(False)
            self.com.append("")
            self.com_act.append(False)

        progress_dialog = wx.ProgressDialog("Search Connected Foxes", CHECK_FOX_PROGRESS_STRING, maximum=(len(ser))*10+1, parent=self, style=wx.PD_APP_MODAL|wx.PD_AUTO_HIDE) #parent=self)
        progress_dialog.SetInitialSize()

        for i in range(0, len(ser)):
            progress_dialog.Update(i*10, CHECK_FOX_PROGRESS_STRING + ": Checking " + str(ser[i]))
            progress_dialog.SetInitialSize();
            progress_dialog.Update(i*10, CHECK_FOX_PROGRESS_STRING + ": Checking " + str(ser[i]))
            fox_number = fox.ReadNumber(ser[i], False)
            if fox_number[0] != False:
                nums[int(fox_number[1])] = True
                self.com[int(fox_number[1])] = ser[i]
                self.com_act[int(fox_number[1])] = True
        for i in range(0, len(nums)):
            if i == 0:
                b = settings.FOX_NUMBER_MAX # to get correct index for panel-array
            else:
                b = i - 1
            if nums[i] == True:
                self.panel_connected[b].SetBackgroundColour(settings.PANEL_COLOR_ENABLED)
                self.panel_connected[b].Refresh()
            else:
                self.panel_connected[b].SetBackgroundColour(settings.PANEL_COLOR_DISABLED)
                self.panel_connected[b].Refresh()
        progress_dialog.Update(len(ser)*10+1, "Finished")
        progress_dialog.Destroy()

        try:
            self.panel_user.RefreshFoxCheckbox()
        except:
            pass
        try:
            self.panel_result.RefreshFoxCheckbox()
        except:
            pass
        return

    def SetFoxNumber(self, event):
        self.foxnumberdialog = fox_dialogs.SetFoxNumberDialog(self)
        ok = self.foxnumberdialog.ShowModal()
        self.foxnumberdialog.SetBlink(False, False) 
        if ok == wx.ID_OK:
            self.foxnumberdialog.SetNumber(None)
            self.TestFoxes(None)
            # that no error message is shown if Dialog ist closed and wrong
            # uart is set!

    def CalibrateCrystal(self, event):
        self.calibratedialog = fox_dialogs.CalibrateCrystalDialog(self, self.com, self.com_act)
        ok = self.calibratedialog.ShowModal()
        while True:
            if ok == wx.ID_OK:
                if self.calibratedialog.SetCrystalFrequency(None) == True:
                    break
                # that no error message is shown if Dialog ist closed and wrong
                # uart is set!
                ok = self.calibratedialog.ShowModal()
            else:
                break
        self.calibratedialog.SetBlink(False, False)
        self.calibratedialog.Destroy()
        return

    def GetSecret(self):
        return self.fox_secret

    def GetStartTime(self):
        return self.start_time_picker.GetValue(as_wxDateTime=True)

    def ProgramFox(self, event):
        
        # i saves fox number in ARRAY_FOX-notation (used for the widget-arrays)
        # number saves fox number in FOX_NUMBER-notation (used in mikrocontroller software)

        sender = event.GetEventObject()
        for i in range (0, 6):
            if sender == self.button_program_fox[i]:
                number = i
                break
        self.ProgramFoxNumber(number)

    def ProgramFoxNumber(self, number):
        # number is from ARRAY_FOX_NUMBER!

        j = -1 # just a value that is not between FOX_NUMBER_MIN and FOX_NUMBER_MAX

        if number == settings.ARRAY_FOX_NUMBER_DEMO:
            transmit_minute = "0"
            secret = "1"
            repetition = "1"
            try:
                frequency = str(int(float(self.text_demo_frequency_freq.GetValue()) * 1000000))
            except ValueError:
                utils.MessageBox("Frequency does not seem to be a float-number (e.g. 3.5)")
                return False
            try:
                amplitude_int = int(self.text_amplitude.GetValue())
                amplitude = str(amplitude_int)
                if amplitude_int < 0 or amplitude_int > 100:
                    utils.MessageBox("Amplitude must be between 0 and 100")
            except ValueError:
                utils.MessageBox("Amplitude does not seem to be an integer")
                return False
            try:
                morse_int = int(self.text_morse_speed.GetValue())
                morse = str(morse_int)
            except ValueError:
                utils.MessageBox("Morse-speed does not seem to be an integer")
                return False
        else:
            transmit_minute = self.combobox_minute[number].GetValue()
            secret = str(self.fox_secret[number])
            repetition = self.combobox_repetition.GetValue()
            try:
                frequency = str(int(float(self.text_frequency.GetValue()) * 1000000))
            except ValueError:
                utils.MessageBox("Frequency does not seem to be a float-number (e.g. 3.5)")
                return False

        if number == settings.ARRAY_FOX_NUMBER_DEMO:
            number = settings.FOX_NUMBER_DEMO
        else:
            number = number + 1

        fox_name = str(number)
        if number == settings.FOX_NUMBER_DEMO:
            fox_name = "Demo"

        if self.com[number] != "":
            ret = fox.ReadNumber(self.com[number])
            if ret[0] == True:
                j = ret[1]

        progress_dialog = wx.ProgressDialog("Program Fox", "Check fox number", maximum=31, parent=None, style=wx.PD_APP_MODAL)
        progress_dialog.SetInitialSize()
        self.frame.Status("Program Fox " + str(number) + ": Check fox number")
        progress_dialog.Update(0, "Program Fox " + str(number) + ": Check fox number")

        if j != number:
            self.TestFoxes(None)
            if self.com[number] != "":
                temp = fox.ReadNumber(self.com[number])
                if temp[0] == True:
                    j = temp[1]
            if j != number or self.com[number] == "":
                progress_dialog.Destroy()
                self.frame.Status("Program Fox " + str(number) + ": Failed")
                if number != settings.FOX_NUMBER_DEMO:
                    utils.MessageBox("Fox " + str(number) + " is not connected")
                else:
                    utils.MessageBox("Demo-Fox is not connected")
                return False
        
        con = fox_serial.Open(self.com[number])
        if con == None:
            progress_dialog.Destroy()
            return False

        if self.checkbox_modulation.GetValue():
            modulation_string = settings.STRING_ON
        else:
            modulation_string = settings.STRING_OFF

        commands = [settings.COMMAND_SET_RELOAD,
                    settings.COMMAND_SET_CALL_SIGN, 
                    settings.COMMAND_SET_FOX_MAX,
                    settings.COMMAND_SET_TRANSMIT_MINUTE,
                    settings.COMMAND_SET_FREQUENCY,
                    settings.COMMAND_SET_START_DATE,
                    settings.COMMAND_SET_START_TIME,
                    settings.COMMAND_SET_STOP_DATE, 
                    settings.COMMAND_SET_STOP_TIME,
                    settings.COMMAND_SET_WPM,
                    settings.COMMAND_SET_MODULATION,
                    settings.COMMAND_SET_MORSING,
                    settings.COMMAND_SET_AMPLITUDE,
                    settings.COMMAND_RESET_HISTORY,
                    settings.COMMAND_SET_SECRET]

        parameters = [settings.STRING_OFF,
                      self.combobox_call[number].GetValue()[:self.combobox_call[number].GetValue().find(" ")],  # SET_CALL_SIGN
                      repetition, # SET_FOX_MAX
                      transmit_minute, # SET_TRANSMIT_MINUTE
                      frequency, # SET_FREQUENCY
                      self.start_date_picker.GetValue().FormatISODate(), # SET_START_DATE
                      self.start_time_picker.GetValue(as_wxDateTime=True).FormatISOTime(), # SET_START_TIME
                      self.stop_date_picker.GetValue().FormatISODate(), # SET_STOP_DATE
                      self.stop_time_picker.GetValue(as_wxDateTime=True).FormatISOTime(), # SET_STOP_TIME
                      self.text_morse_speed.GetValue(), # SET_WPM
                      modulation_string, # SET_MODULATION
                      settings.STRING_ON, # SET_MORSING
                      self.text_amplitude.GetValue(), # SET_AMPLITUDE
                      "", # RESET HISTORY
                      secret]

        self.frame.Status("Program Fox " + str(number) + ": Write commands")
        progress_dialog.Update(10, "Program Fox " + str(number) + ": Write commands")

        fox_serial.ReadAll(con)

        # continue here -> others with commands and parameters, oscillator frequency in fox-panel
        for i in range(0, len(commands)):
            fox.SendCommand(con, commands[i], parameters[i], with_error=False)

        suc = True
        for i in range (0, len(commands)):
            allstr = fox_serial.ReadToPrompt(con)
            if fox.CheckRespond(allstr, False) == False:
                utils.MessageBox(i)
                suc = False
                break

        if suc == False:
            progress_dialog.Destroy()
            utils.MessageBox("Fox does not respond correctly")
            self.frame.Status("Program Fox " + str(number) + ": Failed")
            fox_serial.Close(con)
            return False

        self.frame.Status("Program Fox " + str(number) + ": Synchronise time") 
        progress_dialog.Update(20, "Program Fox " + str(number) + ": Synchronise time")
        if fox.SyncTime(con, with_error=False) == False:
            utils.MessageBox("Fox " + str(number) + " does not respond correctly")
            self.frame.Status("Program Fox " + str(number) + ": Failed")
            progress_dialog.Destroy()
            fox_serial.Writeln(con, settings.COMMAND_SET_RELOAD + settings.STRING_ON)
            fox_serial.ReadToPrompt()
            fox_serial.Close(con)
            return False
        progress_dialog.Update(30)

        fox_serial.Writeln(con, settings.COMMAND_SET_RELOAD + settings.STRING_ON)
        allstr = fox_serial.ReadToPrompt(con)
        if fox.CheckRespond(allstr, False) == False:
            utils.MessageBox("Fox does not respond correctly")
            self.frame.Status("Program Fox " + str(number) + ": Failed")
            fox_serial.Close(con)
            return False

        self.frame.Status("Fox " + str(number) + " programmed successfully")

        fox_serial.Close(con)
        progress_dialog.Destroy()
        return True

    def ProgramAll(self, event):
        for i in range(0, 6):
            if self.checkbox_fox[i].GetValue() == True:
                self.ProgramFoxNumber(i)

    def GetCom(self):
        return self.com

    def GetComAct(self):
        return self.com_act

    def SetPanelUser(self, panel_user):
        self.panel_user = panel_user

    def SetPanelResult(self, panel_result):
        self.panel_result = panel_result

    def CheckboxFoxChanged(self, event):
        checked = 0
        selected = self.combobox_repetition.GetValue()
        for i in range(0, 5):
            if self.checkbox_fox[i].GetValue() == True:
                checked += 1
        if checked == 0:
            checked = 1
        self.combobox_repetition_options = []
        for i in range(1, checked + 1):
            self.combobox_repetition_options.append(str(i))
        self.combobox_repetition.Clear()
        self.combobox_repetition.AppendItems(self.combobox_repetition_options)
        if int(selected) <= checked:
            self.combobox_repetition.SetValue(selected)
        else:
            self.combobox_repetition.SetSelection(checked - 1)

    def WriteXml(self, root):
        fox = ET.SubElement(root, "fox")
        for i in range(0, 6):
            tagname = "fox_" + str(i + 1)
            if i == 5:
                tagname = "fox_demo"
            attribs = {}
            attribs['call'] = self.combobox_call[i].GetValue()
            if i != 5:
                attribs['transmit_minute'] = self.combobox_minute[i].GetValue()
            if self.checkbox_fox[i].GetValue() == True:
                attribs['checked'] = settings.STRING_TRUE
            else:
                attribs['checked'] = settings.STRING_FALSE
            if i != 5:
                attribs['secret'] = str(self.fox_secret[i])
            ET.SubElement(fox, tagname, attribs)
        ET.SubElement(fox, "repetition_interval").text = self.combobox_repetition.GetValue()

        freq = ET.SubElement(root, "frequency")
        if self.checkbox_modulation.GetValue() == True:
            ET.SubElement(freq, "amplitude_modulation").text = settings.STRING_TRUE
        else:
            ET.SubElement(freq, "amplitude_modulation").text = settings.STRING_FALSE
        ET.SubElement(freq, "frequency").text = self.text_frequency.GetValue()
        ET.SubElement(freq, "demo_fox_frequency").text = self.text_demo_frequency_freq.GetValue()
        ET.SubElement(freq, "amplitude").text = self.text_amplitude.GetValue()

        timing = ET.SubElement(root, "timing")
        ET.SubElement(timing, "start_time").text = self.start_time_picker.GetValue(as_wxDateTime=True).FormatISOTime() # returned is wxDateTime not a DateTime!
        ET.SubElement(timing, "start_date").text = self.start_date_picker.GetValue().FormatISODate()
        ET.SubElement(timing, "stop_time").text = self.stop_time_picker.GetValue(as_wxDateTime=True).FormatISOTime()
        ET.SubElement(timing, "stop_date").text = self.stop_date_picker.GetValue().FormatISODate()

        morsing_attribs = {}
        morsing_attribs['speed'] = self.text_morse_speed.GetValue()
        morsing = ET.SubElement(root, "morsing", morsing_attribs)

    def ReadXml(self, root):
        # fox tag
        fox = root.find("fox")
        if fox == None:
            utils.MessageBox("File is not valid, no \"fox\"-tag found as direct child of root-tag")
            return False
        for i in range(0, 6):
            fox_name = "fox_" + str(i + 1)
            if i == 5:
                fox_name = "fox_demo"
            fox_x = fox.find(fox_name)
            if fox_x == None:
                utils.MessageBox("File is not valid, no \"" + fox_name + "\"-tag found as direct child of \"fox\"-tag")
                return False
            try:
                temp = fox_x.attrib['call']
                self.combobox_call[i].SetValue(temp)
            except KeyError:
                utils.MessageBox("File is not valid, \"" + fox_name + "\"-tag does not have a \"call\"-attribute")
                return False

            try:
                temp = fox_x.attrib['checked']
                if temp == settings.STRING_TRUE: 
                    self.checkbox_fox[i].SetValue(True)
                elif temp == settings.STRING_FALSE:
                    self.checkbox_fox[i].SetValue(False)
                else:
                    utils.MessageBox("File is not valid, \"checked\"-attribute in \"" + fox_name + "\"-tag must be either \"" + settings.TRING_TRUE + "\" or \"" + settings.STRING_FALSE + "\"")
                    return False
            except KeyError:
                utils.MessageBox("File is not valid, \"" + fox_name + "\"-tag does not have a \"checked\"-attribute")
                return False

            if i != 5:
                # not for demo-fox because there is no transmit_minute-attribute
                try:
                    temp = fox_x.attrib['transmit_minute']
                    self.combobox_minute[i].SetValue(temp)
                except KeyError:
                    utils.MessageBox("File is not valid, \"" + fox_name + "\"-tag does not have a \"transmit_minute\"-attribute")
                    return False

                try:
                    temp = fox_x.attrib['secret']
                    self.fox_secret[i] = int(temp)
                except KeyError:
                    utils.MessageBox("File is not valid, \"" + fox_name + "\"-tag does not have a \"secret\"-attribute")
                    return False
                except ValueError:
                    utils.MessageBox("File is not valid, \"" + fox_name + "\"-tag does not have an integer number as \"secret\"-attribute")
                    return False

        repetition_interval = fox.find("repetition_interval")
        if repetition_interval == None:
            utils.MessageBox("File is not valid, \"fox\"-tag does not have a \"repetition_interval\"-tag as direct child")    
            return False
        self.combobox_repetition.SetValue(repetition_interval.text)
        self.CheckboxFoxChanged(None)

        # frequency tag
        frequency = root.find("frequency")
        if frequency == None:
            utils.MessageBox("File is not valid, no \"frequency\"-tag found as direct child of root-tag")
            return False

        amplitude_modulation = frequency.find("amplitude_modulation")
        if amplitude_modulation == None:
            utils.MessageBox("File is not valid, \"frequency\"-tag does not have \"amplitude_modulation\"-tag as direct child")
            return False
        if amplitude_modulation.text == settings.STRING_TRUE: 
            self.checkbox_modulation.SetValue(True)
        elif amplitude_modulation.text == settings.STRING_FALSE:
            self.checkbox_modulation.SetValue(False)
        else:
            utils.MessageBox("File is not valid, text of \"amplitude_modulation\"-tag must be either \"" + settings.STRING_TRUE + "\" or \"" + settings.STRING_FALSE + "\"")
            return False

        freq = frequency.find("frequency")
        if freq == None:
            utils.MessageBox("File is not valid, child tag of root-tag \"frequency\" does not have a \"frequency\"-tag")
            return False
        self.text_frequency.SetValue(freq.text)

        demo_freq = frequency.find("demo_fox_frequency")
        if demo_freq == None:
            utils.MessageBox("File is not valid, \"frequency\"-tag does not have a \"demo_fox_frequency\"-tag as direct child")
            return False
        self.text_demo_frequency_freq.SetValue(demo_freq.text)

        amplitude = frequency.find("amplitude")
        if amplitude == None:
            utils.MessageBox("File is not valid, \"frequency\"-tag does not have a \"amplitude\"-tag as direct child")
            return False
        self.text_amplitude.SetValue(amplitude.text)

        # timing tag
        timing = root.find("timing")

        if timing == None:
            utils.MessageBox("File is not valid, no \"timing\"-tag found as direct child of root-tag")
            return False

        start_time = timing.find("start_time")
        if start_time == None:
            utils.MessageBox("File is not valid, \"timing\"-tag does not have a \"start_time\"-tag as direct child")
            return False
        wxdt = wx.DateTime()
        if wxdt.ParseISOTime(start_time.text) == False:
            utils.MessageBox("File is not valid, text of \"start_time\"-tag must be in iso8601-format (e.g 23:59:59)")
            return False
        self.start_time_picker.SetValue(wxdt)

        stop_time = timing.find("stop_time")
        if stop_time == None:
            utils.MessageBox("File is not valid, \"timing\"-tag does not have a \"stop_time\"-tag as direct child")
            return False
        wxdt = wx.DateTime()
        if wxdt.ParseISOTime(stop_time.text) == False:
            utils.MessageBox("File is not valid, text of \"stop_time\"-tag must be in iso8601-format (e.g. 23:59:59)")
            return False
        self.stop_time_picker.SetValue(wxdt)

        start_date = timing.find("start_date")
        if start_date == None:
            utils.MessageBox("File is not valid, \"timing\"-tag does not have a \"start_date\"-tag as direct child")
            return False
        wxdt = wx.DateTime()
        if wxdt.ParseISODate(start_date.text) == False:
            utils.MessageBox("File is not valid, text of \"start_date\"-tag must be in iso8601-format (e.g. 2016-04-08)")
            return False
        self.start_date_picker.SetValue(wxdt)

        stop_date = timing.find("stop_date")
        if stop_date == None:
            utils.MessageBox("File is not valid, \"timing\"-tag does not have a \"stop_date\"-tag as direct child")
            return False
        wxdt = wx.DateTime()
        if wxdt.ParseISODate(stop_date.text) == False:
            utils.MessageBox("File is not valid, text of \"stop_date\"-tag must be in iso8601-format (e.g. 2016-04-08)")
            return False
        self.stop_date_picker.SetValue(wxdt)

        # morsing tag
        morsing = root.find("morsing")

        if morsing == None:
            utils.MessageBox("File is not valid, \"morsing\"-tag found as direct child of root-tag")
            return False
        try:
            self.text_morse_speed.SetValue(morsing.attrib['speed'])
        except KeyError:
            utils.MessageBox("File is not valid, \"morsing\"-tag does not have a \"speed\"-attribute")
            return False
        return True
        

