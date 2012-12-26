
#include <p32xxxx.h>


//--------------------------------------------------------------------------------------
// DEFINE CONFIG BITS
// (processor selection still derived from MPLab)
#if defined(__32MX795F512L__)

	// required settings:
	//   - system clock     80MHz
	//   - peripheral clock 40MHz
	//   - turn off the watchdog
	//   - ...

	#pragma config FPLLMUL  = MUL_20        // PLL Multiplier
	#pragma config FPLLIDIV = DIV_2         // PLL Input Divider
	#pragma config FPLLODIV = DIV_1         // PLL Output Divider
	#pragma config FPBDIV   = DIV_2         // Peripheral Clock divisor
	#pragma config FWDTEN   = OFF           // Watchdog Timer
	#pragma config WDTPS    = PS1           // Watchdog Timer Postscale
	#pragma config FCKSM    = CSDCMD        // Clock Switching & Fail Safe Clock Monitor
	#pragma config OSCIOFNC = OFF           // CLKO Enable
	#pragma config POSCMOD  = XT            // Primary Oscillator
	#pragma config IESO     = OFF           // Internal/External Switch-over
	#pragma config FSOSCEN  = OFF           // Secondary Oscillator Enable (KLO was off)
	#pragma config FNOSC    = PRIPLL        // Oscillator Selection
	#pragma config CP       = OFF           // Code Protect
	#pragma config BWP      = OFF           // Boot Flash Write Protect
	#pragma config PWP      = OFF           // Program Flash Write Protect
	#pragma config ICESEL   = ICS_PGx2      // ICE/ICD Comm Channel Select
	#pragma config DEBUG    = OFF           // Background Debugger Enable
#else
	#error "As of now, all the config bits are set in the code, but you"
	#error "are obviously not using a 795F512L. Check settings!"
#endif
//--------------------------------------------------------------------------------------




#include <stdio.h>
#include <stdlib.h>
#include <plib.h>


#ifndef __asPIC
#define __asPIC
#endif

#include "asPIC.h"

#include "FSIO.h"

#include "asIO.h"
#include "asDBG.h"
#include "asCON.h"




//--------------------------------------------------------------------------------------
// SOME GLOBAL VARS

// Upper 32 bits of the internal 64 bit timer.
// 'gTimerHigh' increments on every Timer45 overflow (~430s).
// Now, we have a 1.845e12s timer (=58494 years)
// Should be sufficient ;)
volatile int gTimerHigh;

static const unsigned long PIC_TRISx[PIC_MAXPORTS]={PIC_TRISA, PIC_TRISB, PIC_TRISC, PIC_TRISD, PIC_TRISE, PIC_TRISF, PIC_TRISG, PIC_TRISH};
static const unsigned long PIC_PORTx[PIC_MAXPORTS]={PIC_PORTA, PIC_PORTB, PIC_PORTC, PIC_PORTD, PIC_PORTE, PIC_PORTF, PIC_PORTG, PIC_PORTH};
static const unsigned long PIC_LATx[PIC_MAXPORTS]={PIC_LATA, PIC_LATB, PIC_LATC, PIC_LATD, PIC_LATE, PIC_LATF, PIC_LATG, PIC_LATH};
static const unsigned long PIC_LATxSET[PIC_MAXPORTS]={PIC_LATASET, PIC_LATBSET, PIC_LATCSET, PIC_LATDSET, PIC_LATESET, PIC_LATFSET, PIC_LATGSET, PIC_LATHSET};
static const unsigned long PIC_LATxCLR[PIC_MAXPORTS]={PIC_LATACLR, PIC_LATBCLR, PIC_LATCCLR, PIC_LATDCLR, PIC_LATECLR, PIC_LATFCLR, PIC_LATGCLR, PIC_LATHCLR};
static const unsigned long PIC_LATxINV[PIC_MAXPORTS]={PIC_LATAINV, PIC_LATBINV, PIC_LATCINV, PIC_LATDINV, PIC_LATEINV, PIC_LATFINV, PIC_LATGINV, PIC_LATHINV};






