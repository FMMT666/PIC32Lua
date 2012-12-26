
// initialises some debug pins
//#define PIC32_DEBUG


#ifndef __asDBG

	extern volatile unsigned char pPin1;
	extern volatile unsigned char pPin2;
	
	#define asDBG_Pin1Init() (TRISDbits.TRISD0=0)
	#define asDBG_Pin1(s)    (LATDbits.LATD0=(s))
	#define asDBG_Pin1Tog()  (LATDbits.LATD0=((pPin1=!pPin1)))

	#define asDBG_Pin2Init() (TRISDbits.TRISD1=0)
	#define asDBG_Pin2(s)    (LATDbits.LATD1=(s))
	#define asDBG_Pin2Tog()  (LATDbits.LATD1=((pPin2=!pPin2)))

	extern void asDBG_DumpReg(unsigned int reg);


#else

	volatile unsigned char pPin1=0;
	volatile unsigned char pPin2=0;

#endif
