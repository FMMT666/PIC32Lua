
#include <p32xxxx.h>


//--------------------------------------------------------------------------------------
// DEFINE SYSTEM AND PERIPHERAL FREQUENCY
// Note #1: This is used to determine or measure "time"
//          (e.g.: UART speed, timers, ...)
// Note #2: This can NOT be used to change peripheral clock frequency!
//          Additionally, changing these values might require changing
//          some configuration bits or other stuff...
#define SYS_FREQ 80000000L
#define PER_FREQ 40000000L


//--------------------------------------------------------------------------------------
// DEFINE CONSOLE BAUDRATE AND PORT (UART)
// Note #1: By default, the console uses UART2 and the PIC32Lua configurable
//          serial interface is routed to UART1.
//          This can be changed by setting CONSOLE_SWAP
//            console         -> UART1
//            PIC32Lua-Serial -> UART2
//          and is the default setting for the Chipkit-Max32.
#define CONSOLE_BAUD 115200
#if defined(CHIPKITMAX32)
	// set Chipkit-Max32 behaviour
	#define CONSOLE_SWAP
#else
	// set behaviour on other hardware
//	#define CONSOLE_SWAP
#endif


//--------------------------------------------------------------------------------------
// DEFINE AVAILABLE TRISx REGISTERS AND PERMISSIONS (FROM WITHIN LUA)
// undefine any TRIS registers that should not be available from within Lua
// (or do not exist for your processor)
#define PIC_MAXPORTS 8
#define PIC_UNUSED   NULL	  // <- do not change this

#define PIC_TRISA (unsigned long)&TRISA
#define PIC_TRISB (unsigned long)&TRISB
#define PIC_TRISC (unsigned long)&TRISC
#define PIC_TRISD (unsigned long)&TRISD
#define PIC_TRISE (unsigned long)&TRISE
#define PIC_TRISF (unsigned long)&TRISF
#define PIC_TRISG (unsigned long)PIC_UNUSED
#define PIC_TRISH (unsigned long)PIC_UNUSED

//static const unsigned long PIC_TRISx[PIC_MAXPORTS]={PIC_TRISA, PIC_TRISB, PIC_TRISC, PIC_TRISD, PIC_TRISE, PIC_TRISF, PIC_TRISG, PIC_TRISH};

//--------------------------------------------------------------------------------------
// DEFINE AVAILABLE PORTx REGISTERS AND PERMISSIONS (FROM WITHIN LUA)
#define PIC_PORTA (unsigned long)&PORTA
#define PIC_PORTB (unsigned long)&PORTB
#define PIC_PORTC (unsigned long)&PORTC
#define PIC_PORTD (unsigned long)&PORTD
#define PIC_PORTE (unsigned long)&PORTE
#define PIC_PORTF (unsigned long)&PORTF
#define PIC_PORTG (unsigned long)PIC_UNUSED
#define PIC_PORTH (unsigned long)PIC_UNUSED

//static const unsigned long PIC_PORTx[PIC_MAXPORTS]={PIC_PORTA, PIC_PORTB, PIC_PORTC, PIC_PORTD, PIC_PORTE, PIC_PORTF, PIC_PORTG, PIC_PORTH};

//--------------------------------------------------------------------------------------
// DEFINE AVAILABLE LATx REGISTERS AND PERMISSIONS (FROM WITHIN LUA)
#define PIC_LATA (unsigned long)&LATA
#define PIC_LATB (unsigned long)&LATB
#define PIC_LATC (unsigned long)&LATC
#define PIC_LATD (unsigned long)&LATD
#define PIC_LATE (unsigned long)&LATE
#define PIC_LATF (unsigned long)&LATF
#define PIC_LATG (unsigned long)PIC_UNUSED
#define PIC_LATH (unsigned long)PIC_UNUSED

//static const unsigned long PIC_LATx[PIC_MAXPORTS]={PIC_LATA, PIC_LATB, PIC_LATC, PIC_LATD, PIC_LATE, PIC_LATF, PIC_LATG, PIC_LATH};

