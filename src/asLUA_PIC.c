
#include <stdlib.h>
#include <math.h>

#define lpic_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

#include "asPIC.h"

#include "FSIO.h"

#include "asLUA_PIC.h"
#include "asCON.h"
#include "asUART1.h"
#include "asCARD.h"



//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int uart1_enable (lua_State *L)
{
	asUART1_Enable(luaL_checkinteger(L, 1));
  lua_pushnil(L);
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int uart1_disable (lua_State *L)
{
	asUART1_Disable();
  lua_pushnil(L);
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int uart1_baud (lua_State *L)
{
	asUART1_Baud(luaL_checkinteger(L, 1));
  lua_pushnil(L);
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int uart1_flush_out (lua_State *L)
{
	asUART1_FlushOut();	
  lua_pushnil(L);
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int uart1_count (lua_State *L)
{
	lua_pushinteger(L,asUART1_RXBufCount());
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int uart1_flush_in (lua_State *L)
{
	asUART1_RXBufFlushIn();
  lua_pushnil(L);
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int uart1_write_char (lua_State *L)
{
	asUART1_WriteChar(luaL_checkinteger(L, 1) & 0xff);	
  lua_pushnil(L);
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
// TOTHINKABOUT:
// Right now, this one returns a Lua integer.
// There might be better ways...
static int uart1_read (lua_State *L)
{
	int ch=asUART1_RXBufReadChar(NONBLOCKING);
	if(ch<0)
	{
		lua_pushnil(L);
	  return 1;
	}
	else
	{
		lua_pushinteger(L,ch);
	  return 1;
	}
}



//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int port_dir (lua_State *L)
{
	switch (lua_gettop(L))
	{
		case 1:
		  lua_pushinteger(L,asPIC_Port_Dir(luaL_checkinteger(L, 1),0,0));
		  break;
		case 3:
		  lua_pushinteger(L,asPIC_Port_Dir(luaL_checkinteger(L, 1),luaL_checkinteger(L, 2),luaL_checkinteger(L, 3)));
		  break;
    default: return luaL_error(L, "wrong number of arguments");
	}
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int port_state (lua_State *L)
{
	switch (lua_gettop(L))
	{
		case 1:
			lua_pushinteger(L,asPIC_Port_State(luaL_checkinteger(L, 1),0,0));
			break;
		case 3:
			lua_pushinteger(L,asPIC_Port_State(luaL_checkinteger(L, 1),luaL_checkinteger(L, 2),luaL_checkinteger(L, 3)));
			break;
    default: return luaL_error(L, "wrong number of arguments");
	}
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int port_latch (lua_State *L)
{
	lua_pushinteger(L,asPIC_Port_Latch(luaL_checkinteger(L, 1),luaL_checkinteger(L, 2)));
	return 1;
}




//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int port_pins_high (lua_State *L)
{
	lua_pushinteger(L,asPIC_Port_SetPins(luaL_checkinteger(L, 1),luaL_checkinteger(L, 2)));
	return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int port_pins_low (lua_State *L)
{
	lua_pushinteger(L,asPIC_Port_ClrPins(luaL_checkinteger(L, 1),luaL_checkinteger(L, 2)));
	return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int port_pins_inv (lua_State *L)
{
	lua_pushinteger(L,asPIC_Port_InvPins(luaL_checkinteger(L, 1),luaL_checkinteger(L, 2)));
	return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int timer_read (lua_State *L)
{
	lua_pushnumber(L,asPIC_Timer_Read());
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int timer_match (lua_State *L)
{
	switch (lua_gettop(L))
	{
		case 1:
			asPIC_Timer_Match(luaL_checknumber(L, 1),BLOCKING);
			lua_pushinteger(L,1);
			break;
		case 2:
			lua_pushinteger(L,asPIC_Timer_Match(luaL_checknumber(L, 1),luaL_checknumber(L, 2)));
		  break;
    default: return luaL_error(L, "wrong number of arguments");
	}
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int timer_delay (lua_State *L)
{
	unsigned long long stim=asPIC_Timer_Read();

	switch (lua_gettop(L))
	{
		case 2:
			// microsecond flag set?
			// if not, proceed with case '1'
			if(luaL_checknumber(L,2)!=0)
			{
			// ASkr TODO should be 0.000001/TIMER45_TICK => bug in C32 casts !$%&$"
				stim+=luaL_checknumber(L,1) * 10;
				break;
			}

		case 1:
			// ASkr TODO should be 0.001/TIMER45_TICK => bug in C32 casts !$%&$"
			stim+=luaL_checknumber(L,1) * 10000;
			break;
		
    default: return luaL_error(L, "wrong number of arguments");
   }

	while(asPIC_Timer_Read() < stim)
	{ ; }

	lua_pushnil(L);
	return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
// TOTHINKABOUT:
// Right now, this one returns a Lua integer.
// There might be better ways...
static int console_read (lua_State *L)
{
	int ch=asCON_RXBufReadChar(NONBLOCKING);
	if(ch<0)
	{
		lua_pushnil(L);
	  return 1;
	}
	else
	{
		lua_pushinteger(L,ch);
	  return 1;
	}
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int console_count (lua_State *L)
{
	lua_pushinteger(L,asCON_RXBufCount());
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int console_flush_in (lua_State *L)
{
	asCON_RXBufFlushIn();
	lua_pushnil(L);
  return 1;
}



#ifdef USE_SDCARD_CODE



//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int card_detect (lua_State *L)
{
	lua_pushinteger(L,asCARD_Detect());
  return 1;
}



//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int card_mount(lua_State *L)
{
	lua_pushinteger(L,asCARD_Mount());
  return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int card_find_first(lua_State *L)
{
	unsigned int i=		ATTR_READ_ONLY
										|ATTR_HIDDEN
										|ATTR_SYSTEM
										|ATTR_VOLUME
//										|ATTR_DIRECTORY
										|ATTR_ARCHIVE
//										|ATTR_MASK
										;

	if(lua_gettop(L)==2)
	{
		switch((int)luaL_checknumber(L,2))
		{
			case 0: break;                    // files only
			case 1: i=ATTR_DIRECTORY; break;	// dirs only
			case 2: i|=ATTR_DIRECTORY; break;	// files and dirs
		}
	}
	
	if(asCARD_FindFirst(luaL_checkstring(L, 1),i) != 0)
	{
		lua_pushnil(L);
	  return 1;
	}		

	lua_pushstring(L,(const char *)CardRec.filename);
	lua_pushinteger(L,CardRec.filesize);
  return 2;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int card_find_next(lua_State *L)
{
	
	if(asCARD_FindNext() != 0)
	{
		lua_pushnil(L);
	  return 1;
	}		

	lua_pushstring(L,(const char *)CardRec.filename);
	lua_pushinteger(L,CardRec.filesize);
  return 2;
}


#endif // USE_SDCARD_CODE



//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int ad_pins(lua_State *L)
{
	AD1CON1bits.ON=0;
	switch(lua_gettop(L))
	{
		case 0:	break;
		case 1: AD1PCFG=(~((unsigned)luaL_checknumber(L,1)))&0xffff; break;
		default:
			AD1CON1bits.ON=1;
			return luaL_error(L, "wrong number of arguments");
	}

	AD1CON1bits.ON=1;
	lua_pushinteger(L,(~AD1PCFG) & 0xffff);
	return 1;
	
}



//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int ad_mux(lua_State *L)
{
	int i=-1;

	switch(lua_gettop(L))
	{
		case 2: 
			i=asPIC_AD_Mux(luaL_checknumber(L,1),luaL_checknumber(L,2),0,0);
			break;
		case 4: 
			i=asPIC_AD_Mux(luaL_checknumber(L,1),luaL_checknumber(L,2),luaL_checknumber(L,3),luaL_checknumber(L,4));
			break;
		default: return luaL_error(L, "wrong number of arguments");
	}

	lua_pushinteger(L,i);
	return 1;
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int ad_ref(lua_State *L)
{
	AD1CON1bits.ON=0;
	// ASkr TODO LuaVREF: no checks
	AD1CON2bits.VCFG=(unsigned)luaL_checknumber(L,1) & 0b111;
	AD1CON1bits.ON=1;

	lua_pushnil(L);
	return 1;
	
}


//**************************************************************************************
//***
//*** 
//**************************************************************************************
static int ad_read(lua_State *L)
{
	lua_pushinteger(L,asPIC_AD_Read());
	return 1;
	
	
}



//**************************************************************************************
//**************************************************************************************
//**************************************************************************************
static const luaL_Reg piclib[] =
{
  {"UART1Enable",     uart1_enable},
  {"UART1Disable",    uart1_disable},
  {"UART1Baud",       uart1_baud},
  {"UART1Count",      uart1_count},
  {"UART1WriteChar",  uart1_write_char},
  {"UART1ReadChar",   uart1_read},
  {"UART1FlushIn",    uart1_flush_in},
  {"UART1FlushOut",   uart1_flush_out},

  {"PortDir",         port_dir},
  {"PortState",       port_state},
  {"PortLatch",       port_latch},
  {"PortPinsHigh",    port_pins_high},
  {"PortPinsLow",     port_pins_low},
  {"PortPinsInv",     port_pins_inv},

  {"TimerRead",       timer_read},
  {"TimerMatch",      timer_match},
  {"TimerDelay",      timer_delay},

  {"ConsoleReadChar", console_read},
  {"ConsoleCount",    console_count},
  {"ConsoleFlushIn",  console_flush_in},

  {"ADPins",          ad_pins},
  {"ADMux",           ad_mux},
  {"ADRef",           ad_ref},
  {"ADRead",          ad_read},

#ifdef USE_SDCARD_CODE

  {"CardDetect",			card_detect},
  {"CardMount",				card_mount},
  {"CardFindFirst",		card_find_first},
  {"CardFindNext",		card_find_next},

#endif

  {NULL, NULL}
};




//**************************************************************************************
//*** 
//*** 
//**************************************************************************************
LUALIB_API int luaopen_pic (lua_State *L)
{
	int i;
	int boot32 = 0;
	char tmp[2];
  luaL_register(L, LUA_PIC_LIBNAME, piclib);
  
  
	//---------------------------------------------------------------------------------  
  // create some easy to remember port "names", e.g.:
  // "pic.port.A", "pic.port.B", ...
#ifdef DEFINE_PORT_NAMES
	lua_newtable(L);

	tmp[0]='A';
	tmp[1]=0;
	for(i=0;i<PIC_MAXPORTS;i++)	
	{
		lua_pushnumber(L,i);
		tmp[0]='A'+i;
  	lua_setfield(L,-2,(const char *)&tmp);
  }
  
  lua_setfield(L,-2,"port");
#endif


	//---------------------------------------------------------------------------------  
	// add a variable for timer45 ticks
	lua_pushnumber(L,TIMER45_TICK);
  lua_setfield(L,-2,"TIMERTICK");


	//---------------------------------------------------------------------------------  
	// mount SD-card and (if possible) execute file "BOOT.LUA"
	
/*	
#ifdef USE_SDCARD_CODE
#ifdef USE_SDCARD_BOOTFILE


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
				lua_getglobal(L, "boot32");
	      if (lua_pcall(L, 0, 0, 0) != 0)
					boot32 |= CARD_BOOT_ERR_NOFCT;
			}
			
		}
		else
			boot32 |= CARD_BOOT_ERR_NOFILE;
			
	}// unable to mount card
	else
		boot32 |= CARD_BOOT_ERR_NOMOUNT;

#else	// _CODE

	boot32 |= CARD_BOOT_ERR_NOBOOT;

#endif // _CODE

#else

	boot32 |= CARD_BOOT_ERR_NOCODE;

#endif // _BOOTFILE

	lua_pushnumber(L,boot32);
  lua_setfield(L,-2,"BOOT32ERR");
*/

 
  
  return 1;
}






