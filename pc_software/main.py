#!/usr/bin/python
# -*- coding: utf-8 -*-

#
# main.py - main file of ARDF Transmitter PC software
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
# parents should be set correctly because if main form closes
# all children also get closed and therefore the program can exit
# if all dialogs, forms and widgets are destroyed!
#

#
# system imports:
#
import time
import wx
import wx.lib.masked.timectrl
import gc
from wx.lib.agw import ultimatelistctrl as ULC
from lxml import etree as ET
import webbrowser
import os

#
# own imports
#
import fox_dialogs
import fox_serial
import fox
import utils
import settings
import panel_user
import panel_fox
import panel_result

class MainNotebook(wx.Notebook):

    def __init__(self, parent):
        wx.Notebook.__init__(self, parent)

        self.panels = {}
        self.panels['pnl_fox'] = panel_fox.PanelFox(self, parent)
        self.panels['pnl_user'] = panel_user.PanelUser(self, parent, self.panels['pnl_fox'])
        self.panels['pnl_fox'].SetPanelUser(self.panels['pnl_user'])
        self.panels['pnl_result'] = panel_result.PanelResult(self, self.panels['pnl_fox'], self.panels['pnl_user'])
        self.panels['pnl_fox'].SetPanelResult(self.panels['pnl_result'])
        self.AddPage(self.panels['pnl_fox'], "Fox / Programming")
        self.AddPage(self.panels['pnl_user'], "User")
        self.AddPage(self.panels['pnl_result'], "Result")

        self.Bind(wx.EVT_NOTEBOOK_PAGE_CHANGING, self.NbChanging)

    def NbChanging(self, event):
        try:
            self.panels['pnl_result'].StopListen(None)
        except:
            pass

# function resource_path copied from: http://stackoverflow.com/questions/7674790/bundling-data-files-with-pyinstaller-onefile
def resource_path(relative_path):
    """ Get absolute path to resource, works for dev and for PyInstaller """
    try:
        # PyInstaller creates a temp folder and stores path in _MEIPASS
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(".")

    return os.path.join(base_path, relative_path)

