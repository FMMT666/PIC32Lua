
#include <p32xxxx.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <plib.h>


#ifndef __asINT
#define __asINT
#endif

#include "asPIC.h"

#include "asIO.h"
#include "asINT.h"
#include "asDBG.h"
#include "asCON.h"
#include "asUART1.h"


// ASkr TODO priorities




//**************************************************************************************
//*** UART1 RX INTERRUPT
//***
//*** 
//**************************************************************************************
void __ISR(_UART_1_VECTOR, ipl3) _U1RXInterrupt(void)
{
	if(mU1RXGetIntFlag())
	{
		while(U1STAbits.URXDA)
		
#ifndef CONSOLE_SWAP
			asUART1_RXBufWriteChar(U1RXREG & 0xFF);
#else
			asCON_RXBufWriteChar(U1RXREG & 0xFF);
#endif
			
		mU1RXClearIntFlag();
	}
}
//**************************************************************************************
//*** UART2 RX INTERRUPT
//***
//*** 
//**************************************************************************************
void __ISR(_UART_2_VECTOR, ipl4) _U2RXInterrupt(void)
{
	if(mU2RXGetIntFlag())
	{
		while(U2STAbits.URXDA)
		
#ifndef CONSOLE_SWAP
			asCON_RXBufWriteChar(U2RXREG & 0xFF);
#else
			asUART1_RXBufWriteChar(U2RXREG & 0xFF);
#endif

		mU2RXClearIntFlag();
	}
}



//**************************************************************************************
//*** TIMER 4/5 INTERRUPT
//***
//***
//**************************************************************************************

// stopped working with newer C32 dist (V2.02)
void __ISR(_TIMER_5_VECTOR, ipl5) _T5Interrupt(void)
{ 
	gTimerHigh++;
  mT5ClearIntFlag(); 
}

