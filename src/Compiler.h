
#ifndef __COMPILER_H
#define __COMPILER_H


#include <p32xxxx.h>
#include <plib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define PTR_BASE		unsigned long
#define ROM_PTR_BASE	unsigned long


#define memcmppgm2ram(a,b,c)	memcmp(a,b,c)
#define strcmppgm2ram(a,b)		strcmp(a,b)
#define memcpypgm2ram(a,b,c)	memcpy(a,b,c)
#define strcpypgm2ram(a,b)		strcpy(a,b)
#define strncpypgm2ram(a,b,c)	strncpy(a,b,c)
#define strstrrampgm(a,b)		strstr(a,b)
#define	strlenpgm(a)			strlen(a)
#define strchrpgm(a,b)			strchr(a,b)
#define strcatpgm2ram(a,b)		strcat(a,b)


#define	ROM						const


#define persistent
#define far
#define FAR
#define Reset()				SoftReset()
#define ClrWdt()			(WDTCONSET = _WDTCON_WDTCLR_MASK)

#if !defined(Nop)	
	#define Nop()				asm("nop")
#endif


#endif //__COMPILER_H