class MainFrame(wx.Frame):
    def __init__(self):
        wx.Frame.__init__(self, None, title="ARDF Transmitter Configuration")

        self.filename = ""

        self.SetDoubleBuffered(True)

        self.CreateWidgets()
        self.SetInitialSize()

    def CreateWidgets(self):
        self.icon = wx.EmptyIcon()
        self.icon.CopyFromBitmap(wx.Bitmap(resource_path(settings.ICON_RELATIVE_PATH), wx.BITMAP_TYPE_ANY))
        self.SetIcon(self.icon)

        self.box_sizer = wx.BoxSizer(wx.VERTICAL)
        self.box_sizer_horizontal = wx.BoxSizer(wx.HORIZONTAL)

        self.SetBackgroundColour(wx.NullColour) # for a nice gray window background color
        self.Center()

        self.Bind(wx.EVT_CLOSE, self.OnClose)

        self.nb = MainNotebook(self)
        self.box_sizer.Add(self.box_sizer_horizontal, proportion=1, flag=wx.EXPAND|wx.ALL, border=3)
        self.box_sizer_horizontal.Add(self.nb, proportion=1, flag=wx.EXPAND)

        filemenu = wx.Menu()
        item_new = filemenu.Append(wx.ID_NEW, "&New")
        item_open = filemenu.Append(wx.ID_OPEN, "&Open")
        item_save = filemenu.Append(wx.ID_SAVE, "&Save")
        item_save_as = filemenu.Append(wx.ID_SAVEAS, "Save &As")
        filemenu.AppendSeparator()
        item_close = filemenu.Append(wx.ID_EXIT, "Exit")

        settingsmenu = wx.Menu()
        self.item_search_foxes_start = settingsmenu.Append(settings.ID_SETTINGS_SEARCH_FOXES_START, "Search Connected Foxes at Start", kind=wx.ITEM_CHECK)

        helpmenu = wx.Menu()
        item_user_manual = helpmenu.Append(settings.ID_HELP_USER_MANUAL, "User Manual (German)")
        item_about = helpmenu.Append(settings.ID_HELP_ABOUT, "About")

        self.Bind(wx.EVT_MENU, self.OnClose, item_close)
        self.Bind(wx.EVT_MENU, self.Save, item_save)
        self.Bind(wx.EVT_MENU, self.SaveAs, item_save_as)
        self.Bind(wx.EVT_MENU, self.Open, item_open)
        self.Bind(wx.EVT_MENU, self.New, item_new)
        self.Bind(wx.EVT_MENU, self.SearchFoxesAtStart, self.item_search_foxes_start)
        self.Bind(wx.EVT_MENU, self.HelpAbout, item_about)
        self.Bind(wx.EVT_MENU, self.HelpUserManual, item_user_manual)

        menuBar = wx.MenuBar()
        menuBar.Append(filemenu, "&File")
        menuBar.Append(settingsmenu, "&Settings")
        menuBar.Append(helpmenu, "&Help")
        self.SetMenuBar(menuBar)

        self.SetSizerAndFit(self.box_sizer)

        self.status_bar = wx.StatusBar(self, style=wx.SB_RAISED)
        self.status_bar.PushStatusText("Software ready")
        self.SetStatusBar(self.status_bar)

        self.file_config = wx.FileConfig(settings.APP_NAME, style=wx.CONFIG_USE_LOCAL_FILE)

        self.last_save_string =  ""

        self.ReadSettings()

        self.last_save_string = self.Saving(with_saving_to_file=False)

    def SaveChangesOnExit(self):
        # returns true if exit should continue, false if program should not exit
        if self.last_save_string != self.Saving(with_saving_to_file=False):
            dialog = wx.MessageDialog(self, "Do you want to save changes?", "Save changes", wx.YES_NO|wx.CANCEL)
            ret = dialog.ShowModal()
            if ret == wx.ID_YES:
                if self.Save(None) == False:
                    return False
            elif ret == wx.ID_CANCEL:
                return False
        return True

    def OnClose(self, event):
        if self.SaveChangesOnExit() == True:
            self.Destroy()

    def SearchFoxesAtStart(self, event):
        if self.item_search_foxes_start.IsChecked():
            self.file_config.Write(settings.SEARCH_FOXES_START_KEY, settings.STRING_TRUE)
        else:
            self.file_config.Write(settings.SEARCH_FOXES_START_KEY, settings.STRING_FALSE)
        self.file_config.Flush()

    def New(self, event):
        if self.SaveChangesOnExit() == True:
            self.nb.NbChanging(None) # to deactivate listening
            self.Destroy()
            settings.app.frame = MainFrame()
            if settings.app.frame.ShouldSearchFoxesStart():
                settings.app.frame.nb.panels['pnl_fox'].TestFoxes(None)
            settings.app.frame.Show()

    def Open(self, event):
        openFileDialog = wx.FileDialog(self, "Open File", "", "", "ARDF-Transmitter Files (*.ardf)|*.ardf|All Files (*)|*", wx.FD_OPEN | wx.FD_FILE_MUST_EXIST)
        ret = openFileDialog.ShowModal()
        fn = openFileDialog.GetPath()
        openFileDialog.Destroy()
        if ret == wx.ID_CANCEL:
            return
        try:
            tree = ET.parse(fn)
        except ET.XMLSyntaxError:
            utils.MessageBox("File does not seem to be a valid XML-File")
            return
        root = tree.getroot()
        if root.tag != "transmitter":
            utils.MessageBox("File is not valid, root-tag name must be \"transmitter\"")
            return

        version = root.find("software_version")
        if version.text != settings.VERSION:
            msg = wx.MessageDialog(None, "File seems to be created with another software version, read it anyway?", style=wx.YES_NO|wx.CANCEL|wx.ICON_QUESTION)
            ret = msg.ShowModal()
            msg.Destroy()
            if ret != wx.ID_YES:
                return
        success = True
        for key, i in self.nb.panels.iteritems():
            if i.ReadXml(root) == False:
                success = False
        if success == True:
            self.filename = fn
            self.last_save_string = self.Saving(with_saving_to_file=False)
            self.nb.NbChanging(None)
        else:
            self.last_save_string = self.Saving(with_saving_to_file=False)
            self.nb.NbChanging(None)
            self.New(None)

    def Save(self, event):
        if self.filename == "":
            fn = self.SaveFileWithDialog()
            if fn == "":
                return False
            self.filename = fn
        self.Saving(self.filename)
        return True

    def SaveAs(self, event):
        fn = self.SaveFileWithDialog()
        if fn == "":
            return
        self.filename = fn
        self.Saving(self.filename)
        return

    def SaveFileWithDialog(self):
        saveFileDialog = wx.FileDialog(self, "Save File", "", "", "ARDF-Transmitter Files (*.ardf)|*.ardf|All Files (*)|*", wx.FD_SAVE | wx.FD_OVERWRITE_PROMPT)
        ret = saveFileDialog.ShowModal()
        fn = saveFileDialog.GetPath()
        saveFileDialog.Destroy()
        if ret == wx.ID_CANCEL:
            return ""
        return fn

    def Saving(self, filename="", with_saving_to_file=True):
        root = ET.Element("transmitter")
        doc = ET.SubElement(root, "software_version").text = settings.VERSION
        for key, i in self.nb.panels.iteritems(): # http://stackoverflow.com/questions/3294889/iterating-over-dictionaries-using-for-loops-in-pythonc
            i.WriteXml(root)
        tree = ET.ElementTree(root)
        if with_saving_to_file:
            tree.write(filename, encoding='utf-8', xml_declaration=True, pretty_print=True)
        return ET.tostring(root, encoding="utf-8", xml_declaration=True, pretty_print=True)

    def Status(self, string):
        self.status_bar.PushStatusText(str(string))

    def HelpUserManual(self, event):
        webbrowser.open(resource_path(settings.USER_MANUAL_RELATIVE_PATH))
    
    def HelpAbout(self, event):
        msg =   "ARDF Transmitter Configuration\n" + \
                "\n" + \
                "Copyright (C) 2016 Simon Kaufmann, Josef Heel, HeKa\n" + \
                "\n" + \
                "This program comes with ABSOLUTELY NO WARRANTY\r\n" + \
                "This is free software, you are welcome to redistribute it\r\n" + \
                "under the terms of the GNU General Public License version 3.\r\n" + \
                "\r\n" + \
                "For further information and documentation related to this project visit:\r\n" + \
                "http://ardf.heka.or.at/\r\n"
        dialog = wx.MessageDialog(self, msg, "ARDF Transmitter Configuration", style=wx.OK)
        dialog.ShowModal()
        dialog.Destroy()


    def ReadSettings(self):
        if self.file_config.Read(settings.SEARCH_FOXES_START_KEY) == settings.STRING_FALSE:
            self.item_search_foxes_start.Check(False)
        else:
            self.item_search_foxes_start.Check(True)

    def ShouldSearchFoxesStart(self):
        return self.item_search_foxes_start.IsChecked()

class MainApp(wx.App):

    def __init__(self):
        wx.App.__init__(self, False)
        
    def OnInit(self):
        self.frame = MainFrame()
        self.frame.Show()
        if self.frame.ShouldSearchFoxesStart():
            self.frame.nb.panels['pnl_fox'].TestFoxes(None)
        return True

if __name__ == '__main__':
    settings.app = MainApp()
    settings.app.MainLoop()