//--------------------------------------------------------------------------------------
// DEFINE AVAILABLE LATxSET REGISTERS AND PERMISSIONS (FROM WITHIN LUA)
#define PIC_LATASET (unsigned long)&LATASET
#define PIC_LATBSET (unsigned long)&LATBSET
#define PIC_LATCSET (unsigned long)&LATCSET
#define PIC_LATDSET (unsigned long)&LATDSET
#define PIC_LATESET (unsigned long)&LATESET
#define PIC_LATFSET (unsigned long)&LATFSET
#define PIC_LATGSET (unsigned long)PIC_UNUSED
#define PIC_LATHSET (unsigned long)PIC_UNUSED

//static const unsigned long PIC_LATxSET[PIC_MAXPORTS]={PIC_LATASET, PIC_LATBSET, PIC_LATCSET, PIC_LATDSET, PIC_LATESET, PIC_LATFSET, PIC_LATGSET, PIC_LATHSET};

//--------------------------------------------------------------------------------------
// DEFINE AVAILABLE LATxCLR REGISTERS AND PERMISSIONS (FROM WITHIN LUA)
#define PIC_LATACLR (unsigned long)&LATACLR
#define PIC_LATBCLR (unsigned long)&LATBCLR
#define PIC_LATCCLR (unsigned long)&LATCCLR
#define PIC_LATDCLR (unsigned long)&LATDCLR
#define PIC_LATECLR (unsigned long)&LATECLR
#define PIC_LATFCLR (unsigned long)&LATFCLR
#define PIC_LATGCLR (unsigned long)PIC_UNUSED
#define PIC_LATHCLR (unsigned long)PIC_UNUSED

//static const unsigned long PIC_LATxCLR[PIC_MAXPORTS]={PIC_LATACLR, PIC_LATBCLR, PIC_LATCCLR, PIC_LATDCLR, PIC_LATECLR, PIC_LATFCLR, PIC_LATGCLR, PIC_LATHCLR};

//--------------------------------------------------------------------------------------
// DEFINE AVAILABLE LATxINV REGISTERS AND PERMISSIONS (FROM WITHIN LUA)
#define PIC_LATAINV (unsigned long)&LATAINV
#define PIC_LATBINV (unsigned long)&LATBINV
#define PIC_LATCINV (unsigned long)&LATCINV
#define PIC_LATDINV (unsigned long)&LATDINV
#define PIC_LATEINV (unsigned long)&LATEINV
#define PIC_LATFINV (unsigned long)&LATFINV
#define PIC_LATGINV (unsigned long)PIC_UNUSED
#define PIC_LATHINV (unsigned long)PIC_UNUSED

//static const unsigned long PIC_LATxINV[PIC_MAXPORTS]={PIC_LATAINV, PIC_LATBINV, PIC_LATCINV, PIC_LATDINV, PIC_LATEINV, PIC_LATFINV, PIC_LATGINV, PIC_LATHINV};



//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// no changes below this line

// the default timer45 tick-time (100ns)
#define TIMER45_TICK 4.0/PER_FREQ
#define GetSystemClock()            (SYS_FREQ)
#define GetPeripheralClock()        (PER_FREQ) 
#define GetInstructionClock()       (SYS_FREQ)


//--------------------------------------------------------------------------------------
// THE REST
#ifndef __asPIC

	extern volatile int gTimerHigh;

	extern const unsigned long PIC_TRISx[PIC_MAXPORTS];
	extern const unsigned long PIC_PORTx[PIC_MAXPORTS];
	extern const unsigned long PIC_LATx[PIC_MAXPORTS];
	extern const unsigned long PIC_LATxSET[PIC_MAXPORTS];
	extern const unsigned long PIC_LATxCLR[PIC_MAXPORTS];
	extern const unsigned long PIC_LATxINV[PIC_MAXPORTS];

	extern void asPIC_Init();

	extern int asPIC_AD_Read(void);
	extern int asPIC_AD_Mux(unsigned muxapos, unsigned muxbpos, unsigned muxaneg, unsigned muxbneg);
	extern unsigned long long asPIC_Timer_Read(void);
	extern int asPIC_Timer_Match(unsigned long long match, int blocked);
	extern int asPIC_Port_Dir(int port, int dir, int mask);
	extern int asPIC_Port_State(int port, int pins, int mask);
	extern int asPIC_Port_Latch(int port, int state);
	extern int asPIC_Port_SetPins(int port, int pins);
	extern int asPIC_Port_ClrPins(int port, int pins);
	extern int asPIC_Port_InvPins(int port, int pins);


#else
	
	inline void asPIC_UART1_WriteChar(char ch);

#endif
