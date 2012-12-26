
#include <p32xxxx.h>


#include <stdio.h>

#include "asIO.h"

//#include <stdlib.h>
#include <plib.h>


#ifndef __asCON
#define __asCON
#endif

#include "asPIC.h"

#include "asCON.h"
#include "asDBG.h"





// The dynamically handled buffer, with auto-size-adaption,
// was a nice idea, but too slow.
// Back to the easy stuff, for now...
// May the future bring DMA :)
char pRXBuf[RXBUFSIZE];
volatile unsigned pRXBufW=0;	// write buffer index
volatile unsigned pRXBufR=0;	// read buffer index
volatile unsigned pRXBufC=0;	// number of bytes in buffer
volatile unsigned pRXBufO=0;	// overflow flag



//**************************************************************************************
//*** asCON__RXBufInit
//***
//***
//**************************************************************************************
inline void asCON_RXBufInit()
{
	pRXBufW=0;
	pRXBufR=0;
	pRXBufC=0;
}


//**************************************************************************************
//*** asCON__RXBufWriteChar
//***
//*** Called from interrupt routine.
//**************************************************************************************
int asCON_RXBufWriteChar(char ch)
{
	// check end of ringbuffer
	if( ++pRXBufW >= RXBUFSIZE )
		pRXBufW=0;

	// check overflow
	if(pRXBufW == pRXBufR )
	{
		asCON_RXBufInit();
		pRXBufO=1;
		return -1;
	}

	pRXBuf[pRXBufW]=ch;
	pRXBufC++;
	
	return 1;
}


//**************************************************************************************
//*** asCON__RXBufReadChar
//***
//*** inline? well... ;)
//**************************************************************************************
inline int asCON_RXBufReadChar(int blocked)
{
	if(blocked)
	{
		while (pRXBufW == pRXBufR)
		{;}
	}
	else
	{
		if(pRXBufW == pRXBufR)
			return RXBUFEMPTY;
	}
	
	if( ++pRXBufR >= RXBUFSIZE )
		pRXBufR=0;

	// TOCHK!
	// The RX interrupt routine may interfere right here!
	if(pRXBufC>0)
		pRXBufC--;
	return pRXBuf[pRXBufR];
}


//**************************************************************************************
//*** asCON__RXBufFlushIn
//***
//*** Flushes the RX input buffer.
//*** There are faster ways, indeed, but this one is the safe method while the RX
//*** interrupt is active...
//**************************************************************************************
inline void asCON_RXBufFlushIn()
{
	while(asCON_RXBufReadChar(NONBLOCKING) >= 0)
	{;}
	pRXBufO=0;
}



//**************************************************************************************
//*** asCON__RXBufCount
//***
//***
//*** Returns the number of bytes in the receive buffer.
// If an overflow occured, a '-1' will be returned.
//**************************************************************************************
inline unsigned asCON_RXBufCount()
{
	if(pRXBufO)
		return -1;
	else
		return pRXBufC;
}



//**************************************************************************************
//*** asCON_SerialWriteString
//***
//*** Sends a string over UART2.
//**************************************************************************************
void asCON_SerialWriteString(char *ch)
{
	if(ch==NULL)
		return;
	
	while(*ch!=0)
		asCON_SerialWriteChar(*ch++);
}
	

//**************************************************************************************
//*** asCON_SerialFlush
//***
//*** 
//**************************************************************************************
inline void asCON_SerialFlush()
{

#ifndef CONSOLE_SWAP

	while(U2STAbits.UTXBF)
	{;}
	
#else

	while(U1STAbits.UTXBF)
	{;}

#endif
}


//**************************************************************************************
//*** asCON_SerialWriteChar
//***
//*** Sends a single character over UART2.
//*** For every '\n', an additional '\r' is sent too.
//*** (Possibly not the best solution...)
//**************************************************************************************
inline void asCON_SerialWriteChar(char ch)
{
	asCON_SerialFlush();
	
#ifndef CONSOLE_SWAP
	
	U2TXREG = ch;
	if(ch == '\r')
	{
		asCON_SerialFlush();
		U2TXREG = '\n';
	}
	
#else

	U1TXREG = ch;
	if(ch == '\r')
	{
		asCON_SerialFlush();
		U1TXREG = '\n';
	}

#endif

}


//**************************************************************************************
//*** asCON_SerialReadLine (fishy...)
//*** 
//*** Reads a line from UART buffer. End is determined by '\r' ("ENTER" ;-)
//**************************************************************************************
unsigned int asCON_SerialReadLine(unsigned int maxdat,char *buffer, unsigned int uart_data_wait)
{
	char *temp_ptr = (char *) buffer;
	int len=0;
	int i;


// ASkr DEBUG
//  can be useful to understand how Lua works ;-)
//	sprintf(buffer,"print(\"kekse\")\r");
//	sprintf(buffer,"io.write(\"kekse\")\r");
//	return 1;

  while(len < maxdat)       
  {
    *temp_ptr = asCON_RXBufReadChar(BLOCKING);
    
// sucks, but works ;-)
#ifdef USE_DIRTY_BACKSPACE    

    if(*(temp_ptr) == KEY_BACKSPACE)
    {
			asCON_SerialWriteChar('\r');
	    temp_ptr--;
	    *temp_ptr=0;
	    if(len>0)
	    	len--;
	    for(i=0;i<len;i++)
		    asCON_SerialWriteChar(*(buffer+i));
	    continue;
    }
#endif

    temp_ptr++;
    len++;

		asCON_SerialWriteChar(*(temp_ptr-1));

    if (*(temp_ptr-1) == '\r')
    {
	    *temp_ptr=0;
      return len+1;
    }
  }// END while
  return(maxdat);
}

