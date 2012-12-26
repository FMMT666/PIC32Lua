
#include "Compiler.h"
#include "GenericTypeDefs.h"
/*
ASkr
#include "MDD File System\FSIO.h"
#include "MDD File System\FSDefs.h"
#include "MDD File System\SD-SPI.h"
*/
#include "FSIO.h"
#include "FSDefs.h"
#include "SD-SPI.h"

#include "string.h"
#include "FSConfig.h"
#include "HardwareProfile.h"

#include "asPIC.h"
/******************************************************************************
 * Global Variables
 *****************************************************************************/

// Description:  Used for the mass-storage library to determine capacity
DWORD MDD_SDSPI_finalLBA;
WORD gMediaSectorSize;
BYTE gSDMode;
static MEDIA_INFORMATION mediaInformation;

const typMMC_CMD sdmmc_cmdtable[] =
{
    // cmd                      crc     response
    {cmdGO_IDLE_STATE,          0x95,   R1,     NODATA},
    {cmdSEND_OP_COND,           0xF9,   R1,     NODATA},
    {cmdSEND_IF_COND,      		0x87,   R7,     NODATA},
    {cmdSEND_CSD,               0xAF,   R1,     MOREDATA},
    {cmdSEND_CID,               0x1B,   R1,     MOREDATA},
    {cmdSTOP_TRANSMISSION,      0xC3,   R1,     NODATA},
    {cmdSEND_STATUS,            0xAF,   R2,     NODATA},
    {cmdSET_BLOCKLEN,           0xFF,   R1,     NODATA},
    {cmdREAD_SINGLE_BLOCK,      0xFF,   R1,     MOREDATA},
    {cmdREAD_MULTI_BLOCK,       0xFF,   R1,     MOREDATA},
    {cmdWRITE_SINGLE_BLOCK,     0xFF,   R1,     MOREDATA},
    {cmdWRITE_MULTI_BLOCK,      0xFF,   R1,     MOREDATA}, 
    {cmdTAG_SECTOR_START,       0xFF,   R1,     NODATA},
    {cmdTAG_SECTOR_END,         0xFF,   R1,     NODATA},
    {cmdERASE,                  0xDF,   R1b,    NODATA},
    {cmdAPP_CMD,                0x73,   R1,     NODATA},
    {cmdREAD_OCR,               0x25,   R7,     NODATA},
    {cmdCRC_ON_OFF,             0x25,   R1,     NODATA}
};




/******************************************************************************
 * Prototypes
 *****************************************************************************/
extern void Delayms(BYTE milliseconds);
BYTE MDD_SDSPI_ReadMedia(void);
MEDIA_INFORMATION * MDD_SDSPI_MediaInitialize(void);
MMC_RESPONSE SendMMCCmd(BYTE cmd, DWORD address);


void OpenSPIM ( unsigned int sync_mode);
void CloseSPIM( void );
unsigned char WriteSPIM( unsigned char data_out );



/*********************************************************
  Function:
    static inline __attribute__((always_inline)) unsigned char SPICacutateBRG (unsigned int pb_clk, unsigned int spi_clk)
  Summary:
    Calculate the PIC32 SPI BRG value
  Conditions:
    None
  Input:
    pb_clk -  The value of the PIC32 peripheral clock
    spi_clk - The desired baud rate
  Return:
    The corresponding BRG register value.
  Side Effects:
    None.
  Description:
    The SPICalutateBRG function is used to determine an appropriate BRG register value for the PIC32 SPI module.
  Remarks:
    None                                                  
  *********************************************************/

static inline __attribute__((always_inline)) unsigned char SPICalutateBRG(unsigned int pb_clk, unsigned int spi_clk)
{
    unsigned int brg;

    brg = pb_clk / (2 * spi_clk);

    if(pb_clk % (2 * spi_clk))
        brg++;

    if(brg > 0x100)
        brg = 0x100;

    if(brg)
        brg--;

    return (unsigned char) brg;
}



/*********************************************************
  Function:
    BYTE MDD_SDSPI_MediaDetect
  Summary:
    Determines whether an SD card is present
  Conditions:
    The MDD_MediaDetect function pointer must be configured
    to point to this function in FSconfig.h
  Input:
    None
  Return Values:
    TRUE -  Card detected
    FALSE - No card detected
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_MediaDetect function determine if an SD card is connected to 
    the microcontroller.
    If the MEDIA_SOFT_DETECT is not defined, the detection is done by polling
    the SD card detect pin.
    The MicroSD connector does not have a card detect pin, and therefore a
    software mechanism must be used. To do this, the SEND_STATUS command is sent 
    to the card. If the card is not answering with 0x00, the card is either not 
    present, not configured, or in an error state. If this is the case, we try
    to reconfigure the card. If the configuration fails, we consider the card not 
    present (it still may be present, but malfunctioning). In order to use the 
    software card detect mechanism, the MEDIA_SOFT_DETECT macro must be defined.
    
  Remarks:
    None                                                  
  *********************************************************/

