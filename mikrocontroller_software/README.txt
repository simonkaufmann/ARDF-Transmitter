--------------------------------------------------------------------------------


                                DIPLOMARBEIT

                                   an der

   Höheren Technischen Bundeslehr- und Versuchsanstalt Innsbruck Anichstraße

                            ----------------------

                            Amateurfunkpeilsender

                        mit RFID - Teilnehmererkennung


--------------------------------------------------------------------------------


MIKROCONTROLLER - SOFTWARE:
---

Die Mikrocontrollersoftware ist in der Programmiersprache C verfasst. Der 
Quellcode befindet sich in den *.c und *.h-Dateien. 

Das fertige Programm ist als Intel-Hex-File main.hex in diesem Ordner verfügbar 
und kann auf den Mikrocontroller geladen werden. Das Hex-File wurde mithilfe 
des GCC-Compilers avr-gcc Version 4.9.2 erstellt.


ÜBERSETZUNG DES PROGRAMMS:
---

Zur Übersetzung des Quellcodes wird der Compiler avr-gcc benötigt. Das Hex-File 
kann mithilfe des Befehls "make all" in der Kommandozeile erzeugt werden. Dazu 
muss das Make-System installiert werden (auf Linux-PCs standardmäßig 
enthalten). Unter Windows liefert WinAVR den Compiler und das Make-System mit 
(http://winavr.sourceforge.net/).


KONFIGURATION DES PROGRAMMS:
---

Im Makefile findet sich folgender Abschnitt:

CFLAGS += -DNEW_PROTOTYPE # comment out if using the old prototype, if you do not know how to set -> let it uncommented
CFLAGS += -DBLINKING_IS_LED_STATE # led blinks if set blink-command is sent over uart
CFLAGS += -DEACH_COMMAND_RELOAD # reload configuration after each command
CFLAGS += -DWITH_IOSYNC # iosync-pin is used for dds communication (better for long dds operations because if WITH_IOSYNC is not set one wrong byte to dds can destroy the whole communication 
#CFLAGS += -DDDS_FUNC_IS_LED_STATE # debug: the last executed dds_on or dds_off function determines led state (regardless whether function succeeded)
#CFLAGS += -DDDS_STATE_IS_LED_STATE # debug: the real dds state is the led state
#CFLAGS += -DSPI_NOT_FREE_LED # debug: when in dds timer interrupt spi is recognized as not free
#CFLAGS += -DISR_LED # debug: LED ligths if controller is inside isr so that isr-times can be checked
#CFLAGS += -DISR_LED_MODULATION # debug: LED lights if controller is inside isr for amplitude modulation
#CFLAGS += -DSPI_NOT_FREE_LED # debug: LED lights if amplitude modulation cannot be done because spi is not free at the moment
#CFLAGS += -DDEBUG_START_TIME # debug: debug messages for start and stop time
#CFLAGS += -DDEBUG_MORSE # debug: send debug messages over uart as soon as morsing starts or stops
#CFLAGS += -DDEBUG_WRITE_HISTORY # debug: send debug messages for rfid-tag-processing

Durch Entfernen des Kommentarzeichens "#" kann die entsprechende Option
aktiviert oder deaktiviert werden. Danach muss der Ordner mit dem Befehl
"make clean" gereinigt und das Programm mit "make all" neu übersetzt werden.
