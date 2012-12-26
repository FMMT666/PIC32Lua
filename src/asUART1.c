
#include <p32xxxx.h>

#include <stdio.h>
#include <stdlib.h>
#include <plib.h>


#ifndef __asUART1
#define __asUART1
#endif

#include "asPIC.h"

#include "asIO.h"
#include "asUART1.h"
#include "asDBG.h"
#include "asCON.h"




// The dynamically handled buffer, with auto-size-adaption,
// was a nice idea, but too slow.
// Back to the easy stuff, for now...
// May the future bring DMA :)
volatile char pUART1_RXBuf[UART1_RXBUFSIZE];
unsigned UART1_Enabled=0;						// UART1 enable flag
volatile unsigned pUART1_RXBufW=0;	// write buffer index
volatile unsigned pUART1_RXBufR=0;	// read buffer index
volatile unsigned pUART1_RXBufC=0;	// number of bytes in buffer
volatile unsigned pUART1_RXBufO=0;	// overflow flag




//**************************************************************************************
//*** asUART1_Enable
//***
//***
//**************************************************************************************
void asUART1_Enable(unsigned baud)
{
	
	asUART1_RXBufInit();


#ifndef CONSOLE_SWAP
	// use UART1
	U1MODE = 0b1000100000000000;
	U1STAbits.UTXEN=1;
	U1STAbits.URXEN=1;
	asUART1_Baud(baud);

	// enable UART1 RX interrupt
  mU1RXClearIntFlag();
  ConfigIntUART1(UART_RX_INT_EN|UART_INT_PR2);
  
  // mark settings
  UART1_Enabled=1;

#else
	// use UART2
	U2MODE = 0b1000100000000000;
	U2STAbits.UTXEN=1;
	U2STAbits.URXEN=1;
	asUART1_Baud(baud);

	// enable UART1 RX interrupt
  mU2RXClearIntFlag();
  ConfigIntUART2(UART_RX_INT_EN|UART_INT_PR2);
  
  // mark settings
  UART1_Enabled = 1;
 #endif
}



//**************************************************************************************
//*** asUART1_Disable
//***
//***
//**************************************************************************************
void asUART1_Disable()
{
	//
  UART1_Enabled = 0;

#ifndef CONSOLE_SWAP

	// disable RX interrupt
  ConfigIntUART1(0);
  mU1RXClearIntFlag();

	// turn UART1 off
	U1MODEbits.ON = 0;
	U1STAbits.UTXEN = 0;
	U1STAbits.URXEN = 0;
	
#else

	// disable RX interrupt
  ConfigIntUART2(0);
  mU2RXClearIntFlag();

	// turn UART1 off
	U2MODEbits.ON = 0;
	U2STAbits.UTXEN = 0;
	U2STAbits.URXEN = 0;

#endif	

	// clear RX buffer
	asUART1_RXBufInit();

}



//**************************************************************************************
//*** asUART1_Baud
//***
//***
//**************************************************************************************
void asUART1_Baud(unsigned baud)
{
	if(baud < 110)
		baud=110;
	else
	{
		if(baud>500000L)
			baud=500000L;
	}
	
#ifndef CONSOLE_SWAP

	U1BRG=(PER_FREQ/(16*baud))-1;
	
#else

	U2BRG=(PER_FREQ/(16*baud))-1;

#endif
}


//**************************************************************************************
//*** asUART1_RXBufInit
//***
//***
//**************************************************************************************
inline void asUART1_RXBufInit()
{
	pUART1_RXBufW=0;
	pUART1_RXBufR=0;
	pUART1_RXBufC=0;
}


//**************************************************************************************
//*** asUART1_RXBufWriteChar
//***
//*** Called from interrupt routine.
//**************************************************************************************
int asUART1_RXBufWriteChar(char ch)
{
	if(!UART1_Enabled)
		return 0;
	
	// check end of ringbuffer
	if( ++pUART1_RXBufW >= UART1_RXBUFSIZE )
		pUART1_RXBufW = 0;

	// check overflow
	if(pUART1_RXBufW == pUART1_RXBufR )
	{
		asUART1_RXBufInit();
		pUART1_RXBufO = 1;
		return -1;
	}

	pUART1_RXBuf[pUART1_RXBufW]=ch;
	pUART1_RXBufC++;
	
	return 1;
}


//**************************************************************************************
//*** asUART1_RXBufReadChar
//***
//*** inline? well... ;)
//**************************************************************************************
inline int asUART1_RXBufReadChar(int blocked)
{
	if( !UART1_Enabled )
		return RXBUFEMPTY;
	
	if( blocked )
	{
		while ( pUART1_RXBufW == pUART1_RXBufR )
		{;}
	}
	else
	{
		if( pUART1_RXBufW == pUART1_RXBufR )
			return RXBUFEMPTY;
	}
	
	if( ++pUART1_RXBufR >= UART1_RXBUFSIZE )
		pUART1_RXBufR = 0;

	// TOCHK!
	// The RX interrupt routine might interfere right here!
	if( pUART1_RXBufC > 0 )
		pUART1_RXBufC--;
	return pUART1_RXBuf[pUART1_RXBufR];
}


//**************************************************************************************
//*** asUART1_RXBufFlushIn
//***
//*** Flushes the RX input buffer.
//*** There are faster ways, indeed, but this one is the safe method while the RX
//*** interrupt is active...
//**************************************************************************************
inline void asUART1_RXBufFlushIn()
{
	if(!UART1_Enabled)
		return;
	
	while(asUART1_RXBufReadChar(NONBLOCKING) >= 0)
	{;}
	
	pUART1_RXBufO=0;
}



//**************************************************************************************
//*** asUART1_RXBufCount
//***
//***
//*** Returns the number of bytes in the receive buffer.
//*** If an overflow occured, '-1' will be returned.
//**************************************************************************************
inline unsigned asUART1_RXBufCount()
{
	// TODO:
	// "-1" ??? ;-)
	
	if(pUART1_RXBufO)
		return -1;
	else
		return pUART1_RXBufC;
}




//**************************************************************************************
//*** asPIC_UART1_FlushOut
//***
//***
//**************************************************************************************
inline void asUART1_FlushOut()
{
	if(!UART1_Enabled)
		return;

#ifndef CONSOLE_SWAP

	while(U1STAbits.UTXBF)
	{;}
	
#else

	while(U2STAbits.UTXBF)
	{;}

#endif
}



//**************************************************************************************
//*** asUART1_WriteString
//***
//*** Sends a string over UART1.
//**************************************************************************************
void asUART1_WriteString(char *ch)
{
	if(!UART1_Enabled)
		return;

	if(ch==NULL)
		return;
	
	while(*ch!=0)
		asUART1_WriteChar(*ch++);
}



//**************************************************************************************
//*** asUART1_WriteChar
//***
//*** Sends a single character over UART1.
//**************************************************************************************
inline void asUART1_WriteChar(char ch)
{
	if(!UART1_Enabled)
		return;

	asUART1_FlushOut();
	
#ifndef CONSOLE_SWAP

	U1TXREG=ch;
	
#else

	U2TXREG=ch;

#endif
}