BYTE MDD_SDSPI_MediaDetect (void)
{
#ifndef MEDIA_SOFT_DETECT
    return(!SD_CD);
#else

	// ASkr
	// won't work
	return 0;


	MMC_RESPONSE response;

	// ASkr TODO
	if (SPIENABLE == 0)
	{
		#if (GetSystemClock() >= 25600000)
	    	response = SendMMCCmdManual(SEND_STATUS,0x0);
		#endif
	}
	else
	
	{
    	response = SendMMCCmd(SEND_STATUS,0x0);
	}

	if(response.r2._word != 0x00)
	{
		MDD_SDSPI_MediaInitialize();
		if (mediaInformation.errorCode == MEDIA_NO_ERROR)
		{
			return 1;
		}
		else 
		{
			return 0;
		}
	}
	else
	{

		return 1;
	}
	
#endif

}//end MediaDetect



/*********************************************************
  Function:
    WORD MDD_SDSPI_ReadSectorSize (void)
  Summary:
    Determines the current sector size on the SD card
  Conditions:
    MDD_MediaInitialize() is complete
  Input:
    None
  Return:
    The size of the sectors for the physical media
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_ReadSectorSize function is used by the
    USB mass storage class to return the card's sector
    size to the PC on request.
  Remarks:
    None
  *********************************************************/

WORD MDD_SDSPI_ReadSectorSize(void)
{
    return gMediaSectorSize;
}


/*********************************************************
  Function:
    DWORD MDD_SDSPI_ReadCapacity (void)
  Summary:
    Determines the current capacity of the SD card
  Conditions:
    MDD_MediaInitialize() is complete
  Input:
    None
  Return:
    The capacity of the device
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_ReadCapacity function is used by the
    USB mass storage class to return the total number
    of sectors on the card.
  Remarks:
    None
  *********************************************************/
DWORD MDD_SDSPI_ReadCapacity(void)
{
    return (MDD_SDSPI_finalLBA);
}


/*********************************************************
  Function:
    WORD MDD_SDSPI_InitIO (void)
  Summary:
    Initializes the I/O lines connected to the card
  Conditions:
    MDD_MediaInitialize() is complete.  The MDD_InitIO
    function pointer is pointing to this function.
  Input:
    None
  Return:
    None
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_InitIO function initializes the I/O
    pins connected to the SD card.
  Remarks:
    None
  *********************************************************/

void MDD_SDSPI_InitIO (void)
{
    // Turn off the card
    SD_CD_TRIS = INPUT;            //Card Detect - input
    SD_CS = 1;                     //Initialize Chip Select line
    SD_CS_TRIS = OUTPUT;            //Card Select - output
    SD_WE_TRIS = INPUT;            //Write Protect - input
}



/*********************************************************
  Function:
    BYTE MDD_SDSPI_ShutdownMedia (void)
  Summary:
    Disables the SD card
  Conditions:
    The MDD_ShutdownMedia function pointer is pointing 
    towards this function.
  Input:
    None
  Return:
    None
  Side Effects:
    None.
  Description:
    This function will disable the SPI port and deselect
    the SD card.
  Remarks:
    None
  *********************************************************/

BYTE MDD_SDSPI_ShutdownMedia(void)
{
    // close the spi bus
    CloseSPIM();
    
    // deselect the device
    SD_CS = 1;

    return 0;
}


/*****************************************************************************
  Function:
    MMC_RESPONSE SendMMCCmd (BYTE cmd, DWORD address)
  Summary:
    Sends a command packet to the SD card.
  Conditions:
    None.
  Input:
    None.
  Return Values:
    MMC_RESPONSE    - The response from the card
                    - Bit 0 - Idle state
                    - Bit 1 - Erase Reset
                    - Bit 2 - Illegal Command
                    - Bit 3 - Command CRC Error
                    - Bit 4 - Erase Sequence Error
                    - Bit 5 - Address Error
                    - Bit 6 - Parameter Error
                    - Bit 7 - Unused. Always 0.
  Side Effects:
    None.
  Description:
    SendMMCCmd prepares a command packet and sends it out over the SPI interface.
    Response data of type 'R1' (as indicated by the SD/MMC product manual is returned.
  Remarks:
    None.
  ***************************************************************************************/