//**************************************************************************************
//*** asPIC_Init
//***
//*** Minimal PIC32 initialization.
//*** Because different PICs require different settings, I left out the config
//*** bits from the code. This allows much quicker changes...
//***
//***  - PIC should be running on 80MHz
//***  - peripheral clock should be 40MHz
//**************************************************************************************
void asPIC_Init()
{

	// CPU stuff
	// default is 2 flash waitstates for 80MHz operation
	SYSTEMConfig(SYS_FREQ, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE);
	mOSCSetPBDIV(OSC_PB_DIV_2);
	
	
#ifndef CONSOLE_SWAP

	// MAIN CONSOLE ON UART2; 115k, 8N1
	U2MODE = 0b1000100000000000;
	U2STAbits.UTXEN = 1;
	U2STAbits.URXEN = 1;
	U2BRG=(PER_FREQ/(16*CONSOLE_BAUD))-1;
	
#else	

	// MAIN CONSOLE ON UART1; 115k, 8N1
	U1MODE = 0b1000100000000000;
	U1STA = 0b0000000000000000100010000;
	U1STAbits.UTXEN = 1;
	U1STAbits.URXEN = 1;
	U1BRG=(PER_FREQ/(16*CONSOLE_BAUD))-1;

#endif

	// AD CONVERTER
	AD1PCFG=0xffff;								// digital pins
	
	// TOCHK
	// C32 V2.00
//	AD1CON1bits.FRZ=1;						// freeze on exception


	AD1CON1bits.SIDL=0;						// continue in idle mode
	AD1CON1bits.FORM=0b100;				// unsigned, 10bit output
	AD1CON1bits.SSRC=0;						// manual mode (SAMP bit)
	AD1CON1bits.CLRASAM=0;				// do not stop conv.
	AD1CON1bits.ASAM=1;						// auto sampling mode
	AD1CON1bits.SAMP=1;						// start sampling			

	AD1CON2bits.VCFG=0b000;				// use AVdd and AVss as reference
	AD1CON2bits.OFFCAL=0;					// no calib.
	AD1CON2bits.CSCNA=0;					// no scan
	AD1CON2bits.SMPI=0;						// int. if conv. complete			
	AD1CON2bits.BUFM=0;						// 16 bit buffer
	AD1CON2bits.ALTS=0;						// use MUX A settings

	AD1CON3bits.ADRC=0;						// AD clock from periph. clk.
	AD1CON3bits.SAMC=1;						// only 1 TAD (sampling all the time)
	AD1CON3bits.ADCS=1;						// TAD=100ns (@40MHz peripheral clock)
	
	AD1CHS=0;											// MUX A/B pos = AN0; neg = VR-

	AD1CSSL=0;										// no scan (just in case)	
	
	AD1CON1bits.ON=1;							// on (no need to save power)
	
	// TIMER 4/5
	// With a 40MHz peripheral clock and a 1/4 ratio, this timer
	// increments every 100ns.
	// 100ns * 2^32 = ~430s (overflow)
	// forms a 64 bit timer with 'gTimerHigh' (overflows every 58494 years ;-)

	OpenTimer4(T4_ON|
							T4_IDLE_CON|
							T4_GATE_OFF|
							T4_PS_1_4|				// <- changing this requires changing "TIMER45_TICK"
							T4_32BIT_MODE_ON|
							T4_SOURCE_INT,
							0xffffffff);

	// INTERRUPTS
	INTEnableSystemMultiVectoredInt();
  mT4ClearIntFlag(); 
  mT5ClearIntFlag(); 
  mU2RXClearIntFlag();
  mU1RXClearIntFlag();
  
  ConfigIntTimer4(T5_INT_ON|T5_INT_PRIOR_4);

  
#ifndef CONSOLE_SWAP

  ConfigIntUART2(UART_RX_INT_EN|UART_INT_PR2);
  
#else

  ConfigIntUART1(UART_RX_INT_EN|UART_INT_PR2);

#endif


	INTEnableInterrupts();	

	
	// OTHER STUFF
	asCON_RXBufInit();	// initialize receive buffer
	gTimerHigh=0;				// clear upper 32 bits of the internal 64 bit timer
	

#ifdef PIC32_DEBUG
	// ---- DEBUG STUFF BELOW ----
	// only a few test pins
	asDBG_Pin1Init();
	asDBG_Pin1(0);
	asDBG_Pin2Init();
	asDBG_Pin2(0);
#endif	


}




