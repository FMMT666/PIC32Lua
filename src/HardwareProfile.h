
#ifndef _HARDWAREPROFILE_H_
#define _HARDWAREPROFILE_H_


#include "asPIC.h"



//--------------------------------------------------------------------------------------
// DEFINE SD-CARD SPI PORT
//   #Note1: Choose only 1 ;)
//#define MDD_USE_SPI_1
#define MDD_USE_SPI_2






//===========================================================================================
//===========================================================================================
// NO CHANGES BELOW THIS LINE
// (EXCEPT YOU KNOW WHAT YOU ARE DOING ;-)


// Clock values
#define MILLISECONDS_PER_TICK       10                  // Definition for use with a tick timer
#define TIMER_PRESCALER             TIMER_PRESCALER_8   // Definition for use with a tick timer
#define TIMER_PERIOD                37500               // Definition for use with a tick timer

//SPI Configuration
#define SPI_START_CFG_1     (PRI_PRESCAL_64_1 | SEC_PRESCAL_8_1 | MASTER_ENABLE_ON | SPI_CKE_ON | SPI_SMP_ON)
#define SPI_START_CFG_2     (SPI_ENABLE)

// Define the SPI frequency
#define SPI_FREQUENCY			(20000000)


#if defined MDD_USE_SPI_1
    // Description: SD-SPI Chip Select Output bit
	 	#define SD_CS               LATBbits.LATB1
	 	// Description: SD-SPI Chip Select TRIS bit
    #define SD_CS_TRIS          TRISBbits.TRISB1
    
    // Description: SD-SPI Card Detect Input bit
    #define SD_CD               PORTFbits.RF0
    // Description: SD-SPI Card Detect TRIS bit
    #define SD_CD_TRIS          TRISFbits.TRISF0

    // Description: SD-SPI Write Protect Check Input bit
    #define SD_WE               PORTFbits.RF1
    // Description: SD-SPI Write Protect Check TRIS bit
    #define SD_WE_TRIS          TRISFbits.TRISF1
           
    // Description: The main SPI control register
    #define SPICON1             SPI1CON
    // Description: The SPI status register
    #define SPISTAT             SPI1STAT
    // Description: The SPI Buffer
    #define SPIBUF              SPI1BUF
    // Description: The receive buffer full bit in the SPI status register
    #define SPISTAT_RBF         SPI1STATbits.SPIRBF
    // Description: The bitwise define for the SPI control register (i.e. _____bits)
    #define SPICON1bits         SPI1CONbits
    // Description: The bitwise define for the SPI status register (i.e. _____bits)
    #define SPISTATbits         SPI1STATbits
    // Description: The enable bit for the SPI module
    #define SPIENABLE           SPICON1bits.ON
    // Description: The definition for the SPI baud rate generator register (PIC32)
    #define SPIBRG			    SPI1BRG

    // Tris pins for SCK/SDI/SDO lines


		// changed by ASkr
		// the 795F512L does not have F6 and F7
/*		
	    // Description: The TRIS bit for the SCK pin
  	  #define SPICLOCK            TRISFbits.TRISF6
    	// Description: The TRIS bit for the SDI pin
	    #define SPIIN               TRISFbits.TRISF7
  	  // Description: The TRIS bit for the SDO pin
	    #define SPIOUT              TRISFbits.TRISF8
*/
  	  #define SPICLOCK            TRISDbits.TRISD15
	    #define SPIIN               TRISFbits.TRISF2
	    #define SPIOUT              TRISFbits.TRISF8


    
    
    //SPI library functions
    #define putcSPI             putcSPI1
    #define getcSPI             getcSPI1
    #define OpenSPI(config1, config2)   OpenSPI1(config1, config2)
    
#elif defined MDD_USE_SPI_2
    // Description: SD-SPI Chip Select Output bit
    #define SD_CS               LATBbits.LATB9
    // Description: SD-SPI Chip Select TRIS bit
    #define SD_CS_TRIS          TRISBbits.TRISB9
    
    // Description: SD-SPI Card Detect Input bit
    #define SD_CD               PORTGbits.RG0
    // Description: SD-SPI Card Detect TRIS bit
    #define SD_CD_TRIS          TRISGbits.TRISG0

    // Description: SD-SPI Write Protect Check Input bit
    #define SD_WE               PORTGbits.RG1
    // Description: SD-SPI Write Protect Check TRIS bit
    #define SD_WE_TRIS          TRISGbits.TRISG1
    
    // Description: The main SPI control register
    #define SPICON1             SPI2CON
    // Description: The SPI status register
    #define SPISTAT             SPI2STAT
    // Description: The SPI Buffer
    #define SPIBUF              SPI2BUF
    // Description: The receive buffer full bit in the SPI status register
    #define SPISTAT_RBF         SPI2STATbits.SPIRBF
    // Description: The bitwise define for the SPI control register (i.e. _____bits)
    #define SPICON1bits         SPI2CONbits
    // Description: The bitwise define for the SPI status register (i.e. _____bits)
    #define SPISTATbits         SPI2STATbits
    // Description: The enable bit for the SPI module
    #define SPIENABLE           SPI2CONbits.ON
    // Description: The definition for the SPI baud rate generator register (PIC32)
    #define SPIBRG			    SPI2BRG

    // Tris pins for SCK/SDI/SDO lines

    // Description: The TRIS bit for the SCK pin
    #define SPICLOCK            TRISGbits.TRISG6
    // Description: The TRIS bit for the SDI pin
    #define SPIIN               TRISGbits.TRISG7
    // Description: The TRIS bit for the SDO pin
    #define SPIOUT              TRISGbits.TRISG8
    //SPI library functions
    #define putcSPI             putcSPI2
    #define getcSPI             getcSPI2
    #define OpenSPI(config1, config2)   OpenSPI2(config1, config2)
#endif       


// Will generate an error if the clock speed is too low to interface to the card
#if (GetSystemClock() < 100000)
    #error Clock speed must exceed 100 kHz
#endif    



#endif