MMC_RESPONSE SendMMCCmd(BYTE cmd, DWORD address)
{
    WORD timeout = 0x8;
    BYTE index;
    MMC_RESPONSE    response;
    CMD_PACKET  CmdPacket;
    
    SD_CS = 0;                           //Card Select
    
    // Copy over data
    CmdPacket.cmd        = sdmmc_cmdtable[cmd].CmdCode;
    CmdPacket.address    = address;
    CmdPacket.crc        = sdmmc_cmdtable[cmd].CRC;       // Calc CRC here
    
    CmdPacket.TRANSMIT_BIT = 1;             //Set Tranmission bit
    
    WriteSPIM(CmdPacket.cmd);                //Send Command
    WriteSPIM(CmdPacket.addr3);              //Most Significant Byte
    WriteSPIM(CmdPacket.addr2);
    WriteSPIM(CmdPacket.addr1);
    WriteSPIM(CmdPacket.addr0);              //Least Significant Byte
    WriteSPIM(CmdPacket.crc);                //Send CRC
    
    // see if we are going to get a response
    if(sdmmc_cmdtable[cmd].responsetype == R1 || sdmmc_cmdtable[cmd].responsetype == R1b || sdmmc_cmdtable[cmd].responsetype == R7)
    {
        do
        {
            response.r1._byte = MDD_SDSPI_ReadMedia();
            timeout--;
        }while(response.r1._byte == MMC_FLOATING_BUS && timeout != 0);
    }
    else if(sdmmc_cmdtable[cmd].responsetype == R2)
    {
        MDD_SDSPI_ReadMedia();
        
        response.r2._byte1 = MDD_SDSPI_ReadMedia();
        response.r2._byte0 = MDD_SDSPI_ReadMedia();
    }

    if(sdmmc_cmdtable[cmd].responsetype == R1b)
    {
        response.r1._byte = 0x00;
        
        for(index =0; index < 0xFF && response.r1._byte == 0x00; index++)
        {
            timeout = 0xFFFF;
            
            do
            {
                response.r1._byte = MDD_SDSPI_ReadMedia();
                timeout--;
            }while(response.r1._byte == 0x00 && timeout != 0);
        }
    }

    if (sdmmc_cmdtable[cmd].responsetype == R7)
    {
        response.r7.bytewise._returnVal = (DWORD)MDD_SDSPI_ReadMedia() << 24;
        response.r7.bytewise._returnVal += (DWORD)MDD_SDSPI_ReadMedia() << 16;
        response.r7.bytewise._returnVal += (DWORD)MDD_SDSPI_ReadMedia() << 8;
        response.r7.bytewise._returnVal += (DWORD)MDD_SDSPI_ReadMedia();
    }

    mSend8ClkCycles();                      //Required clocking (see spec)

    // see if we are expecting data or not
    if(!(sdmmc_cmdtable[cmd].moredataexpected))
        SD_CS = 1;

    return(response);
}




#ifdef __18CXX
#if (GetSystemClock() >= 25600000)
/*****************************************************************************
  Function:
    MMC_RESPONSE SendMMCCmdManual (BYTE cmd, DWORD address)
  Summary:
    Sends a command packet to the SD card with bit-bang SPI.
  Conditions:
    None.
  Input:
    None.
  Return Values:
    MMC_RESPONSE    - The response from the card
                    - Bit 0 - Idle state
                    - Bit 1 - Erase Reset
                    - Bit 2 - Illegal Command
                    - Bit 3 - Command CRC Error
                    - Bit 4 - Erase Sequence Error
                    - Bit 5 - Address Error
                    - Bit 6 - Parameter Error
                    - Bit 7 - Unused. Always 0.
  Side Effects:
    None.
  Description:
    SendMMCCmd prepares a command packet and sends it out over the SPI interface.
    Response data of type 'R1' (as indicated by the SD/MMC product manual is returned.
    This function is intended to be used when the clock speed of a PIC18 device is
    so high that the maximum SPI divider can't reduce the clock below the maximum
    SD card initialization sequence speed.
  Remarks:
    None.
  ***************************************************************************************/
