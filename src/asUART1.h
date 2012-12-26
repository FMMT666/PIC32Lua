
// length of the UART1 receive buffer (in bytes/characters)
#define UART1_RXBUFSIZE 256


#ifndef __asUART1

	extern void asUART1_Enable(unsigned baud);
	extern void asUART1_Disable(void);
	extern void asUART1_Baud(unsigned baud);
	extern inline void asUART1_RXBufInit(void);
	extern int asUART1_RXBufWriteChar(char ch);
	extern inline int asUART1_RXBufReadChar(int blocked);
	extern inline void asUART1_RXBufFlushIn(void);
	extern inline unsigned asUART1_RXBufCount(void);
	extern inline void asUART1_FlushOut(void);
	extern void asUART1_WriteString(char *ch);
	extern inline void asUART1_WriteChar(char ch);


#else

	 void asUART1_Baud(unsigned baud);
	 inline void asUART1_RXBufInit(void);
	 int asUART1_RXBufWriteChar(char ch);
	 inline int asUART1_RXBufReadChar(int blocked);
	 inline void asUART1_RXBufFlushIn(void);
	 inline unsigned asUART1_RXBufCount(void);
	 inline void asUART1_FlushOut(void);
	 void asUART1_WriteString(char *ch);
	 inline void asUART1_WriteChar(char ch);

#endif

