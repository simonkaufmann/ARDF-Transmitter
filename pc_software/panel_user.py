#!/usr/bin/python
# -*- coding: utf-8 -*-

#
# panel_user.py - code for the user-tab in the software
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

#
# own imports
#
import settings
import utils
import fox_serial
import fox
import fox_dialogs

class PanelUser(wx.Panel):

    def __init__(self, parent, frame, panel_fox):
        wx.Panel.__init__(self, parent)

        self.frame = frame

        self.panel_fox = panel_fox

        self.SetDoubleBuffered(True)

        box_sizer = wx.BoxSizer(wx.HORIZONTAL)

        self.list_ctrl = ULC.UltimateListCtrl(self, agwStyle=wx.LC_REPORT|wx.LC_VRULES|wx.LC_HRULES|ULC.ULC_HAS_VARIABLE_ROW_HEIGHT)#, style=wx.LC_REPORT)
        # refer to: http://stackoverflow.com/questions/15466244/adding-a-button-to-every-row-in-listctrl-wxpython
        # for UltimateListCtrl
        box_sizer_horizontal = wx.BoxSizer(wx.HORIZONTAL)
        box_sizer.Add(self.list_ctrl, 1, border=3, flag=wx.EXPAND|wx.RIGHT)

        button_add = wx.Button(self, label="Add")
        button_edit = wx.Button(self, label="Edit")
        button_delete = wx.Button(self, label="Delete")
        button_clear_programmed = wx.Button(self, label="Clear Flag Created")
        button_clear_all = wx.Button(self, label="Clear All")

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

        box_sizer_vertical = wx.BoxSizer(wx.VERTICAL)
        box_sizer_vertical.Add(button_add, flag=wx.EXPAND)
        box_sizer_vertical.Add(button_edit, flag=wx.EXPAND)
        box_sizer_vertical.Add(button_delete, flag=wx.EXPAND)
        box_sizer_vertical.AddSpacer(30)
        box_sizer_vertical.Add(button_clear_programmed, flag=wx.EXPAND)
        box_sizer_vertical.Add(button_clear_all, flag=wx.EXPAND)
        box_sizer_vertical.AddSpacer(30)
        box_sizer_vertical.Add(wx.StaticText(self, label="Fox Number:"), border=8, flag=wx.BOTTOM)
        box_sizer_vertical.Add(self.combobox_fox)

        box_sizer.Add(box_sizer_vertical, flag=wx.ALIGN_CENTER_VERTICAL)

        self.Bind(wx.EVT_BUTTON, self.Add, button_add)
        self.Bind(wx.EVT_BUTTON, self.Edit, button_edit)
        self.Bind(wx.EVT_BUTTON, self.Delete, button_delete)
        self.Bind(wx.EVT_BUTTON, self.ClearAll, button_clear_all)
        self.Bind(wx.EVT_BUTTON, self.ClearFlagsCreated, button_clear_programmed)

        self.buttons_program = []
        self.checkbox_created = []
        self.names = []
        self.tag_id = []

        self.list_ctrl.InsertColumn(1, "Name", width=130)
        self.list_ctrl.InsertColumn(2, "Tag ID", width=80)
        self.list_ctrl.InsertColumn(3, "Program", width=120)
        self.list_ctrl.InsertColumn(4, "Tag Created", width=100)

        self.SetSizerAndFit(box_sizer)

        self.last_tagid = settings.TAG_ID_MIN

    def Add(self, event):
        user_dialog = fox_dialogs.UserDialog(self, self.last_tagid)
        self.last_tagid = self.last_tagid + 1
        if self.last_tagid > settings.TAG_ID_MAX:
            self.last_tagid = settings.TAG_ID_MIN
        if user_dialog.ShowModal() == wx.ID_OK:
            name = user_dialog.GetName()
            tag_id = user_dialog.GetTagId()
            self.AddElement(name, tag_id)
        user_dialog.Destroy()
        return

    def Edit(self, event):
        item = self.list_ctrl.GetFocusedItem()
        if item == -1:
            return
        name = self.list_ctrl.GetItem(item, 0).GetText()
        tag_id = self.list_ctrl.GetItem(item, 1).GetText()
        user_dialog = fox_dialogs.UserDialog(self, int(tag_id), name)
        if user_dialog.ShowModal() == wx.ID_OK:
            name = user_dialog.GetName()
            self.list_ctrl.SetStringItem(item, 0, str(name))
            tag_id = user_dialog.GetTagId()
            self.list_ctrl.SetStringItem(item, 1, str(tag_id))
        user_dialog.Destroy()
        return

    def Delete(self, event):
        focused = self.list_ctrl.GetFocusedItem()
        if focused != -1:
            self.list_ctrl.DeleteItem(focused)
        return

    def AddElement(self, name, tag_id, created=False):
        item = self.list_ctrl.Append([str(name), str(tag_id)])
        self.buttons_program.append(wx.Button(self.list_ctrl, label="Program Tag"))
        self.Bind(wx.EVT_BUTTON, self.Program, self.buttons_program[len(self.buttons_program) - 1])
        self.checkbox_created.append(wx.CheckBox(self.list_ctrl, label="Created"))
        self.checkbox_created[len(self.checkbox_created) - 1].SetValue(created)
        self.list_ctrl.SetItemWindow(item, col=2, wnd=self.buttons_program[len(self.buttons_program) - 1], expand=True)
        self.list_ctrl.SetItemWindow(item, col=3, wnd=self.checkbox_created[len(self.checkbox_created) - 1], expand=True)

    def Program(self, event):
        index = -1
        for i in range(0, len(self.buttons_program)):
            if self.buttons_program[i] == event.GetEventObject():
                index = i
        if index == -1:
            # no index of button found, should not occur
            return

        self.tag_id_set_id =  self.list_ctrl.GetItem(index, 1).GetText()
        self.index_set_id = index

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

        self.progress_dialog_set_id = wx.ProgressDialog("Set Transponder ID", "Take the transponder near the rfid module", 2, parent=self, style=wx.PD_APP_MODAL|wx.PD_AUTO_HIDE|wx.PD_CAN_ABORT)
        self.progress_dialog_set_id.SetInitialSize()
        self.progress_dialog_set_id.Update(0)

        self.con_set_id = fox_serial.Open(com[fox_number])
        if self.con_set_id == None:
            self.progress_dialog_set_id.Destroy()
            return

        fox.SendCommand(self.con_set_id, settings.COMMAND_SET_ID, str(self.tag_id_set_id))

        self.timer_set_id = wx.Timer(self)
        self.timer_set_id.Start(1000)
        self.Bind(wx.EVT_TIMER, self.SetIdTimer, self.timer_set_id)

    def SetIdTimer(self, event):
        self.timer_set_id.Stop()
        retstr = fox_serial.Readln(self.con_set_id, False)
        ret = self.progress_dialog_set_id.Update(0) 
        if ret[0] == False:
            # user cancelled 
            self.progress_dialog_set_id.Destroy()
            self.timer_set_id.Stop()
            fox_serial.Writeln(self.con_set_id, "exit")
            fox_serial.Close(self.con_set_id)
            return
        if retstr.find("tag id " + str(self.tag_id_set_id) + " written!") >= 0:
            self.frame.Status("Tag " + str(self.tag_id_set_id) + " successfully written!")
            self.checkbox_created[self.index_set_id].SetValue(True)
            self.progress_dialog_set_id.Destroy()
            self.timer_set_id.Stop()
            fox_serial.Close(self.con_set_id)
            return
        self.timer_set_id.Start(1500)

        
    def ClearAll(self, event):
        dialog = wx.MessageDialog(self, "Clear all participants?", style=wx.YES_NO|wx.CANCEL)
        if dialog.ShowModal() == wx.ID_YES:
            self.list_ctrl.DeleteAllItems()
        dialog.Destroy()
        return

    def ClearFlagsCreated(self, event):
        for i in range(0, len(self.checkbox_created)):
            self.checkbox_created[i].SetValue(False)
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

    def GetName(self, tag_id):
        for i in range(0, self.list_ctrl.GetItemCount()):
            if int(self.list_ctrl.GetItem(i, 1).GetText()) == int(tag_id):
                return self.list_ctrl.GetItem(i, 0).GetText()
        return False

    def WriteXml(self, root):
        user = ET.SubElement(root, "user")
        number = self.list_ctrl.GetItemCount()
        for i in range(0, number):
            name = self.list_ctrl.GetItem(i, 0).GetText()
            tag_id = self.list_ctrl.GetItem(i, 1).GetText()
            tag_created = self.list_ctrl.GetItemWindow(i, 3).IsChecked()
            attribs = {}
            attribs['name'] = name
            attribs['tag_id'] = tag_id
            if tag_created:
                attribs['created'] = settings.STRING_TRUE
            else:
                attribs['created'] = settings.STRING_FALSE
            ET.SubElement(user, "user_item", attribs)
        return

    def ReadXml(self, root):
        user = root.find("user")
        if user == None:
            utils.MessageBox("File is not valid, no user-tag as direct child of root tag")
            return False

        self.list_ctrl.DeleteAllItems()
        for child in user:
            if child.tag == "user_item":
                try:
                    name = child.attrib['name']
                except KeyError:
                    utils.MessageBox("File is not valid, tag \"user_item\" does not have a name-attribute")
                    return False
                try:
                    tag_id = child.attrib['tag_id']
                except KeyError:
                    utils.MessageBox("File is not valid, tag \"user_item\" does not have a tag_id-attribute")
                    return False
                try:
                    created = child.attrib['created']
                except KeyError:
                    utils.MessageBox("File is not valid, tag \"user_item\" does not have a created-attribute")
                    return False
                if created == settings.STRING_TRUE:
                    created = True
                elif created == settings.STRING_FALSE:
                    created = False
                else:
                    utils.MessageBox("File is not valid, attribute \"created\" in tag \"user_item\" must be either True or False")
                    return False
                self.AddElement(name, tag_id, created)
        return True