MMC_RESPONSE SendMMCCmdManual(BYTE cmd, DWORD address)
{
    WORD timeout = 0x8;
    BYTE index;
    MMC_RESPONSE response;
    CMD_PACKET  CmdPacket;
    
    SD_CS = 0;                           //Card Select
    
    // Copy over data
    CmdPacket.cmd        = sdmmc_cmdtable[cmd].CmdCode;
    CmdPacket.address    = address;
    CmdPacket.crc        = sdmmc_cmdtable[cmd].CRC;       // Calc CRC here
    
    CmdPacket.TRANSMIT_BIT = 1;             //Set Tranmission bit
    
    WriteSPIManual(CmdPacket.cmd);                //Send Command
    WriteSPIManual(CmdPacket.addr3);              //Most Significant Byte
    WriteSPIManual(CmdPacket.addr2);
    WriteSPIManual(CmdPacket.addr1);
    WriteSPIManual(CmdPacket.addr0);              //Least Significant Byte
    WriteSPIManual(CmdPacket.crc);                //Send CRC
    
    // see if we are going to get a response
    if(sdmmc_cmdtable[cmd].responsetype == R1 || sdmmc_cmdtable[cmd].responsetype == R1b || sdmmc_cmdtable[cmd].responsetype == R7)
    {
        do
        {
            response.r1._byte = ReadMediaManual();
            timeout--;
        }while(response.r1._byte == MMC_FLOATING_BUS && timeout != 0);
    }
    else if(sdmmc_cmdtable[cmd].responsetype == R2)
    {
        ReadMediaManual();
        
        response.r2._byte1 = ReadMediaManual();
        response.r2._byte0 = ReadMediaManual();
    }

    if(sdmmc_cmdtable[cmd].responsetype == R1b)
    {
        response.r1._byte = 0x00;
        
        for(index =0; index < 0xFF && response.r1._byte == 0x00; index++)
        {
            timeout = 0xFFFF;
            
            do
            {
                response.r1._byte = ReadMediaManual();
                timeout--;
            }while(response.r1._byte == 0x00 && timeout != 0);
        }
    }
    if (sdmmc_cmdtable[cmd].responsetype == R7)
    {
        response.r7.bytewise._returnVal = (DWORD)ReadMediaManual() << 24;
        response.r7.bytewise._returnVal += (DWORD)ReadMediaManual() << 16;
        response.r7.bytewise._returnVal += (DWORD)ReadMediaManual() << 8;
        response.r7.bytewise._returnVal += (DWORD)ReadMediaManual();
    }

    WriteSPIManual(0xFF);                      //Required clocking (see spec)

    // see if we are expecting data or not
    if(!(sdmmc_cmdtable[cmd].moredataexpected))
        SD_CS = 1;

    return(response);
}


#endif
#endif




/*****************************************************************************
  Function:
    BYTE MDD_SDSPI_SectorRead (DWORD sector_addr, BYTE * buffer)
  Summary:
    Reads a sector of data from an SD card.
  Conditions:
    The MDD_SectorRead function pointer must be pointing towards this function.
  Input:
    sector_addr - The address of the sector on the card.
    byffer -      The buffer where the retrieved data will be stored.  If
                  buffer is NULL, do not store the data anywhere.
  Return Values:
    TRUE -  The sector was read successfully
    FALSE - The sector could not be read
  Side Effects:
    None
  Description:
    The MDD_SDSPI_SectorRead function reads a sector of data bytes (512 bytes) 
    of data from the SD card starting at the sector address and stores them in 
    the location pointed to by 'buffer.'
  Remarks:
    The card expects the address field in the command packet to be a byte address.
    The sector_addr value is converted to a byte address by shifting it left nine
    times (multiplying by 512).
  ***************************************************************************************/

BYTE MDD_SDSPI_SectorRead(DWORD sector_addr, BYTE* buffer)
{
    WORD index;
    WORD delay;
    MMC_RESPONSE    response;
    BYTE data_token;
    BYTE status = TRUE;
    DWORD   new_addr;
   
    // send the cmd
    if (gSDMode == SD_MODE_NORMAL)
        new_addr = sector_addr << 9;
    else
        new_addr = sector_addr;
    response = SendMMCCmd(READ_SINGLE_BLOCK,new_addr);

    // Make sure the command was accepted
    if(response.r1._byte != 0x00)
    {
        response = SendMMCCmd (READ_SINGLE_BLOCK,new_addr);
        if(response.r1._byte != 0x00)
        {
            return FALSE;
        }
    }

    index = 0x2FF;
    
    // Timing delay- at least 8 clock cycles
    delay = 0x40;
    while (delay)
        delay--;
  
    //Now, must wait for the start token of data block
    do
    {
        data_token = MDD_SDSPI_ReadMedia();
        index--;   
        
        delay = 0x40;
        while (delay)
            delay--;

    }while((data_token == MMC_FLOATING_BUS) && (index != 0));

    // Hopefully that zero is the datatoken
    if((index == 0) || (data_token != DATA_START_TOKEN))
    {
        status = FALSE;
    }
    else
    {
        for(index = 0; index < gMediaSectorSize; index++)      //Reads in a sector of data (512 bytes)
        {
            if(buffer != NULL)
            {
               	buffer[index] = MDD_SDSPI_ReadMedia();
            }
            else
            {
                MDD_SDSPI_ReadMedia();
            }
        }
        // Now ensure CRC
        mReadCRC();               //Read 2 bytes of CRC
        //status = mmcCardCRCError;
    }

    mSend8ClkCycles();            //Required clocking (see spec)

    SD_CS = 1;

    return(status);
}//end SectorRead


/*****************************************************************************
  Function:
    BYTE MDD_SDSPI_SectorWrite (DWORD sector_addr, BYTE * buffer, BYTE allowWriteToZero)
  Summary:
    Writes a sector of data to an SD card.
  Conditions:
    The MDD_SectorWrite function pointer must be pointing to this function.
  Input:
    sector_addr -      The address of the sector on the card.
    buffer -           The buffer with the data to write.
    allowWriteToZero -
                     - TRUE -  Writes to the 0 sector (MBR) are allowed
                     - FALSE - Any write to the 0 sector will fail.
  Return Values:
    TRUE -  The sector was written successfully.
    FALSE - The sector could not be written.
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_SectorWrite function writes one sector of data (512 bytes) 
    of data from the location pointed to by 'buffer' to the specified sector of 
    the SD card.
  Remarks:
    The card expects the address field in the command packet to be a byte address.
    The sector_addr value is ocnverted to a byte address by shifting it left nine
    times (multiplying by 512).
  ***************************************************************************************/

