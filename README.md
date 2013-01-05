
PIC32Lua
========

Lua for PIC32 Processors.  
  
A newly created Git playground.  
For binary releases see [www.askrprojects.net/software/pic32lua/index.html][1]

---

## CHANGES

### V0.1a
    - NEW: port "names" ("pic.port.A", "pic.port.B", ...)
    - NEW: PortLatch(<port>, <state>)
    - FIX: some cleanups in asIO.c (stdio, stderr, io.write, ...)

          
### V0.1b
    - NEW: binary number representation with, e.g.: '0b001110'
    - NEW: timer tick-time variable (100ns, by default) in "pic.TIMERTICK"

          
### V0.2a
    - NEW: simple delay function in msec and usec; TimerDelay(<usec>, [<uflag>])
    - SD-CARD FUNCTIONS:
        - NEW: check card insertion: CardDetect()
        - NEW: mount card: CardMount()
        - NEW: find first file/dir: FindFirst(<name>, <type>)
        - NEW: find next file/dir: FindNext()
        - NEW: almost complete (but yet not fully tested) IOLIB sd-card integration;
                 reading, writing, seeking, ..., can all be done through Lua's iolib
    - NEW: interface to IOLIB's console interface. E.g.: io.write, io.read, ...

          
### V0.2b
    - NEW: configuration bits now in code (by popular demand ;-)
    - FIX: PortDir() now returns the correct values (~TRISx)
    - AD CONVERTER:
         - NEW: select analog input pins; ADPins(<state>)
         - NEW: select multiplexer input pins; ADMux(<muxa_pos>,<muxb_pos>,<muxa_neg>,<muxb_neg>)
         - NEW: select Vref; ADRef(<vrefcfg>)
         - NEW: start conversion and read result; <adval>=ADRead()

          
### V0.2c
    - NEW: limited support for "dofile()" and "loadfile()" (no real error checks)
    - FIX: wrong buffer index in asUART1.c (UART1_RXBUFSIZE replaces RXBUFSIZE)
    - FIX: stupid buffer overrun error in asUART1.c and asCON.c

          
### V0.3a
    - FIX: support for C32 v2.01
    - CHG: CardMount() does not check for card insertion anymore
    - CHG: minor code cleanups (replaced default 'stderr' output by 'stdin', ...)

          
### V0.4a
    - NEW: support for chipKIT-Max32
    - NEW: console and serial port UARTs can now be swapped
    - FIX: minor cleanups (still a LOT to do ;-)


### V0.4b
    - NEW: SD-card boot file support; boots function "BOOT32()" in file "BOOT32.LUA"
    - NEW: added SPI pin documentation
    - FIX: corrected pins for SPI1  
      SD-card now works on both SPI interfaces  


Have fun  
FMMT666(ASkr)  


[1]: http://www.askrprojects.net/software/pic32lua/index.html

