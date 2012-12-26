
// length of the UART2 receive buffer (in bytes/characters)
#define RXBUFSIZE 1024


// undef if backspace should not be used
#define USE_DIRTY_BACKSPACE    

#ifdef USE_DIRTY_BACKSPACE
	#define KEY_BACKSPACE 127		// ascii code: backspace
#endif


//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// no changes below this line

#define NONBLOCKING     0
#define BLOCKING        1
#define RXBUFEMPTY     -1
#define RXOVERFLOW     -2


#ifndef __asCON

	extern inline void asCON_RXBufFlushIn(void);
	extern inline unsigned asCON_RXBufCount(void);
	extern inline void asCON_RXBufInit(void);
	extern int asCON_RXBufWriteChar(char ch);
	extern inline char asCON_RXBufReadChar(int blocked);
	extern void asCON_SerialWriteString(char *ch);
	extern inline void asCON_SerialFlush();
	extern inline void asCON_SerialWriteChar(char ch);
	extern unsigned int asCON_SerialReadLine(unsigned int maxdat,char *buffer, unsigned int uart_data_wait);


#else

	void asCON_SerialWriteString(char *ch);
	inline void asCON_SerialFlush();
	inline void asCON_SerialWriteChar(char ch);
	unsigned int asCON_SerialReadLine(unsigned int maxdat,char *buffer, unsigned int uart_data_wait);

#endif