BYTE MDD_SDSPI_SectorWrite(DWORD sector_addr, BYTE* buffer, BYTE allowWriteToZero)
{
    WORD            index;
    DWORD           counter;
    BYTE            data_response;
    MMC_RESPONSE    response; 
    BYTE            status = TRUE;

    if (sector_addr == 0 && allowWriteToZero == FALSE)
        status = FALSE;
    else
    {
        // send the cmd

        if (gSDMode == SD_MODE_NORMAL)
            response = SendMMCCmd(WRITE_SINGLE_BLOCK,(sector_addr << 9));
        else
            response = SendMMCCmd(WRITE_SINGLE_BLOCK,(sector_addr));
        
        // see if it was accepted
        if(response.r1._byte != 0x00)
            status = FALSE;
        else
        {
            WriteSPIM(DATA_START_TOKEN);                 //Send data start token

            for(index = 0; index < gMediaSectorSize; index++)      //Send 512 bytes
            {
   	            WriteSPIM(buffer[index]);
            }

            // calc crc
            mSendCRC();                                 //Send 2 bytes of CRC
            
            data_response = MDD_SDSPI_ReadMedia();                //Read response
            
            if((data_response & 0x0F) != DATA_ACCEPTED)
            {
                status = FALSE;
            }
            else
            {

					do
					{
					    putcSPI((unsigned int)0xFF);
					    data_response = getcSPI();
					}while(!data_response);
            }

            mSend8ClkCycles();
        }

        SD_CS = 1;

    } // Not writing to 0 sector

    return(status);
} //end SectorWrite


/*****************************************************************************
  Function:
    BYTE MDD_SDSPI_WriteProtectState
  Summary:
    Indicates whether the card is write-protected.
  Conditions:
    The MDD_WriteProtectState function pointer must be pointing to this function.
  Input:
    None.
  Return Values:
    TRUE -  The card is write-protected
    FALSE - The card is not write-protected
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_WriteProtectState function will determine if the SD card is
    write protected by checking the electrical signal that corresponds to the
    physical write-protect switch.
  Remarks:
    None
  ***************************************************************************************/

BYTE MDD_SDSPI_WriteProtectState(void)
{
    return(SD_WE);
}


/*****************************************************************************
  Function:
    void Delayms (BYTE milliseconds)
  Summary:
    Delay.
  Conditions:
    None.
  Input:
    BYTE milliseconds - Number of ms to delay
  Return:
    None.
  Side Effects:
    None.
  Description:
    The Delayms function will delay a specified number of milliseconds.  Used for SPI
    timing.
  Remarks:
    Depending on compiler revisions, this function may delay for the exact time
    specified.  This shouldn't create a significant problem.
  ***************************************************************************************/

void Delayms(BYTE milliseconds)
{
    BYTE    ms;
    DWORD   count;
    
    ms = milliseconds;
    while (ms--)
    {
        count = MILLISECDELAY;
        while (count--);
    }
    Nop();
    return;
}


/*****************************************************************************
  Function:
    void CloseSPIM (void)
  Summary:
    Disables the SPI module.
  Conditions:
    None.
  Input:
    None.
  Return:
    None.
  Side Effects:
    None.
  Description:
    Disables the SPI module.
  Remarks:
    None.
  ***************************************************************************************/

void CloseSPIM (void)
{
	// changed by ASkr
	SPISTAT &= 0x7FFF;
}



/*****************************************************************************
  Function:
    unsigned char WriteSPIM (unsigned char data_out)
  Summary:
    Writes data to the SD card.
  Conditions:
    None.
  Input:
    data_out - The data to write.
  Return:
    0.
  Side Effects:
    None.
  Description:
    The WriteSPIM function will write a byte of data from the microcontroller to the
    SD card.
  Remarks:
    None.
  ***************************************************************************************/

unsigned char WriteSPIM( unsigned char data_out )
{

   BYTE   clear;
   putcSPI((BYTE)data_out);
   clear = getcSPI();
   return ( 0 );                // return non-negative#
}



/*****************************************************************************
  Function:
    BYTE MDD_SDSPI_ReadMedia (void)
  Summary:
    Reads a byte of data from the SD card.
  Conditions:
    None.
  Input:
    None.
  Return:
    The byte read.
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_ReadMedia function will read one byte from the SPI port.
  Remarks:
    This function replaces ReadSPI, since some implementations of that function
    will initialize SSPBUF/SPIBUF to 0x00 when reading.  The card expects 0xFF.
  ***************************************************************************************/
