
#include <p32xxxx.h>

#include <stdio.h>
#include <stdlib.h>
#include <plib.h>


#ifndef __asCARD
#define __asCARD
#endif

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#include "asPIC.h"

#include "FSIO.h"

#include "asIO.h"
#include "asDBG.h"
#include "asCON.h"
#include "asLUA_PIC.h"
#include "asCARD.h"







#ifdef USE_SDCARD_CODE


unsigned gCardState=0;

SearchRec CardRec;




//**************************************************************************************
//*** asCARD_Detect
//***
//*** 
//**************************************************************************************
int asCARD_Detect()
{
	if(MDD_MediaDetect())
	{
		gCardState |= CARD_DETECTED;
		return 1;
	}
	else
	{
		gCardState &= ~CARD_DETECTED;
		return 0;
	}
}



//**************************************************************************************
//*** asCARD_Mount
//***
//*** 
//**************************************************************************************
int asCARD_Mount()
{
	// changed default behaviour
/*
	if(asCARD_Detect())
	{
*/		
		if(FSInit())
		{
			gCardState|=CARD_MOUNTED;
			return 1;
		}
		else
		{
			gCardState&=~CARD_MOUNTED;
			return 0;
		}
/*		
	}
	else
		return 0;
*/	
}



//**************************************************************************************
//*** asCARD_FindFirst
//***
//***
//*** type: 0=files, 1=dirs; 2=files+dirs
//*** returns 0 if file found
//**************************************************************************************
int asCARD_FindFirst(char *fname, unsigned int type)
{
	// this is not really failsafe...
	if( gCardState & CARD_MOUNTED )
	  return FindFirst(fname, type, &CardRec);
	else
		return -1;
	
}



//**************************************************************************************
//*** asCARD_FindNext
//***
//*** returns 0 if file found
//**************************************************************************************
int asCARD_FindNext()
{
	// this is not really failsafe...
	if(gCardState&CARD_MOUNTED)
	  return FindNext(&CardRec);
	else
		return -1;
}





//**************************************************************************************
//*** asCARD_Boot
//***
//*** 
//**************************************************************************************
int asCARD_Boot(lua_State *L)
{
	int boot32 = 0;

	if( asCARD_Mount() )
	{
	
		if( asCARD_FindFirst("BOOT32.LUA",ATTR_READ_ONLY|ATTR_HIDDEN|ATTR_SYSTEM|ATTR_VOLUME|ATTR_ARCHIVE) == 0 )
		{
			// TODO: SD-card boot file errors
			lua_getglobal(L, "dofile");
			lua_pushstring(L,"BOOT32.LUA");
      if (lua_pcall(L, 1, 0, 0) != 0)
				boot32 |= CARD_BOOT_ERR_NORUN;
			else
			{
				lua_getglobal(L, "BOOT32");
	      if (lua_pcall(L, 0, 0, 0) != 0)
					boot32 |= CARD_BOOT_ERR_NOFCT;
			}
			
		}
		else
			boot32 |= CARD_BOOT_ERR_NOFILE;
			
	}// unable to mount card
	else
		boot32 |= CARD_BOOT_ERR_NOMOUNT;

	return boot32;
}


/*
int asCARD_Boot(lua_State *asL)
{
	int boot32 = 0;

	if( asCARD_Mount() )
	{
	
		if( asCARD_FindFirst("BOOT32.LUA",ATTR_READ_ONLY|ATTR_HIDDEN|ATTR_SYSTEM|ATTR_VOLUME|ATTR_ARCHIVE) == 0 )
		{
			// TODO: SD-card boot file errors
			lua_getglobal(asL, "dofile");
			lua_pushstring(asL,"BOOT32.LUA");
      if (lua_pcall(asL, 1, 0, 0) != 0)
				boot32 |= CARD_BOOT_ERR_NORUN;
			else
			{
				lua_getglobal(asL, "BOOT32");
	      if (lua_pcall(asL, 0, 0, 0) != 0)
					boot32 |= CARD_BOOT_ERR_NOFCT;
			}
			
		}
		else
			boot32 |= CARD_BOOT_ERR_NOFILE;
			
	}// unable to mount card
	else
		boot32 |= CARD_BOOT_ERR_NOMOUNT;

	return boot32;
}
*/


#endif // USE SD CODE

