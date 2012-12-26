#include "asPIC.h"

#ifndef __asIO

	#ifdef CONSOLE_SWAP
		extern void _mon_putc(char ch);
	#endif

	extern int ffferror(FILE *fin);
	extern int ffflush (FILE *fin);
	extern char *fgets (char *, int , FILE *);
	extern int fgetc (FILE *fin);
	extern int fclose (FILE *fin);
	extern FILE *fopen (const char *a, const char *b);

	extern size_t fread (void *a, size_t b, size_t c, FILE *fin);
	
	extern FILE *open (const char *a, const char *b);
	extern int close (FILE *fin);
	extern int lseek (FILE *fin, long int a, int b);
	extern long int ftell (FILE *fin);
	extern size_t read (void *a, size_t b, size_t c, FILE *fin);
	extern int write (void *fin, char *dat);
	extern int remove (const char *fnam);
	extern void unlink();
	extern void link();
	extern int gettimeofday (struct timeval *a, void *b);

#endif