//**************************************************************************************
//*** asPIC_Port_Dir
//***
//*** Sets or reads the direction state of the TRISx registers.
//*** It's up to the Lua user to make sense of this non-atomic bit fiddling,
//*** which might cause unexpected results if bidirectional pins are involved...
//***
//*** <port> : TRISx register; 0->A, 1->B...
//*** <dir>  : new 16 bit direction state
//*** <mask> : only <dir> bits which have <mask> set to 1 will be updated
//***          if <mask> == 0, nothing is updated, but the register content is returned.
//*** returns: -1 if error (port does not exist)
//***          16 bit port dir otherwise
//**************************************************************************************
int asPIC_Port_Dir(int port, unsigned int dir, unsigned int mask)
{
	unsigned long *pReg;
	int value=0;

	if((port<0)||(port>=PIC_MAXPORTS))
		return -1;

	pReg=(unsigned long*)PIC_TRISx[(int)port];

	if(pReg==PIC_UNUSED)
		return -1;
		
	value=(*pReg)&0xffff;
	
	if(mask==0)
		return (~value)&0xffff;

	dir=~dir;
	dir&=0xffff;
	mask&=0xffff;
	
	// intentionally does not use SET or CLR registers
	// (only ONE register write)
	value|=(dir & mask);               // set "ones"
	value&=(dir | ((~mask)&0xffff) );  // set "zeros"
	
	// all changes in one instruction
	*pReg=value;	

	return (~value) & 0xffff;
}



//**************************************************************************************
//*** asPIC_Port_State
//***
//*** Sets or reads the ports.
//*** It's up to the Lua user to make sense of this non-atomic bit fiddling,
//*** which might cause unexpected results if bidirectional pins are involved...
//***
//*** Note: This is a read-modify-write function!
//***
//*** <port> : PORTx or LATx register; 0->A, 1->B...
//*** <pins> : new 16 bit pin state
//*** <mask> : only <dir> bits which have <mask> set to 1 will be updated
//***          if <mask> == 0, nothing is updated, but the register content is returned.
//*** returns: -1 if error (port does not exist)
//***          16 bit port dir otherwise
//**************************************************************************************
int asPIC_Port_State(int port, int pins, int mask)
{
	unsigned long *pReg;
	int value=0;

	if((port<0)||(port>=PIC_MAXPORTS))
		return -1;

	pReg=(unsigned long *)PIC_PORTx[(int)port];

	if(pReg==PIC_UNUSED)
		return -1;
		
	value=(*pReg)&0xffff;
	
	if(mask==0)
		return value;

	pins&=0xffff;
	mask&=0xffff;

	value|=(pins & mask);               // set "ones"
	value&=(pins | ((~mask)&0xffff) );  // set "zeros"
	
	// all changes in one instruction
	*pReg=value;	

	return value & 0xffff;
}


//**************************************************************************************
//*** asPIC_Port_Latch
//***
//*** Unconditionally sets pins on a port (writes to latch)
//***
//*** <port> : LATx register; 0->A, 1->B...
//*** <state>: 16 bit port state
//*** returns: -1 if error (port does not exist)
//***           0 otherwise
//**************************************************************************************
int asPIC_Port_Latch(int port, int state)
{
	unsigned long *pReg;

	if((port<0)||(port>=PIC_MAXPORTS))
		return -1;

	pReg=(unsigned long *)PIC_LATx[(int)port];

	if(pReg==PIC_UNUSED)
		return -1;
		
	*pReg=state;

	return 0;
}



//**************************************************************************************
//*** asPIC_Port_SetPins
//***
//*** Unconditionally sets pins on a port.
//***
//*** <port> : LATx register; 0->A, 1->B...
//*** <pins> : 16 bit pin mask; every '1' sets the according pin to a high state
//*** returns: -1 if error (port does not exist)
//***           0 otherwise
//**************************************************************************************
int asPIC_Port_SetPins(int port, int pins)
{
	unsigned long *pReg;

	if((port<0)||(port>=PIC_MAXPORTS))
		return -1;

	pReg=(unsigned long *)PIC_LATxSET[(int)port];

	if(pReg==PIC_UNUSED)
		return -1;
		
	*pReg=pins;

	return 0;
}


//**************************************************************************************
//*** asPIC_Port_ClrPins
//***
//*** Unconditionally clears pins on a port.
//***
//*** <port> : LATx register; 0->A, 1->B...
//*** <pins> : 16 bit pin mask; every '1' sets the according pin to a low state
//*** returns: -1 if error (port does not exist)
//***           0 otherwise
//**************************************************************************************
int asPIC_Port_ClrPins(int port, int pins)
{
	unsigned long *pReg;

	if((port<0)||(port>=PIC_MAXPORTS))
		return -1;

	pReg=(unsigned long *)PIC_LATxCLR[(int)port];

	if(pReg==PIC_UNUSED)
		return -1;
		
	*pReg=pins;

	return 0;
}


