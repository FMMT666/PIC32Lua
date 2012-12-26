@echo off
rem
rem   Minimal PIC32Lua programming helper
rem
rem   Move this batch file and the "PIC32Lua-ChipkitMAX32.hex" file to
rem   your "MPide" root directory.
rem
rem
rem   USAGE:
rem     asProgPIC32Lua <COMPORT>
rem     
rem     Where "COMPORT" is the name of your "chipKIT-Max32" FTDI interface.
rem     E.g. COM1, COM2, COM6...
rem
rem
rem   ASkr 11/2011
rem


set PIC32LUAHEXFILE=PIC32Lua-ChipkitMAX32.hex
set AVRDUDE=hardware\tools\avr\bin\avrdude.exe
set AVRDUDECFG=hardware\tools\avr\etc\avrdude.conf


rem ---------------------------------------------------------------------------------
if "%1" == "" goto USAGE

echo PROGRAMMING %PIC32LUAHEXFILE%

%AVRDUDE% -C %AVRDUDECFG% -c stk500v2 -P %1 -p pic32 -b 115200 -U flash:w:%PIC32LUAHEXFILE%:i -v 

goto QUIT

:USAGE
	echo ---
	echo USAGE:
	echo asProgPIC32Lua COMPORT
	echo where COMPORT is COM1, COM2, COM6...
	echo ---

:QUIT
