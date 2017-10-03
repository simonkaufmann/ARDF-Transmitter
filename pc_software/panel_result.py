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
import threading

#
# own imports
#
import settings
import utils
import fox_serial
import fox
import fox_dialogs

class PanelResult(wx.Panel):

    def __init__(self, parent, panel_fox, panel_user):
        self.myEVT_TAG_FOUND = wx.NewEventType()
        self.EVT_TAG_FOUND = wx.PyEventBinder(self.myEVT_TAG_FOUND, 1)

        wx.Panel.__init__(self, parent)

        self.panel_fox = panel_fox
        self.panel_user = panel_user

        self.SetDoubleBuffered(True)

        box_sizer = wx.BoxSizer(wx.VERTICAL)

        box_sizer_read_tag = wx.BoxSizer(wx.HORIZONTAL)
        self.com_act = self.panel_fox.GetComAct()
        self.fox_number_string = []
        for i in range(0, len(self.com_act) - 1):
            if self.com_act[i] == True:
                fox = str(i)
                if i == settings.FOX_NUMBER_DEMO:
                    fox = "Demo"
                self.fox_number_string.append(fox)

        if len(self.fox_number_string) == 0:
            self.fox_number_string.append(settings.NO_FOX_MESSAGE)
        self.combobox_fox= wx.ComboBox(self, value=self.fox_number_string[0], choices=self.fox_number_string, style=wx.CB_READONLY)
        self.button_listen_tag = wx.Button(self, label="Listen")
        self.button_stop_listen_tag = wx.Button(self, label="Stop Listening")
        self.button_stop_listen_tag.Disable()
        box_sizer_read_tag.Add(wx.StaticText(self, label="Fox number:"), border=5, flag=wx.RIGHT|wx.ALIGN_CENTER_VERTICAL)
        box_sizer_read_tag.Add(self.combobox_fox, border=10, flag=wx.RIGHT|wx.ALIGN_CENTER_VERTICAL)
        box_sizer_read_tag.Add(self.button_listen_tag, border=10, flag=wx.RIGHT|wx.ALIGN_CENTER_VERTICAL)
        box_sizer_read_tag.Add(self.button_stop_listen_tag, flag=wx.ALIGN_CENTER_VERTICAL)

        box_sizer_list_ctrl_tag = wx.BoxSizer(wx.HORIZONTAL)
        self.list_ctrl_tag = ULC.UltimateListCtrl(self, agwStyle=wx.LC_REPORT|wx.LC_VRULES|wx.LC_HRULES|ULC.ULC_HAS_VARIABLE_ROW_HEIGHT)

        box_sizer_list_ctrl_tag_buttons = wx.BoxSizer(wx.HORIZONTAL)
        self.button_clear_all_tag = wx.Button(self, label="Clear All")
        self.button_export_tag = wx.Button(self, label="Export CSV")
        self.button_delete_tag = wx.Button(self, label="Delete")
        self.button_set_finish_time = wx.Button(self, label="Set Finish Time")
        box_sizer_list_ctrl_tag_buttons.Add(self.button_delete_tag)
        box_sizer_list_ctrl_tag_buttons.Add(self.button_clear_all_tag)
        box_sizer_list_ctrl_tag_buttons.Add(self.button_set_finish_time)
        box_sizer_list_ctrl_tag_buttons.Add(self.button_export_tag)

        box_sizer_list_ctrl_tag.Add(self.list_ctrl_tag, proportion=1, flag=wx.EXPAND)

        self.list_ctrl_tag.InsertColumn(2, "Name", width=130)
        self.list_ctrl_tag.InsertColumn(1, "Tag ID", width=80)
        self.list_ctrl_tag.InsertColumn(3, "Fox 1")
        self.list_ctrl_tag.InsertColumn(4, "Fox 2")
        self.list_ctrl_tag.InsertColumn(5, "Fox 3")
        self.list_ctrl_tag.InsertColumn(6, "Fox 4")
        self.list_ctrl_tag.InsertColumn(7, "Fox 5")
        self.list_ctrl_tag.InsertColumn(8, "Finish")
        self.list_ctrl_tag.InsertColumn(9, "Secrets correct")

        box_sizer.Add(box_sizer_read_tag, border=10, flag=wx.ALL|wx.EXPAND)
        box_sizer.Add(box_sizer_list_ctrl_tag, proportion=1, border=10, flag=wx.ALL|wx.EXPAND)
        box_sizer.Add(box_sizer_list_ctrl_tag_buttons, border=10, flag=wx.LEFT|wx.BOTTOM|wx.EXPAND)

        self.Bind(wx.EVT_BUTTON, self.Listen, self.button_listen_tag)
        self.Bind(wx.EVT_BUTTON, self.StopListen, self.button_stop_listen_tag)

        self.Bind(wx.EVT_BUTTON, self.Delete, self.button_delete_tag)
        self.Bind(wx.EVT_BUTTON, self.ClearAll, self.button_clear_all_tag)
        self.Bind(wx.EVT_BUTTON, self.SetFinishTime, self.button_set_finish_time)
        self.Bind(wx.EVT_BUTTON, self.ExportCsv, self.button_export_tag)

        self.SetSizerAndFit(box_sizer)

    def Delete(self, event):
        focused = self.list_ctrl_tag.GetFocusedItem()
        if focused != -1:
            self.list_ctrl_tag.DeleteItem(focused)

    def ClearAll(self, event):
        dialog = wx.MessageDialog(self, "Clear all read transponder?", style=wx.YES_NO|wx.CANCEL)
        if dialog.ShowModal() == wx.ID_YES:
            self.list_ctrl_tag.DeleteAllItems()
        dialog.Destroy()
        return

    def SetFinishTime(self, event):
        focused = self.list_ctrl_tag.GetFocusedItem()
        if focused == -1:
            return
        dialog = fox_dialogs.SetTimeDialog(self)
        if dialog.ShowModal() == wx.ID_OK:
            time = dialog.GetFinishingTime()
            self.list_ctrl_tag.SetStringItem(focused, 7, time)
        dialog.Destroy()
        return

    def ExportCsv(self, event):
        saveFileDialog = wx.FileDialog(self, "Export Result as CSV", "", "", "CSV-File (*.csv)|*.csv|All Files (*)|*", wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT)
        ret = saveFileDialog.ShowModal()
        fn = saveFileDialog.GetPath()
        saveFileDialog.Destroy()
        if ret == wx.ID_CANCEL:
            return
        
        file = open(fn, "w")
        file.write("Name, Tag-ID, Fox1 Time, Fox2 Time, Fox3 Time, Fox4 Time, Fox5 Time, Finishing Time, Secrets\n")
        for i in range(0, self.list_ctrl_tag.GetItemCount()):
            name = self.list_ctrl_tag.GetItem(i, 0).GetText()
            tag_id = self.list_ctrl_tag.GetItem(i, 1).GetText()
            fox = []
            for j in range(0, 5):
                fox.append(self.list_ctrl_tag.GetItem(i, 2 + j).GetText())
            finish = self.list_ctrl_tag.GetItem(i, 7).GetText()
            secret = self.list_ctrl_tag.GetItem(i, 8).GetText()
            file.write(str(name) + ", " + str(tag_id) + ", ")
            for j in range(0, 5):
                file.write(str(fox[j]) + ", ")
            file.write(str(finish) + ", " + str(secret) + "\n")
        file.close()
        return

    def RefreshFoxCheckbox(self):
        val = self.combobox_fox.GetValue()
        self.com_act = self.panel_fox.GetComAct()
        self.fox_number_string = []
        for i in range(0, len(self.com_act) - 1):
            if self.com_act[i] == True:
                fox = str(i)
                if i == settings.FOX_NUMBER_DEMO:
                    fox = "Demo"
                self.fox_number_string.append(fox)

        if len(self.fox_number_string) == 0:
            self.fox_number_string.append(settings.NO_FOX_MESSAGE)
        self.combobox_fox.Clear()
        self.combobox_fox.AppendItems(self.fox_number_string)
        valexists = False
        for i in self.fox_number_string:
            if i == val:
                valexists = True
        if valexists:
            self.combobox_fox.SetValue(val)
        else:
            self.combobox_fox.SetSelection(0)

    def Listen(self, event):
        fox_number = self.combobox_fox.GetValue()
        if fox_number == settings.DEMO_FOX_STRING:
            fox_number = 0
        elif fox_number == settings.NO_FOX_MESSAGE:
            utils.MessageBox("No Fox Detected! Connect fox and press Refresh-button in Fox-tab")
            return False
        else:
            fox_number = int(fox_number)

        com = self.panel_fox.GetCom()

        if len(com) > fox_number:
            if com[fox_number] == "":
                self.panel_fox.TestFoxes(None)
                com = self.panel_fox.GetCom()
        else:
            self.panel_fox.TestFoxes(None)
            com = self.panel_fox.GetCom()

        if com[fox_number] == "":
            utils.MessageBox("Selected Fox does not seem to be connected")
            return

        self.con_listen_tag = fox_serial.Open(com[fox_number])
        if self.con_listen_tag == None:
            return

        self.thread_listen_tag = self.ListenThread(self, self.con_listen_tag)
        self.thread_listen_tag.start()
        self.Bind(self.EVT_TAG_FOUND, self.TagFound)

        self.button_stop_listen_tag.Enable()
        self.button_listen_tag.Disable()

    def StopListen(self, event):
        try:
            self.thread_listen_tag.stops()
            fox_serial.Close(self.con_listen_tag)
    
            self.button_listen_tag.Enable()
            self.button_stop_listen_tag.Disable()
        except:
            pass

    def TagFound(self, event):
        tag_string = self.thread_listen_tag.GetTag()
        if self.ReadTag(tag_string) == False:
            utils.MessageBox("Error reading tag")

    def ReadTag(self, tag_string):
        for i in range(0, len(tag_string)):
            if tag_string[i].find(settings.MESSAGE_ERROR_TAG) >= 0:
                utils.MessageBox("Error reading tag")
                return False

        ret = self.TestString(tag_string[1], settings.TAG_TAG_ID)
        if ret[0] != False and ret[1] != False:
            tag_id = ret[2]
        else:
            return False

        secret = []
        for i in range(0, 5):
            ret = self.TestString(tag_string[2 + i], settings.TAG_FOX[i])
            if ret[0] != False and ret[1] != False:
                secret.append(ret[2])
            else:
                return False
        
        history_tag_id = []
        history_timestamp = []
        for j in range(0, 5):
            history_tag_id.append([])
            history_timestamp.append([])
            for i in range(0, settings.TAG_HISTORY_ENTRIES_PER_FOX):
                ret = self.TestString(tag_string[9 + j * (settings.TAG_HISTORY_ENTRIES_PER_FOX * 2 + 2) + 2 * i], settings.TAG_TAG_ID) # + 2 because of one line with "" and one line with "Fox x:"
                if ret[0] != False and ret[1] != False:
                    history_tag_id[j].append(ret[2])
                else:
                    return False
                ret = self.TestString(tag_string[9 + j * (settings.TAG_HISTORY_ENTRIES_PER_FOX * 2 + 2) + 2 * i + 1], settings.TAG_TIMESTAMP) # + 2 because of one line with "" and one line with "Fox x:"
                if ret[0] != False and ret[1] != False:
                    history_timestamp[j].append(ret[2])
                else:
                    return False

        # check secret:
        correct_secret = [True, True, True, True, True]
        all_secrets_correct = True
        secret_msg = ""
        for i in range(0, 5):
            if self.panel_fox.GetSecret()[i] != secret[i]:
                correct_secret[i] = False
                secret_msg += "n"
                all_secrets_correct = False
            else:
                secret_msg += "y"

        time = []
        for i in range(0, 5):
            tim = self.panel_fox.GetStartTime()
            tim.AddTS(wx.TimeSpan(0, 0, history_timestamp[i][0], 0))
            time.append(tim.FormatISOTime())

        self.AddElement(tag_id, time, secret_msg)


    def TestString(self, string_give, string_check):
        if string_give.find(string_check) >= 0:
            try:
                val = string_give[len(string_check):len(string_give)]
                return [True, True, int(val)]
            except ValueError:
                return [True, False, 0] 
        else:
            return [False, False, 0]

    def AddElement(self, tag_id, time, secrets, finish_time="", name="", set_name=False):
        if set_name == False: 
            try:
                tag_id = int(tag_id)
                name = self.panel_user.GetName(tag_id)
            except ValueError:
                # value is not a number
                pass
        item = self.list_ctrl_tag.Append([str(name), str(tag_id), str(time[0]), str(time[1]), str(time[2]), str(time[3]), str(time[4]), str(finish_time), str(secrets)])
        return

    def WriteXml(self, root):
        result = ET.SubElement(root, "result")
        number = self.list_ctrl_tag.GetItemCount()
        for i in range(0, number):
            name = self.list_ctrl_tag.GetItem(i, 0).GetText()
            tag_id = self.list_ctrl_tag.GetItem(i, 1).GetText()
            attribs = {}
            for j in range(0, 5):
                attribs['fox' + str(j + 1)] = self.list_ctrl_tag.GetItem(i, 2 + j).GetText()
            attribs['finish'] = self.list_ctrl_tag.GetItem(i, 7).GetText()
            attribs['secret'] = self.list_ctrl_tag.GetItem(i, 8).GetText()
            attribs['name'] = name
            attribs['tag_id'] = tag_id
            ET.SubElement(result, "result_item", attribs)
        return

    def ReadXml(self, root):
        result = root.find("result")
        if result == None:
            utils.MessageBox("File is not valid, no result-tag as direct child of root tag")
            return False

        self.list_ctrl_tag.DeleteAllItems()
        for child in result:
            if child.tag == "result_item":
                try:
                    name = child.attrib['name']
                except KeyError:
                    utils.MessageBox("File is not valid, tag \"result_item\" does not have a name-attribute")
                    return False
                try:
                    tag_id = child.attrib['tag_id']
                except KeyError:
                    utils.MessageBox("File is not valid, tag \"result_item\" does not have a tag_id-attribute")
                    return False
                fox = []
                try:
                    for i in range(0, 5):
                        fox.append(child.attrib['fox' + str(i + 1)])
                except KeyError:
                    utils.MessageBox("File is not valid, tag \"result_item\" does not have all fox-attributes (1 to 5)")
                    return False
                try:
                    finish = child.attrib['finish']
                except KeyError:
                    utils.MessageBox("File is not valid, tag \"result_item\" does not have a finish-attribute")
                    return False
                try:
                    secret = child.attrib['secret']
                except KeyError:
                    utils.MessageBox("File is not valid, tag \"result_item\" does not have a secret-attribute")
                    return False
                self.AddElement(tag_id, fox, secret, finish, name, True)
        return True

    # threading code is copied and modified from online example in 
    # wxPython-wiki: http://wiki.wxpython.org/Non-Blocking%20Gui
    class ListenThread(threading.Thread):
    
        def __init__(self, parent, con):
            threading.Thread.__init__(self)
            self.con = con
            self._parent = parent
            self.allstr = ""
    
        def run(self):
            self.stop = False
            while self.stop == False:
                line = fox_serial.Readln(self.con)
                if line.find(settings.MESSAGE_NEW_TAG) >= 0:
                    self.allstr = fox_serial.ReadToEndTag(self.con)
                    evt = self._parent.TagFoundEvent(self._parent.myEVT_TAG_FOUND, -1)
                    wx.PostEvent(self._parent, evt)
                time.sleep(0.1)

        def GetTag(self):
            return self.allstr
    
        def stops(self):
            self.stop = True
    
    class TagFoundEvent(wx.PyCommandEvent):
    
        def __init__(self, etype, eid, value=None):
            wx.PyCommandEvent.__init__(self, etype, eid)
            self._value = value
        
        def GetValue(self):
            return self._value
    

