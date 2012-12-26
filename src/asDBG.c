
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#ifndef __asDBG
#define __asDBG
#endif


#include "asPIC.h"

#include "asDBG.h"
#include "asCON.h"


// ADDITIONAL STUFF (MACROS) IN THE HEADER FILE!


#ifdef PIC32_DEBUG



//**************************************************************************************
//*** asDBG_DumpReg
//*** 
//*** Dumps the contents of <reg> (to wherever)
//**************************************************************************************
void asDBG_DumpReg(unsigned int reg)
{
	char tmp[11];

	sprintf((char *)&tmp,"%08x\n",reg);
	asCON_SerialWriteString((char *)&tmp);
}













#endif // PIC32DEBUG