BYTE MDD_SDSPI_ReadMedia(void)
{
    putcSPI((BYTE)0xFF);
    return (BYTE)getcSPI();
}

/*****************************************************************************
  Function:
    void OpenSPIM (unsigned int sync_mode)
  Summary:
    Initializes the SPI module
  Conditions:
    None.
  Input:
    sync_mode - Sets synchronization
  Return:
    None.
  Side Effects:
    None.
  Description:
    The OpenSPIM function will enable and configure the SPI module.
  Remarks:
    None.
  ***************************************************************************************/

void OpenSPIM( unsigned int sync_mode)
{
  SPISTAT = 0x0000;               // power on state 

  SPICON1bits.CKP = 1;
  SPICON1bits.CKE = 0;

  SPICLOCK = 0;
  SPIOUT = 0;                  // define SDO1 as output (master or slave)
  SPIIN = 1;                  // define SDI1 as input (master or slave)
  SPIENABLE = 1;             // enable synchronous serial port
}

#ifdef __18CXX
#if (GetSystemClock() >= 25600000)

// Description: Delay value for the manual SPI clock
#define MANUAL_SPI_CLOCK_VALUE             1
/*****************************************************************************
  Function:
    unsigned char WriteSPIManual (unsigned char data_out)
  Summary:
    Write a character to the SD card with bit-bang SPI.
  Conditions:
    Make sure the SDI pin is pre-configured as a digital pin, if it is 
    multiplexed with analog functionality.
  Input:
    data_out - Data to send.
  Return:
    0.
  Side Effects:
    None.
  Description:
    Writes a character to the SD card.
  Remarks:
    The WriteSPIManual function is for use on a PIC18 when the clock speed is so
    high that the maximum SPI clock divider cannot reduce the SPI clock speed below
    the maximum SD card initialization speed.
  ***************************************************************************************/
unsigned char WriteSPIManual(unsigned char data_out)
{
    unsigned char i;
    unsigned char clock;

    SPICLOCKLAT = 0;
    SPIOUTLAT = 1;
    SPICLOCK = OUTPUT;
    SPIOUT = OUTPUT;

	//Loop to send out 8 bits of SDO data and associated SCK clock.
	for(i = 0; i < 8; i++)
	{
		SPICLOCKLAT = 0;
		if(data_out & 0x80)
			SPIOUTLAT = 1;
		else
			SPIOUTLAT = 0;
		data_out = data_out << 1;				//Bit shift, so next bit to send is in MSb position
    	clock = MANUAL_SPI_CLOCK_VALUE;
    	while (clock--);
    	SPICLOCKLAT = 1;
    	clock = MANUAL_SPI_CLOCK_VALUE;
    	while (clock--);    			
	}	
    SPICLOCKLAT = 0;

    return 0; 
}


/*****************************************************************************
  Function:
    BYTE ReadMediaManual (void)
  Summary:
    Reads a byte of data from the SD card.
  Conditions:
    None.
  Input:
    None.
  Return:
    The byte read.
  Side Effects:
    None.
  Description:
    The MDD_SDSPI_ReadMedia function will read one byte from the SPI port.
  Remarks:
    This function replaces ReadSPI, since some implementations of that function
    will initialize SSPBUF/SPIBUF to 0x00 when reading.  The card expects 0xFF.
    This function is for use on a PIC18 when the clock speed is so high that the
    maximum SPI clock prescaler cannot reduce the SPI clock below the maximum SD card
    initialization speed.
  ***************************************************************************************/
BYTE ReadMediaManual (void)
{
    unsigned char i;
    unsigned char clock;
    unsigned char result = 0x00;

    SPIOUTLAT = 1;
    SPIOUT = OUTPUT;
    SPIIN = INPUT;
    SPICLOCKLAT = 0;
    SPICLOCK = OUTPUT;
 
 	//Loop to send 8 clock pulses and read in the returned bits of data. Data "sent" will be = 0xFF
	for(i = 0; i < 8; i++)
	{
		SPICLOCKLAT = 0;
    	clock = MANUAL_SPI_CLOCK_VALUE;
    	while (clock--);
    	SPICLOCKLAT = 1;
    	clock = MANUAL_SPI_CLOCK_VALUE;
    	while (clock--);
		result = result << 1;	//Bit shift the previous result.  We receive the byte MSb first. This operation makes LSb = 0.  
    	if(SPIINPORT)
    		result++;			//Set the LSb if we detected a '1' on the SPIINPORT pin, otherwise leave as 0.
	}	
    SPICLOCKLAT = 0;

    return result;
}//end ReadMedia


#endif      // End >25600000
#endif      // End __18CXX


