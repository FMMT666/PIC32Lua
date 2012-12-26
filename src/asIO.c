
#include <stdio.h>

#ifndef __asIO
#define __asIO
#endif


#include "asPIC.h"
#include "FSIO.h"
#include "asCON.h"



// JUST SOME DIRTY AND QUICK HACKS
//
// Especially the new (V0.4) CONSOLE_SWAP feature (UART1 <-> UART2)
// needs some intense testing...
//



#ifdef CONSOLE_SWAP
//**************************************************************************************
//*** used by (f)printf() on console swap (UART1)
//***
//***
//**************************************************************************************
void _mon_putc(char ch)
{
	asCON_SerialWriteChar(ch);
}

#endif



//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
int ffferror(FILE *fin)
{
	// Just returns the result of the last operation,
	// not related to <fin>...
	return FSerror();
}



//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
int ffflush (FILE *fin)
{
	// should not be required...
	return 0;
}



//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
char *fgets (char *str, int max, FILE *fin)
{
	int ch=0;
	char *ptr=str;

	while (--max > 0 && ((ch = fgetc(fin)) != EOF))
	{
		*ptr++ = (char)ch;
		if ( (char)ch == '\n')
			break;
	}


	if (ch == EOF)
	{
		if (FSfeof((FSFILE *)fin))
		{
			if (ptr == str)
				return NULL;
    }
    else
    	return NULL;
	}
	*ptr = '\0';
	return str;
}



//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
int fgetc (FILE *fin)
{
	int ch;

	if( fin == stdin )
	{
		// not that nice, but useful
		ch=asCON_RXBufReadChar(BLOCKING);
		asCON_SerialWriteChar(ch);
		// ENTER key returns '\r'
		if((char)ch=='\r')
			ch='\n';
		return ch;
	}
	else
	{
		if(FSfread((int *)&ch,1,1,(FSFILE *)fin) != 1 )
			return EOF;
		else
			return ch;
	}
}



//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
FILE *fopen (const char *a, const char *b)
{
	return (FILE *)FSfopen(a,b);
}	



//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
FILE *open (const char *a, const char *b)
{
	// This should not be required...
	// DEBUG ONLY
	asCON_SerialWriteString("OWN OPEN\n\r");
	return (FILE *)FSfopen(a,b);

}



//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
int fclose (FILE *fin)
{
	return FSfclose((FSFILE *)fin);
}



//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
int close (FILE *fin)
{
	// This should not be required...
	// DEBUG ONLY
	asCON_SerialWriteString("close\n\r");
	return FSfclose((FSFILE *)fin);
}



//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
int lseek (FILE *fin, long int a, int b)
{
	return FSfseek((FSFILE *)fin,a,b);
}



//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
long ftell (FILE *fin)
{
	return FSftell((FSFILE *)fin);
}



//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
size_t fread (void *a, size_t b, size_t c, FILE *fin)
{
	if(fin==stdin)
	{
		// not required (direct call of "fgets()" above)

		// ASkr DEBUG
		asCON_SerialWriteString("*ouch*\n\r");

		return 0;
	}
	else

	return FSfread(a,b,c,(FSFILE *)fin);
}



//**************************************************************************************
//*** fwrite
//***
//*** output for:
//*** - io.write (file and "console") 
//**************************************************************************************
size_t fwrite (const void *ptr, size_t size, size_t count, FILE *fin )
{
	char *pc=(char *)ptr;

	if( ( fin == stdout) || ( fin == stderr ) )
	{
		while(count--)
			asCON_SerialWriteChar(*pc++);
		return count;
	}
	else
		return FSfwrite(ptr,size,count,(FSFILE *)fin);
}




//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
size_t read (void *a, size_t b, size_t c, FILE *fin)
{
	// This should not be required...
	// DEBUG ONLY
	asCON_SerialWriteString("read\n\r");
	return FSfread(a,b,c,(FSFILE *)fin);
}




//**************************************************************************************
//*** write()
//***
//*** This is the main UART2 console output function for:
//*** - fputs(..., stderr)
//***	- fputs(..., stdout)
//*** - printf(...)
//*** - fprintf(stdout, ...)
//***	- fprintf(stderr, ...)
//***
//**************************************************************************************
size_t write(int fd, char *dat, size_t len)
{
	// ASkr TODO
	// ouch...
	if( ( fd==1 ) || ( fd==2 ) )
		asCON_SerialWriteChar(*dat);
	else
	{
		// ASkr DEBUG
		asCON_SerialWriteString("*ouch*\n\r");
	}	
	return 1;
}



//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
int remove (const char *fnam)
{
	// DEBUG ONLY
	asCON_SerialWriteString("remove\n\r");
	return 0;
}


//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
void unlink()
{
	// DEBUG ONLY
	asCON_SerialWriteString("unlink\n\r");
}


//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
void link()
{
	// DEBUG ONLY
	asCON_SerialWriteString("link\n\r");
}


//**************************************************************************************
//*** 
//***
//***
//**************************************************************************************
int gettimeofday (struct timeval *a, void *b)
{
	// DEBUG ONLY
	return 0;
}