//**************************************************************************************
//*** asPIC_Port_InvPins
//***
//*** Unconditionally inverts pins on a port.
//***
//*** <port> : LATx register; 0->A, 1->B...
//*** <pins> : 16 bit pin mask; every '1' inverts the according pin state
//*** returns: -1 if error (port does not exist)
//***           0 otherwise
//**************************************************************************************
int asPIC_Port_InvPins(int port, int pins)
{
	unsigned long *pReg;

	if((port<0)||(port>=PIC_MAXPORTS))
		return -1;

	pReg=(unsigned long *)PIC_LATxINV[(int)port];

	if(pReg==PIC_UNUSED)
		return -1;
		
	*pReg=pins;

	return 0;
}


//**************************************************************************************
//*** asPIC_Timer_Read
//***
//*** Returns the current value of the internal 64bit timer.
//**************************************************************************************
unsigned long long asPIC_Timer_Read()
{
	unsigned i=gTimerHigh;
	
	// ASkr C32 1.13 adaption
//	unsigned j=TMR45;
	unsigned j=TMR4;
	
	// did an overflow occur between i and j, above?
	if(gTimerHigh > i)
	{
	// ASkr C32 1.13 adaption
//		j=TMR45;
		j=TMR4;
		i=gTimerHigh;
	}
	
	return (unsigned long long)i<<32 | j;
}



//**************************************************************************************
//*** asPIC_Timer_Match
//***
//*** Blocks until the 64 bit timer value matches (or "passes") <match>
//**************************************************************************************
int asPIC_Timer_Match(unsigned long long match, int blocked)
{
	if(blocked == BLOCKING)
	{
		while(asPIC_Timer_Read() < match)
		{;}
		return 1;
	}
	else
	{
		if(asPIC_Timer_Read() < match)
			return 0;
		else
			return 1;
	}
}



//**************************************************************************************
//*** asPIC_AD_Mux
//***
//*** Sets the AD converter multiplexer.
//*** Returns -1 on error
//**************************************************************************************
int asPIC_AD_Mux(unsigned muxapos, unsigned muxbpos, unsigned muxaneg, unsigned muxbneg)
{
	if((muxapos > 15)||(muxbpos > 15))
		return -1;

	AD1CON1bits.ON=0;

	if(muxaneg > 0)
		muxaneg=1;
	if(muxbneg > 0)
		muxbneg=1;
		
	AD1CHSbits.CH0SA=muxapos;
	AD1CHSbits.CH0SB=muxbpos;
	AD1CHSbits.CH0NA=muxaneg;
	AD1CHSbits.CH0NB=muxbneg;

	AD1CON1bits.ON=1;

	return 0;	
}



//**************************************************************************************
//*** asPIC_AD_Read
//***
//*** Starts converseion and returns the result
//**************************************************************************************
int asPIC_AD_Read()
{
	// mhh...
	// might the 5 stage pipeline cause problems with DONE=0
	// and querying it < 3 cycles?
/*

	looks like it is safe ;-)
	
473:                 	AD1CON1bits.DONE=0;
9D01F2F8  3C04BF81   lui         a0,0xbf81
9D01F2FC  90829000   lbu         v0,-28672(a0)
9D01F300  2403FFFE   addiu       v1,zero,-2
9D01F304  00431024   and         v0,v0,v1
9D01F308  A0829000   sb          v0,-28672(a0)
474:                   AD1CON1bits.SAMP=0;
9D01F30C  90829000   lbu         v0,-28672(a0)
9D01F310  2403FFFD   addiu       v1,zero,-3
9D01F314  00431024   and         v0,v0,v1
9D01F318  A0829000   sb          v0,-28672(a0)
9D01F31C  00801821   addu        v1,a0,zero
475:                   while(!AD1CON1bits.DONE)
9D01F320  8C629000   lw          v0,-28672(v1)
9D01F324  7C420000   ext         v0,v0,0,1
9D01F328  1040FFFD   beq         v0,zero,0x9d01f320
9D01F32C  00000000   nop         
*/	
	
	AD1CON1bits.DONE=0;
  AD1CON1bits.SAMP=0;
  while(!AD1CON1bits.DONE)
  {;}
  return ADC1BUF0 & 0x3ff;
}