/*****************************************************************************
  Function:
    MEDIA_INFORMATION *  MDD_SDSPI_MediaInitialize (void)
  Summary:
    Initializes the SD card.
  Conditions:
    The MDD_MediaInitialize function pointer must be pointing to this function.
  Input:
    None.
  Return Values:
    The function returns a pointer to the MEDIA_INFORMATION structure.  The
    errorCode member may contain the following values:
        * MEDIA_NO_ERROR - The media initialized successfully
        * MEDIA_CANNOT_INITIALIZE - Cannot initialize the media.  
  Side Effects:
    None.
  Description:
    This function will send initialization commands to and SD card.
  Remarks:
    None.
  ***************************************************************************************/
MEDIA_INFORMATION *  MDD_SDSPI_MediaInitialize(void)
{
    WORD timeout;
    MMC_RESPONSE    response;
	BYTE CSDResponse[20];
	BYTE count, index;
	DWORD c_size;
	BYTE c_size_mult;
	BYTE block_len;
 
   	WORD spiconvalue = 0x0003;

    mediaInformation.errorCode = MEDIA_NO_ERROR;
    mediaInformation.validityFlags.value = 0;
    MDD_SDSPI_finalLBA = 0x00000000;	//Will compute a valid value later, from the CSD register values we get from the card

    SD_CS = 1;               //Initialize Chip Select line
    
    //Media powers up in the open-drain mode and cannot handle a clock faster
    //than 400kHz. Initialize SPI port to slower than 400kHz
		
				OpenSPI(SPI_START_CFG_1, SPI_START_CFG_2);
		    SPIBRG = SPICalutateBRG(GetPeripheralClock(), 400000);
		    

	    // let the card power on and initialize
	    Delayms(1);
	    
	    //Media requires 80 clock cycles to startup [8 clocks/BYTE * 10 us]
	    for(timeout=0; timeout<10; timeout++)
	        mSend8ClkCycles();
	
	    SD_CS = 0;
	    
	    Delayms(1);
	    
	    // Send CMD0 to reset the media
	    response = SendMMCCmd(GO_IDLE_STATE,0x0);
	    
	    if((response.r1._byte == MMC_BAD_RESPONSE) || ((response.r1._byte & 0xF7) != 0x01))
	    {
	        SD_CS = 1;                               // deselect the devices
	        mediaInformation.errorCode = MEDIA_CANNOT_INITIALIZE;
	        return &mediaInformation;
	    }
	
	    response = SendMMCCmd(SEND_IF_COND, 0x1AA);
	    if (((response.r7.bytewise._returnVal & 0xFFF) == 0x1AA) && (!response.r7.bitwise.bits.ILLEGAL_CMD))
		{
	        timeout = 0xFFF;
	        do
	        {
	        	response = SendMMCCmd(SEND_OP_COND, 0x40000000);
	            timeout--;
	        }while(response.r1._byte != 0x00 && timeout != 0);
			response = SendMMCCmd(READ_OCR, 0x0);
	        if (((response.r7.bytewise._returnVal & 0xC0000000) == 0xC0000000) && (response.r7.bytewise._byte == 0))
			{
	        	gSDMode = SD_MODE_HC;
			}
	        else //if (((response.r7.bytewise._returnVal & 0xC0000000) == 0x80000000) && (response.r7.bytewise._byte == 0))
			{
				gSDMode = SD_MODE_NORMAL;
			}
		}
	    else
		{
	        gSDMode = SD_MODE_NORMAL;
	
		    // According to spec cmd1 must be repeated until the card is fully initialized
		    timeout = 0xFFF;
		    do
		    {
		        response = SendMMCCmd(SEND_OP_COND,0x0);
		        timeout--;
		    }while(response.r1._byte != 0x00 && timeout != 0);
		}
	
	    // see if it failed
	    if(timeout == 0)
	    {
	        mediaInformation.errorCode = MEDIA_CANNOT_INITIALIZE;
	        SD_CS = 1;                               // deselect the devices
	    }
	    else      
	    {
	//Common code below for all processors.

        Delayms (2);

         #if (GetSystemClock() <= 20000000)
             SPIBRG = SPICalutateBRG(GetPeripheralClock(), 10000);
         #else
             SPIBRG = SPICalutateBRG(GetPeripheralClock(), SPI_FREQUENCY);
         #endif

		/* Send the CMD9 to read the CSD register */
        timeout = 0xFFF;
        do
        {
			response = SendMMCCmd(SEND_CSD, 0x00);
            timeout--;
        }while((response.r1._byte != 0x00) && (timeout != 0));

		/* According to the simplified spec, section 7.2.6, the card will respond
		with a standard response token, followed by a data block of 16 bytes
		suffixed with a 16-bit CRC.*/
		index = 0;
		for (count = 0; count < 20; count ++)
		{
			CSDResponse[index] = MDD_SDSPI_ReadMedia();
			index ++;			
			/* Hopefully the first byte is the datatoken, however, some cards do
			not send the response token before the CSD register.*/
			if((count == 0) && (CSDResponse[0] == DATA_START_TOKEN))
			{
				/* As the first byte was the datatoken, we can drop it. */
				index = 0;
			}
		}

		//Extract some fields from the response for computing the card capacity.
		//Note: The structure format depends on if it is a CSD V1 or V2 device.
		//Therefore, need to first determine version of the specs that the card 
		//is designed for, before interpreting the individual fields.

 		//-------------------------------------------------------------
 		//READ_BL_LEN: CSD Structure v1 cards always support 512 byte
 		//read and write block lengths.  Some v1 cards may optionally report
 		//READ_BL_LEN = 1024 or 2048 bytes (and therefore WRITE_BL_LEN also 
 		//1024 or 2048).  However, even on these cards, 512 byte partial reads
 		//and 512 byte write are required to be supported.
 		//On CSD structure v2 cards, it is always required that READ_BL_LEN 
 		//(and therefore WRITE_BL_LEN) be 512 bytes, and partial reads and
 		//writes are not allowed.
 		//Therefore, all cards support 512 byte reads/writes, but only a subset
 		//of cards support other sizes.  For best compatibility with all cards,
 		//and the simplest firmware design, it is therefore preferrable to 
 		//simply ignore the READ_BL_LEN and WRITE_BL_LEN values altogether,
 		//and simply hardcode the read/write block size as 512 bytes.
 		//-------------------------------------------------------------
		gMediaSectorSize = 512u;
		mediaInformation.validityFlags.bits.sectorSize = TRUE;
 		mediaInformation.sectorSize = gMediaSectorSize;
 		//-------------------------------------------------------------

		//Calculate the MDD_SDSPI_finalLBA (see SD card physical layer simplified spec 2.0, section 5.3.2).
		//In USB mass storage applications, we will need this information to 
		//correctly respond to SCSI get capacity requests.  Note: method of computing 
		//MDD_SDSPI_finalLBA depends on CSD structure spec version (either v1 or v2).
		if(CSDResponse[0] & 0xC0)	//Check CSD_STRUCTURE field for v2+ struct device
		{
			//Must be a v2 device (or a reserved higher version, that doesn't currently exist)

			//Extract the C_SIZE field from the response.  It is a 22-bit number in bit position 69:48.  This is different from v1.  
			//It spans bytes 7, 8, and 9 of the response.
			c_size = (((DWORD)CSDResponse[7] & 0x3F) << 16) | ((WORD)CSDResponse[8] << 8) | CSDResponse[9];
			
			MDD_SDSPI_finalLBA = ((DWORD)(c_size + 1) * (WORD)(1024u)) - 1; //-1 on end is correction factor, since LBA = 0 is valid.
		}
		else //if(CSDResponse[0] & 0xC0)	//Check CSD_STRUCTURE field for v1 struct device
		{
			//Must be a v1 device.
			//Extract the C_SIZE field from the response.  It is a 12-bit number in bit position 73:62.  
			//Although it is only a 12-bit number, it spans bytes 6, 7, and 8, since it isn't byte aligned.
			c_size = ((DWORD)CSDResponse[6] << 16) | ((WORD)CSDResponse[7] << 8) | CSDResponse[8];	//Get the bytes in the correct positions
			c_size &= 0x0003FFC0;	//Clear all bits that aren't part of the C_SIZE
			c_size = c_size >> 6;	//Shift value down, so the 12-bit C_SIZE is properly right justified in the DWORD.
			
			//Extract the C_SIZE_MULT field from the response.  It is a 3-bit number in bit position 49:47.
			c_size_mult = ((WORD)((CSDResponse[9] & 0x03) << 1)) | ((WORD)((CSDResponse[10] & 0x80) >> 7));

            //Extract the BLOCK_LEN field from the response. It is a 4-bit number in bit position 83:80.
            block_len = CSDResponse[5] & 0x0F;

            block_len = 1 << (block_len - 9); //-9 because we report the size in sectors of 512 bytes each
			
			//Calculate the MDD_SDSPI_finalLBA (see SD card physical layer simplified spec 2.0, section 5.3.2).
			//In USB mass storage applications, we will need this information to 
			//correctly respond to SCSI get capacity requests (which will cause MDD_SDSPI_ReadCapacity() to get called).
			MDD_SDSPI_finalLBA = ((DWORD)(c_size + 1) * (WORD)((WORD)1 << (c_size_mult + 2)) * block_len) - 1;	//-1 on end is correction factor, since LBA = 0 is valid.		
		}	

        // Turn off CRC7 if we can, might be an invalid cmd on some cards (CMD59)
        response = SendMMCCmd(CRC_ON_OFF,0x0);

        // Now set the block length to media sector size. It should be already
        response = SendMMCCmd(SET_BLOCKLEN,gMediaSectorSize);
    }

    return &mediaInformation;
}//end MediaInitialize

