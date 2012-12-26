/******************************************************************************
*
*               Microchip Memory Disk Drive File System
*
******************************************************************************
* FileName:           FSIO.c
* Dependencies:       GenericTypeDefs.h
*                     FSIO.h
*                     Physical interface include file (SD-SPI.h, CF-PMP.h, ...)
*                     string.h
*                     stdlib.h
*                     FSDefs.h
*                     ctype.h
*                     salloc.h
* Processor:          PIC18/PIC24/dsPIC30/dsPIC33/PIC32
* Compiler:           C18/C30/C32
* Company:            Microchip Technology, Inc.
*
* Software License Agreement
*
* The software supplied herewith by Microchip Technology Incorporated
* (the �Company�) for its PICmicro� Microcontroller is intended and
* supplied to you, the Company�s customer, for use solely and
* exclusively on Microchip PICmicro Microcontroller products. The
* software is owned by the Company and/or its supplier, and is
* protected under applicable copyright laws. All rights are reserved.
* Any use in violation of the foregoing restrictions may subject the
* user to criminal sanctions under applicable laws, as well as to
* civil liability for the breach of the terms and conditions of this
* license.
*
* THIS SOFTWARE IS PROVIDED IN AN �AS IS� CONDITION. NO WARRANTIES,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED
* TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE COMPANY SHALL NOT,
* IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR
* CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
*
********************************************************************
 File Description:

 Change History:
  Rev     Description
  -----   -----------
  1.2.5   Fixed bug that prevented writes to alternate FAT tables
  1.2.5   Fixed bug that prevented FAT being updated when media is re-inserted
  1.2.6   Fixed bug that resulted in a bus error when attempts to read a invalid memory region
  1.2.6   Fixed bug that prevented the Windows Explorer to show the Date Creation field for directories
********************************************************************/

#include "Compiler.h"
// ASkr
//#include "MDD File System\FSIO.h"
#include "FSIO.h"
#include "GenericTypeDefs.h"
#include "string.h"
#include "stdlib.h"
#include "ctype.h"
// ASkr
//#include "MDD File System\FSDefs.h"
#include "FSDefs.h"

#ifdef ALLOW_FSFPRINTF
#include "stdarg.h"
#endif

#ifdef FS_DYNAMIC_MEM
   #ifdef __18CXX
      #include "salloc.h"
   #endif
#endif

#ifndef ALLOW_WRITES
   #ifdef ALLOW_FORMATS
      #error Write functions must be enabled to use the format function
   #endif
   #ifdef ALLOW_FSFPRINTF
      #error Write functions must be enabled to use the FSfprintf function
   #endif
#endif

#ifdef USEREALTIMECLOCK
    #ifdef USERDEFINEDCLOCK
        #error Please select only one timestamp clocking mode in FSconfig.h
    #endif
    #ifdef INCREMENTTIMESTAMP
        #error Please select only one timestamp clocking mode in FSconfig.h
    #endif
#elif defined USERDEFINEDCLOCK
    #ifdef INCREMENTTIMESTAMP
        #error Please select only one timestamp clocking mode in FSconfig.h
    #endif
#endif
/*****************************************************************************/
/*                         Global Variables                                  */
/*****************************************************************************/

#ifndef FS_DYNAMIC_MEM
    FSFILE  gFileArray[FS_MAX_FILES_OPEN];      // Array that contains file information (static allocation)
    BYTE    gFileSlotOpen[FS_MAX_FILES_OPEN];   // Array that indicates which elements of gFileArray are available for use
#endif

#if defined(USEREALTIMECLOCK) || defined(USERDEFINEDCLOCK)
// Timing variables
BYTE    gTimeCrtMS;     // Global time variable (for timestamps) used to indicate create time (milliseconds)
WORD    gTimeCrtTime;   // Global time variable (for timestamps) used to indicate create time
WORD    gTimeCrtDate;   // Global time variable (for timestamps) used to indicate create date
WORD    gTimeAccDate;   // Global time variable (for timestamps) used to indicate last access date
WORD    gTimeWrtTime;   // Global time variable (for timestamps) used to indicate last update time
WORD    gTimeWrtDate;   // Global time variable (for timestamps) used to indicate last update date
#endif

DWORD       gLastFATSectorRead = 0xFFFFFFFF;    // Global variable indicating which FAT sector was read last
BYTE        gNeedFATWrite = FALSE;              // Global variable indicating that there is information that needs to be written to the FAT
FSFILE  *   gBufferOwner = NULL;                // Global variable indicating which file is using the data buffer
DWORD       gLastDataSectorRead = 0xFFFFFFFF;   // Global variable indicating which data sector was read last
BYTE        gNeedDataWrite = FALSE;             // Global variable indicating that there is information that needs to be written to the data section
BYTE        nextClusterIsLast = FALSE;          // Global variable indicating that the entries in a directory align with a cluster boundary

BYTE    gBufferZeroed = FALSE;      // Global variable indicating that the data buffer contains all zeros

DWORD   FatRootDirClusterValue;     // Global variable containing the cluster number of the root dir (0 for FAT12/16)

BYTE    FSerrno;                    // Global error variable.  Set to one of many error codes after each function call.

DWORD   TempClusterCalc;            // Global variable used to store the calculated value of the cluster of a specified sector.
BYTE    dirCleared;                 // Global variable used by the "recursive" FSrmdir function to indicate that all subdirectories and files have been deleted from the target directory.
BYTE    recache = FALSE;            // Global variable used by the "recursive" FSrmdir function to indicate that additional cache reads are needed.
FSFILE  tempCWDobj;                 // Global variable used to preserve the current working directory information.
FSFILE  gFileTemp;                  // Global variable used for file operations.

#ifdef ALLOW_DIRS
    FSFILE   cwd;               // Global current working directory
    FSFILE * cwdptr = &cwd;     // Pointer to the current working directory
#endif



BYTE __attribute__ ((aligned(4)))   gDataBuffer[MEDIA_SECTOR_SIZE];     // The global data sector buffer
BYTE __attribute__ ((aligned(4)))   gFATBuffer[MEDIA_SECTOR_SIZE];      // The global FAT sector buffer


#pragma udata

DISK gDiskData;         // Global structure containing device information.



/************************************************************************/
/*                        Structures and defines                        */
/************************************************************************/

// Directory entry structure
typedef struct
{
    char      DIR_Name[DIR_NAMESIZE];           // File name
    char      DIR_Extension[DIR_EXTENSION];     // File extension
    BYTE      DIR_Attr;                         // File attributes
    BYTE      DIR_NTRes;                        // Reserved byte
    BYTE      DIR_CrtTimeTenth;                 // Create time (millisecond field)
    WORD      DIR_CrtTime;                      // Create time (second, minute, hour field)
    WORD      DIR_CrtDate;                      // Create date
    WORD      DIR_LstAccDate;                   // Last access date
    WORD      DIR_FstClusHI;                    // High word of the entry's first cluster number
    WORD      DIR_WrtTime;                      // Last update time
    WORD      DIR_WrtDate;                      // Last update date
    WORD      DIR_FstClusLO;                    // Low word of the entry's first cluster number
    DWORD     DIR_FileSize;                     // The 32-bit file size
}_DIRENTRY;

typedef _DIRENTRY * DIRENTRY;                   // A pointer to a directory entry structure

#define DIRECTORY 0x12          // Value indicating that the CreateFileEntry function will be creating a directory

#define DIRENTRIES_PER_SECTOR   (MEDIA_SECTOR_SIZE / 32)        // The number of directory entries in a sector

// internal errors
#define CE_FAT_EOF            60   // Error that indicates an attempt to read FAT entries beyond the end of the file
#define CE_EOF                61   // Error that indicates that the end of the file has been reached

typedef FSFILE   * FILEOBJ;         // Pointer to an FSFILE object

#ifdef ALLOW_FSFPRINTF

#define _FLAG_MINUS 0x1             // FSfprintf minus flag indicator
#define _FLAG_PLUS  0x2             // FSfprintf plus flag indicator
#define _FLAG_SPACE 0x4             // FSfprintf space flag indicator
#define _FLAG_OCTO  0x8             // FSfprintf octothorpe (hash mark) flag indicator
#define _FLAG_ZERO  0x10            // FSfprintf zero flag indicator
#define _FLAG_SIGNED 0x80           // FSfprintf signed flag indicator

#ifdef __18CXX
    #define _FMT_UNSPECIFIED 0      // FSfprintf unspecified argument size flag
    #define _FMT_LONG 1             // FSfprintf 32-bit argument size flag
    #define _FMT_SHRTLONG 2         // FSfprintf 24-bit argument size flag
    #define _FMT_BYTE   3           // FSfprintf 8-bit argument size flag
#else
    #define _FMT_UNSPECIFIED 0      // FSfprintf unspecified argument size flag
    #define _FMT_LONGLONG 1         // FSfprintf 64-bit argument size flag
    #define _FMT_LONG 2             // FSfprintf 32-bit argument size flag
    #define _FMT_BYTE 3             // FSfprintf 8-bit argument size flag
#endif

#ifdef __18CXX
    static const rom char s_digits[] = "0123456789abcdef";      // FSfprintf table of conversion digits
#else
    static const char s_digits[] = "0123456789abcdef";          // FSfprintf table of conversion digits
#endif

#endif

/************************************************************************************/
/*                               Prototypes                                         */
/************************************************************************************/

DWORD ReadFAT (DISK *dsk, DWORD ccls);
DIRENTRY Cache_File_Entry( FILEOBJ fo, WORD * curEntry, BYTE ForceRead);
BYTE Fill_File_Object(FILEOBJ fo, WORD *fHandle);
DWORD Cluster2Sector(DISK * disk, DWORD cluster);
DIRENTRY LoadDirAttrib(FILEOBJ fo, WORD *fHandle);
#ifdef INCREMENTTIMESTAMP
    void IncrementTimeStamp(DIRENTRY dir);
#elif defined USEREALTIMECLOCK
    void CacheTime (void);
#endif

BYTE ReadByte( BYTE* pBuffer, WORD index );
WORD ReadWord( BYTE* pBuffer, WORD index );
DWORD ReadDWord( BYTE* pBuffer, WORD index );

void FileObjectCopy(FILEOBJ foDest,FILEOBJ foSource);
BYTE ValidateChars (char * FileName, BYTE mode);
BYTE FormatFileName( const char* fileName, char* fN2, BYTE mode);
CETYPE FILEfind( FILEOBJ foDest, FILEOBJ foCompareTo, BYTE cmd, BYTE mode);
BYTE FILEget_next_cluster(FILEOBJ fo, DWORD n);
CETYPE FILEopen (FILEOBJ fo, WORD *fHandle, char type);

// Write functions
#ifdef ALLOW_WRITES
    BYTE Write_File_Entry( FILEOBJ fo, WORD * curEntry);
    BYTE flushData (void);
    CETYPE FILEerase( FILEOBJ fo, WORD *fHandle, BYTE EraseClusters);
    BYTE FILEallocate_new_cluster( FILEOBJ fo, BYTE mode);
    BYTE FAT_erase_cluster_chain (DWORD cluster, DISK * dsk);
    DWORD FATfindEmptyCluster(FILEOBJ fo);
    BYTE FindEmptyEntries(FILEOBJ fo, WORD *fHandle);
    BYTE PopulateEntries(FILEOBJ fo, char *name , WORD *fHandle, BYTE mode);
    CETYPE FILECreateHeadCluster( FILEOBJ fo, DWORD *cluster);
    BYTE EraseCluster(DISK *disk, DWORD cluster);
    CETYPE CreateFirstCluster(FILEOBJ fo);
    DWORD WriteFAT (DISK *dsk, DWORD ccls, DWORD value, BYTE forceWrite);
    CETYPE CreateFileEntry(FILEOBJ fo, WORD *fHandle, BYTE mode);
#endif

// Directory functions
#ifdef ALLOW_DIRS
    BYTE GetPreviousEntry (FSFILE * fo);
    BYTE FormatDirName (char * string, BYTE mode);
    int CreateDIR (char * path);
    BYTE writeDotEntries (DISK * dsk, DWORD dotAddress, DWORD dotdotAddress);
    int eraseDir (char * path);
#ifdef ALLOW_PGMFUNCTIONS
    #ifdef ALLOW_WRITES
        int mkdirhelper (BYTE mode, char * ramptr, const rom char * romptr);
        int rmdirhelper (BYTE mode, char * ramptr, const rom char * romptr, unsigned char rmsubdirs);
    #endif
int chdirhelper (BYTE mode, char * ramptr, const rom char * romptr);
#else
    #ifdef ALLOW_WRITES
        int mkdirhelper (BYTE mode, char * ramptr, char * romptr);
        int rmdirhelper (BYTE mode, char * ramptr, char * romptr, unsigned char rmsubdirs);
    #endif
    int chdirhelper (BYTE mode, char * ramptr, char * romptr);
#endif
#endif

#ifdef ALLOW_FSFPRINTF
    #ifdef __18CXX
        int FSvfprintf (auto FSFILE *handle, auto const rom char *formatString, auto va_list ap);
    #else
        int FSvfprintf (FSFILE *handle, const char *formatString, va_list ap);
    #endif
    int FSputc (char c, FSFILE * file);
    unsigned char str_put_n_chars (FSFILE * handle, unsigned char n, char c);
#endif

BYTE DISKmount( DISK *dsk);
BYTE LoadMBR(DISK *dsk);
BYTE LoadBootSector(DISK *dsk);
DWORD GetFullClusterNumber(DIRENTRY entry);


/*************************************************************************
  Function:
    int FSInit(void)
  Summary:
    Function to initialize the device.
  Conditions:
    The physical device should be connected to the microcontroller.
  Input:
    None
  Return Values:
    TRUE -  Initialization successful
    FALSE - Initialization unsuccessful
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    Initializes the static or dynamic memory slots for holding file
    structures.  Initializes the device with the DISKmount function. Loads
    MBR and boot sector information.  Initializes the current working
    directory to the root directory for the device if directory support
    is enabled.
  Remarks:
    None
  *************************************************************************/

int FSInit(void)
{
    int fIndex;
#ifndef FS_DYNAMIC_MEM
    for( fIndex = 0; fIndex < FS_MAX_FILES_OPEN; fIndex++ )
        gFileSlotOpen[fIndex] = TRUE;
#else
    #ifdef __18CXX
        SRAMInitHeap();
    #endif
#endif

    gBufferZeroed = FALSE;
    gNeedFATWrite = FALSE;             
    gLastFATSectorRead = 0xFFFFFFFF;       
    gLastDataSectorRead = 0xFFFFFFFF;  

    MDD_InitIO();

    if(DISKmount(&gDiskData) == CE_GOOD)
    {
    // Initialize the current working directory to the root
#ifdef ALLOW_DIRS
        cwdptr->dsk = &gDiskData;
        cwdptr->sec = 0;
        cwdptr->pos = 0;
        cwdptr->seek = 0;
        cwdptr->size = 0;
        cwdptr->name[0] = '\\';
        for (fIndex = 1; fIndex < 11; fIndex++)
        {
            cwdptr->name[fIndex] = 0x20;
        }
        cwdptr->entry = 0;
        cwdptr->attributes = ATTR_DIRECTORY;
        // "FatRootDirClusterValue" indicates the root
        cwdptr->dirclus = FatRootDirClusterValue;
        cwdptr->dirccls = FatRootDirClusterValue;
#endif

        FSerrno = 0;
        return TRUE;
    }

    return FALSE;
}


/********************************************************************************
  Function:
    CETYPE FILEfind (FILEOBJ foDest, FILEOBJ foCompareTo, BYTE cmd, BYTE mode)
  Summary
    Finds a file on the device
  Conditions:
    This function should not be called by the user.
  Input:
    foDest -       FSFILE object containing information of the file found
    foCompareTo -  FSFILE object containing the name/attr of the file to be
                   found
    cmd -
        -          LOOK_FOR_EMPTY_ENTRY: Search for empty entry.
        -          LOOK_FOR_MATCHING_ENTRY: Search for matching entry.
    mode -
         -         0: Match file exactly with default attributes.
         -         1: Match file to user-specified attributes.
  Return Values:
    CE_GOOD -            File found.
    CE_FILE_NOT_FOUND -  File not found.
  Side Effects:
    None.
  Description:
    The FILEfind function will sequentially cache directory entries within
    the current working directory into the foDest FSFILE object.  If the cmd
    parameter is specified as LOOK_FOR_EMPTY_ENTRY the search will continue
    until an empty directory entry is found. If the cmd parameter is specified
    as LOOK_FOR_MATCHING_ENTRY these entries will be compared to the foCompareTo
    object until a match is found or there are no more entries in the current
    working directory. If the mode is specified a '0' the attributes of the FSFILE
    entries are irrelevant. If the mode is specified as '1' the attributes of the
    foDest entry must match the attributes specified in the foCompareTo file and
    partial string search characters may bypass portions of the comparison.
  Remarks:
    None
  ********************************************************************************/

CETYPE FILEfind( FILEOBJ foDest, FILEOBJ foCompareTo, BYTE cmd, BYTE mode)
{
    WORD   attrib, compareAttrib;
    WORD   fHandle = foDest->entry;                  // current entry counter
    BYTE   state,index;                              // state of the current object
    CETYPE   statusB = CE_FILE_NOT_FOUND;
    BYTE   character,test;

    // reset the cluster
    foDest->dirccls = foDest->dirclus;
    compareAttrib = 0xFFFF ^ foCompareTo->attributes;                // Attribute to be compared as per application layer request

    if (fHandle == 0)
    {
        if (Cache_File_Entry(foDest, &fHandle, TRUE) == NULL)
        {
            statusB = CE_BADCACHEREAD;
        }
    }
    else
    {
        if ((fHandle & MASK_MAX_FILE_ENTRY_LIMIT_BITS) != 0)          // Maximum 16 entries possible
        {
            if (Cache_File_Entry (foDest, &fHandle, TRUE) == NULL)
            {
                statusB = CE_BADCACHEREAD;
            }
        }
    }

    if (statusB != CE_BADCACHEREAD)
    {
        // Loop until you reach the end or find the file
        while(1)
        {
            if(statusB!=CE_GOOD) //First time entry always here
            {
                state = Fill_File_Object(foDest, &fHandle);
                if(state == NO_MORE) // Reached the end of available files. Comparision over and file not found so quit.
                {
                    break;
                }
            }
            else // statusB == CE_GOOD then exit
            {
                break; // Code below intializes"statusB = CE_GOOD;" so, if no problem in the filled file, Exit the while loop.
            }

            if(state == FOUND) // Validate the correct matching of filled file data with the required(to be found) one.
            {
                /* We got something */
                // get the attributes
                attrib = foDest->attributes;

                attrib &= ATTR_MASK;
                switch (mode)
                {
                    case 0:
                        // see if we are a volume id or hidden, ignore
                        if(attrib != ATTR_VOLUME)
                        {
                            statusB = CE_GOOD;
                            character = (BYTE)'m'; // random value

                            // search for one. if status = TRUE we found one
                            for(index = 0; index < DIR_NAMECOMP; index++)
                            {
                                // get the source character
                                character = foDest->name[index];
                                // get the destination character
                                test = foCompareTo->name[index];
                                if(tolower(character) != tolower(test))
                                {
                                    statusB = CE_FILE_NOT_FOUND; // Nope its not a match
                                    break;
                                }
                            }// for loop
                        } // not dir nor vol
                        break;

                    case 1:
                        // Check for attribute match
                        if (((attrib & compareAttrib) == 0) && (attrib != ATTR_LONG_NAME))
                        {
                            statusB = CE_GOOD;                 // Indicate the already filled file data is correct and go back
                            character = (BYTE)'m';             // random value
                            if (foCompareTo->name[0] != '*')   //If "*" is passed for comparion as 1st char then don't proceed. Go back, file alreay found.
                            {
                                for (index = 0; index < DIR_NAMESIZE; index++)
                                {
                                    // Get the source character
                                    character = foDest->name[index];
                                    // Get the destination character
                                    test = foCompareTo->name[index];
                                    if (test == '*')
                                        break;
                                    if (test != '?')
                                    {
                                        if(tolower(character) != tolower(test))
                                        {
                                            statusB = CE_FILE_NOT_FOUND; // it's not a match
                                            break;
                                        }
                                    }
                                }
                            }

                            // Before calling this "FILEfind" fn, "formatfilename" must be called. Hence, extn always starts from position "8".
                            if ((foCompareTo->name[8] != '*') && (statusB == CE_GOOD))
                            {
                                for (index = 8; index < DIR_NAMECOMP; index++)
                                {
                                    // Get the source character
                                    character = foDest->name[index];
                                    // Get the destination character
                                    test = foCompareTo->name[index];
                                    if (test == '*')
                                        break;
                                    if (test != '?')
                                    {
                                        if(tolower(character) != tolower(test))
                                        {
                                            statusB = CE_FILE_NOT_FOUND; // it's not a match
                                            break;
                                        }
                                    }
                                }
                            }

                        } // Attribute match

                        break;
                }
            } // not found
            else
            {
                /*** looking for an empty/re-usable entry ***/
                if ( cmd == LOOK_FOR_EMPTY_ENTRY)
                    statusB = CE_GOOD;
            } // found or not

            // increment it no matter what happened
            fHandle++;

        }// while
    }

    return(statusB);
} // FILEFind


/**************************************************************************
  Function:
    CETYPE FILEopen (FILEOBJ fo, WORD *fHandle, char type)
  Summary:
    Loads file information from the device
  Conditions:
    This function should not be called by the user.
  Input:
    fo -       File to be opened
    fHandle -  Location of file
    type -
         -     WRITE -  Create a new file or replace an existing file
         -     READ -   Read data from an existing file
         -     APPEND - Append data to an existing file
  Return Values:
    CE_GOOD -            FILEopen successful
    CE_NOT_INIT -        Device is not yet initialized
    CE_FILE_NOT_FOUND -  Could not find the file on the device
    CE_BAD_SECTOR_READ - A bad read of a sector occured
  Side Effects:
    None
  Description:
    This function will cache a directory entry in the directory specified
    by the dirclus parameter of hte FSFILE object 'fo.'  The offset of the
    entry in the directory is specified by fHandle.  Once the directory entry
    has been loaded, the first sector of the file can be loaded using the
    cluster value specified in the directory entry. The type argument will
    specify the mode the files will be opened in.  This will allow this
    function to set the correct read/write flags for the file.
  Remarks:
    If the mode the file is being opened in is a plus mode (e.g. READ+) the
    flags will be modified further in the FSfopen function.
  **************************************************************************/

CETYPE FILEopen (FILEOBJ fo, WORD *fHandle, char type)
{
    DISK   *dsk;      //Disk structure
    BYTE    r;               //Result of search for file
    DWORD    l;               //lba of first sector of first cluster
    CETYPE    error = CE_GOOD;

    dsk = (DISK *)(fo->dsk);
    if (dsk->mount == FALSE)
    {
        error = CE_NOT_INIT;
    }
    else
    {
        // load the sector
        fo->dirccls = fo->dirclus;
        // Cache no matter what if it's the first entry
        if (*fHandle == 0)
        {
            if (Cache_File_Entry(fo, fHandle, TRUE) == NULL)
            {
                error = CE_BADCACHEREAD;
            }
        }
        else
        {
            // If it's not the first, only cache it if it's
            // not divisible by the number of entries per sector
            // If it is, Fill_File_Object will cache it
            if ((*fHandle & 0xf) != 0)
            {
                if (Cache_File_Entry (fo, fHandle, TRUE) == NULL)
                {
                    error = CE_BADCACHEREAD;
                }
            }
        }

        // Fill up the File Object with the information pointed to by fHandle
        r = Fill_File_Object(fo, fHandle);
        if (r != FOUND)
            error = CE_FILE_NOT_FOUND;
        else
        {
            fo->seek = 0;               // first byte in file
            fo->ccls = fo->cluster;     // first cluster
            fo->sec = 0;                // first sector in the cluster
            fo->pos = 0;                // first byte in sector/cluster

            if  ( r == NOT_FOUND)
            {
                error = CE_FILE_NOT_FOUND;
            }
            else
            {
                // Determine the lba of the selected sector and load
                l = Cluster2Sector(dsk,fo->ccls);
#ifdef ALLOW_WRITES
                if (gNeedDataWrite)
                    if (flushData())
                        return CE_WRITE_ERROR;
#endif
                gBufferOwner = fo;
                if (gLastDataSectorRead != l)
                {
                    gBufferZeroed = FALSE;
                    if ( !MDD_SectorRead( l, dsk->buffer))
                        error = CE_BAD_SECTOR_READ;
                    gLastDataSectorRead = l;
                }
            } // -- found

            fo->flags.FileWriteEOF = FALSE;
            // Set flag for operation type
#ifdef ALLOW_WRITES
            if (type == 'w' || type == 'a')
            {
                fo->flags.write = 1;   //write or append
                fo->flags.read = 0;
            }
            else
            {
#endif
                fo->flags.write = 0;   //read
                fo->flags.read = 1;
#ifdef ALLOW_WRITES
            } // -- flags
#endif
        } // -- r = Found
    } // -- Mounted
    return (error);
} // -- FILEopen


/*************************************************************************
  Function:
    BYTE FILEget_next_cluster(FILEOBJ fo, WORD n)
  Summary:
    Step through a chain of clusters
  Conditions:
    This function should not be called by the user.
  Input:
    fo - The file to get the next cluster of
    n -  Number of links in the FAT cluster chain to jump through
  Return Values:
    CE_GOOD - Operation successful
    CE_BAD_SECTOR_READ - A bad read occured of a sector
    CE_INVALID_CLUSTER - Invalid cluster value \> maxcls
    CE_FAT_EOF - Fat attempt to read beyond EOF
  Side Effects:
    None
  Description:
    This function will load 'n' proximate clusters for a file from
    the FAT on the device.  It will stop checking for clusters if the
    ReadFAT function returns an error, if it reaches the last cluster in
    a file, or if the device tries to read beyond the last cluster used
    by the device.
  Remarks:
    None
  *************************************************************************/

BYTE FILEget_next_cluster(FILEOBJ fo, DWORD n)
{
    DWORD         c, c2, ClusterFailValue, LastClustervalue;
    BYTE          error = CE_GOOD;
    DISK *      disk;

    disk = fo->dsk;

    /* Settings based on FAT type */
    switch (disk->type)
    {
#ifdef SUPPORT_FAT32 // If FAT32 supported.
        case FAT32:
            LastClustervalue = LAST_CLUSTER_FAT32;
            ClusterFailValue  = CLUSTER_FAIL_FAT32;
            break;
#endif
        case FAT12:
            LastClustervalue = LAST_CLUSTER_FAT12;
            ClusterFailValue  = CLUSTER_FAIL_FAT16;
            break;
        case FAT16:
        default:
            LastClustervalue = LAST_CLUSTER_FAT16;
            ClusterFailValue  = CLUSTER_FAIL_FAT16;
            break;
    }

    // loop n times
    do
    {
        // get the next cluster link from FAT
        c2 = fo->ccls;
        if ( (c = ReadFAT( disk, c2)) == ClusterFailValue)
            error = CE_BAD_SECTOR_READ;
        else
        {
            // check if cluster value is valid
            if ( c >= disk->maxcls)
            {
                error = CE_INVALID_CLUSTER;
            }

            // compare against max value of a cluster in FAT
            // return if eof
            if ( c >= LastClustervalue)    // check against eof
            {
                error = CE_FAT_EOF;
            }
        }

        // update the FSFILE structure
        fo->ccls = c;

    } while (--n > 0 && error == CE_GOOD);// loop end

    return(error);
} // get next cluster


/**************************************************************************
  Function:
    BYTE DISKmount ( DISK *dsk)
  Summary:
    Initialies the device and loads MBR and boot sector information
  Conditions:
    This function should not be called by the user.
  Input:
    dsk -  The disk structure to be initialized.
  Return Values:
    CE_GOOD -       Disk mounted
    CE_INIT_ERROR - Initialization error has occured
    CE_UNSUPPORTED_SECTOR_SIZE - Media sector size bigger than
                MEDIA_SECTOR_SIZE as defined in FSconfig.h.
  Side Effects:
    None
  Description:
    This function will use the function pointed to by the MDD_MediaInitialize
    function pointer to initialize the device (if any initialization is
    required).  It then attempts to load the master boot record with the
    LoadMBR function and the boot sector with the LoadBootSector function.
    These two functions will be used to initialize a global DISK structure
    that will be used when accessing file information in the future.
  Remarks:
    None
  **************************************************************************/

BYTE DISKmount( DISK *dsk)
{
    BYTE                error = CE_GOOD;
    MEDIA_INFORMATION   *mediaInformation;

    dsk->mount = FALSE; // default invalid
    dsk->buffer = gDataBuffer;    // assign buffer

    // Initialize the device
    mediaInformation = MDD_MediaInitialize();
    if (mediaInformation->errorCode != MEDIA_NO_ERROR)
    {
        error = CE_INIT_ERROR;
        FSerrno = CE_INIT_ERROR;
    }
    else
    {
        // If the media initialization routine determined the sector size,
        // check it and make sure we can support it.
        if (mediaInformation->validityFlags.bits.sectorSize)
        {
			dsk->sectorSize = mediaInformation->sectorSize;
            if (mediaInformation->sectorSize > MEDIA_SECTOR_SIZE)
            {
                error = CE_UNSUPPORTED_SECTOR_SIZE;
                FSerrno = CE_UNSUPPORTED_SECTOR_SIZE;
                return error;
            }
        }

        // Load the Master Boot Record (partition)
        if((error = LoadMBR(dsk)) == CE_GOOD)
        {
            // Now the boot sector
            if((error = LoadBootSector(dsk)) == CE_GOOD)
                dsk->mount = TRUE; // Mark that the DISK mounted successfully
        }
    } // -- Load file parameters

    return(error);
} // -- mount



/********************************************************************
  Function:
    CETYPE LoadMBR ( DISK *dsk)
  Summary:
    Loads the MBR and extracts necessary information
  Conditions:
    This function should not be called by the user.
  Input:
    dsk -  The disk containing the master boot record to be loaded
  Return Values:
    CE_GOOD -            MBR loaded successfully
    CE_BAD_SECTOR_READ - A bad read occured of a sector
    CE_BAD_PARTITION -   The boot record is bad
  Side Effects:
    None
  Description:
    The LoadMBR function will use the function pointed to by the
    MDD_SectorRead function pointer to read the 0 sector from the
    device.  If a valid boot signature is obtained, this function
    will compare fields in that cached sector to the values that
    would be present if that sector was a boot sector.  If all of
    those values match, it will be assumed that the device does not
    have a master boot record and the 0 sector is actually the boot
    sector.  Otherwise, data about the partition and the actual
    location of the boot sector will be loaded from the MBR into
    the DISK structure pointed to by 'dsk.'
  Remarks:
    None
  ********************************************************************/

BYTE LoadMBR(DISK *dsk)
{
    PT_MBR  Partition;
    BYTE error = CE_GOOD;
    BYTE type;
    BootSec BSec;

    // Get the partition table from the MBR
    if ( MDD_SectorRead( FO_MBR, dsk->buffer) != TRUE)
    {
        error = CE_BAD_SECTOR_READ;
        FSerrno = CE_BAD_SECTOR_READ;
    }
    else
    {
        // Check if the card has no MBR
        BSec = (BootSec) dsk->buffer;

        if((BSec->Signature0 == FAT_GOOD_SIGN_0) && (BSec->Signature1 == FAT_GOOD_SIGN_1))
        {
         // Technically, the OEM name is not for indication
         // The alternative is to read the CIS from attribute
         // memory.  See the PCMCIA metaformat for more details
            if (ReadByte( dsk->buffer, BSI_FSTYPE ) == 'F' && \
            ReadByte( dsk->buffer, BSI_FSTYPE + 1 ) == 'A' && \
            ReadByte( dsk->buffer, BSI_FSTYPE + 2 ) == 'T' && \
            ReadByte( dsk->buffer, BSI_FSTYPE + 3 ) == '1' && \
            ReadByte( dsk->buffer, BSI_BOOTSIG) == 0x29)
             {
                dsk->firsts = 0;
                dsk->type = FAT16;
                return CE_GOOD;
             }
             else
             {
                if (ReadByte( dsk->buffer, BSI_FAT32_FSTYPE ) == 'F' && \
                    ReadByte( dsk->buffer, BSI_FAT32_FSTYPE + 1 ) == 'A' && \
                    ReadByte( dsk->buffer, BSI_FAT32_FSTYPE + 2 ) == 'T' && \
                    ReadByte( dsk->buffer, BSI_FAT32_FSTYPE + 3 ) == '3' && \
                    ReadByte( dsk->buffer, BSI_FAT32_BOOTSIG) == 0x29)
                {
                    dsk->firsts = 0;
                    dsk->type = FAT32;
                    return CE_GOOD;
                }
            }
        }
        // assign it the partition table strucutre
        Partition = (PT_MBR)dsk->buffer;

        // Ensure its good
        if((Partition->Signature0 != FAT_GOOD_SIGN_0) || (Partition->Signature1 != FAT_GOOD_SIGN_1))
        {
            FSerrno = CE_BAD_PARTITION;
            error = CE_BAD_PARTITION;
        }
        else
        {
            /*    Valid Master Boot Record Loaded   */

            // Get the 32 bit offset to the first partition
            dsk->firsts = Partition->Partition0.PTE_FrstSect;

            // check if the partition type is acceptable
              type = Partition->Partition0.PTE_FSDesc;

            switch (type)
            {
                case 0x01:
                    dsk->type = FAT12;
                    break;

            case 0x04:
                case 0x06:
                case 0x0E:
                    dsk->type = FAT16;
                    break;

                case 0x0B:
                case 0x0C:

#ifdef SUPPORT_FAT32 // If FAT32 supported.
            dsk->type = FAT32;    // FAT32 is supported too
#else
            FSerrno = CE_CARDFAT32;
            error = CE_CARDFAT32;
#endif
                    break;

                default:
                    FSerrno = CE_UNSUPPORTED_FS;
                    error = CE_UNSUPPORTED_FS;
            } // switch
        }
    }

    return(error);
}// -- LoadMBR


/**************************************************************************
  Function:
    BYTE LoadBootSector (DISK *dsk)
  Summary:
    Load the boot sector and extract the necessary information
  Conditions:
    This function should not be called by the user.
  Input:
    dsk -  The disk containing the boot sector
  Return Values:
    CE_GOOD -                    Boot sector loaded
    CE_BAD_SECTOR_READ -         A bad read occured of a sector
    CE_NOT_FORMATTED -           The disk is of an unsupported format
    CE_CARDFAT32 -               FAT 32 device not supported
    CE_UNSUPPORTED_SECTOR_SIZE - The sector size is not supported
  Side Effects:
    None
  Description:
    LoadBootSector will use the function pointed to by the MDD_SectorWrite
    function pointer to load the boot sector, whose location was obtained
    by a previous call of LoadMBR.  If the boot sector is loaded successfully,
    partition information will be calcualted from it and copied into the DISK
    structure pointed to by 'dsk.'
  Remarks:
    None
  **************************************************************************/


BYTE LoadBootSector(DISK *dsk)
{
    DWORD       RootDirSectors;
    DWORD       TotSec,DataSec;
    BYTE        error = CE_GOOD;
    BootSec     BSec;
    WORD        BytesPerSec;
    WORD        ReservedSectorCount;

    // Get the Boot sector
    if ( MDD_SectorRead( dsk->firsts, dsk->buffer) != TRUE)
    {
        FSerrno = CE_BAD_SECTOR_READ;
        error = CE_BAD_SECTOR_READ;
    }
    else
    {
        BSec = (BootSec)dsk->buffer;

        //Verify the Boot Sector is valid
        if((BSec->Signature0 != FAT_GOOD_SIGN_0) || (BSec->Signature1 != FAT_GOOD_SIGN_1))
        {
            FSerrno = CE_NOT_FORMATTED;
            error = CE_NOT_FORMATTED;
        }
        else
        {
            #ifdef __18CXX

                // Load count of sectors per cluster
                dsk->SecPerClus = BSec->FAT.FAT_16.BootSec_SPC;
                // Load the sector number of the first FAT sector
                dsk->fat        = dsk->firsts + BSec->FAT.FAT_16.BootSec_ResrvSec;
                // Load the count of FAT tables
                dsk->fatcopy    = BSec->FAT.FAT_16.BootSec_FATCount;
                // Load the size of the FATs
                dsk->fatsize = BSec->FAT.FAT_16.BootSec_SPF;
                if(dsk->fatsize == 0)
                    dsk->fatsize  = BSec->FAT.FAT_32.BootSec_FATSz32;
                // Calculate the location of the root sector (for FAT12/16)
                dsk->root = dsk->fat + (DWORD)(dsk->fatcopy * (DWORD)dsk->fatsize);
                // Determine the max size of the root (will be 0 for FAT32)
                dsk->maxroot    = BSec->FAT.FAT_16.BootSec_RootDirEnts;

                // Determine the total number of sectors in the partition
                if(BSec->FAT.FAT_16.BootSec_TotSec16 != 0)
                {
                    TotSec = BSec->FAT.FAT_16.BootSec_TotSec16;
                }
                else
                {
                    TotSec = BSec->FAT.FAT_16.BootSec_TotSec32;
                }

                // Calculate the number of bytes in each sector
                BytesPerSec = BSec->FAT.FAT_16.BootSec_BPS;
                if( BytesPerSec == 0 || (BytesPerSec & 1) == 1 )
                {
                    FSerrno = CE_UNSUPPORTED_SECTOR_SIZE;
                    return( CE_UNSUPPORTED_SECTOR_SIZE );
                }

                // Calculate the number of sectors in the root (will be 0 for FAT32)
                RootDirSectors = ((BSec->FAT.FAT_16.BootSec_RootDirEnts * 32) + (BSec->FAT.FAT_16.BootSec_BPS - 1)) / BSec->FAT.FAT_16.BootSec_BPS;
                // Calculate the number of data sectors on the card
                DataSec = TotSec - (dsk->root + RootDirSectors);
                // Calculate the maximum number of clusters on the card
                dsk->maxcls = DataSec / dsk->SecPerClus;

            #else // PIC24/30/33

                // Read the count of reserved sectors
                ReservedSectorCount = ReadWord( dsk->buffer, BSI_RESRVSEC );
                // Load the count of sectors per cluster
                dsk->SecPerClus = ReadByte( dsk->buffer, BSI_SPC );
                // Load the sector number of the first FAT sector
                dsk->fat = dsk->firsts + ReservedSectorCount;
                // Load the count of FAT tables
                dsk->fatcopy    = ReadByte( dsk->buffer, BSI_FATCOUNT );
                // Load the size of the FATs
                dsk->fatsize = ReadWord( dsk->buffer, BSI_SPF );
                if(dsk->fatsize == 0)
                    dsk->fatsize  = ReadDWord( dsk->buffer, BSI_FATSZ32 );
                // Calculate the location of the root sector (for FAT12/16)
                dsk->root = dsk->fat + (DWORD)(dsk->fatcopy * (DWORD)dsk->fatsize);
                // Determine the max size of the root (will be 0 for FAT32)
                dsk->maxroot = ReadWord( dsk->buffer, BSI_ROOTDIRENTS );

                // Determine the total number of sectors in the partition
                TotSec = ReadWord( dsk->buffer, BSI_TOTSEC16 );
                if( TotSec == 0 )
                    TotSec = ReadDWord( dsk->buffer, BSI_TOTSEC32 );

                // Calculate the number of bytes in each sector
                BytesPerSec = ReadWord( dsk->buffer, BSI_BPS );
                if( BytesPerSec == 0 || (BytesPerSec & 1) == 1 )
                {
                    FSerrno = CE_UNSUPPORTED_SECTOR_SIZE;
                    return( CE_UNSUPPORTED_SECTOR_SIZE );
                }

                // Calculate the number of sectors in the root (will be 0 for FAT32)
                RootDirSectors = ((dsk->maxroot * NUMBER_OF_BYTES_IN_DIR_ENTRY) + (BytesPerSec - 1)) / BytesPerSec;
                // Calculate the number of data sectors on the card
                DataSec = TotSec - (ReservedSectorCount + (dsk->fatcopy * dsk->fatsize )  + RootDirSectors);
                // Calculate the maximum number of clusters on the card
                dsk->maxcls = DataSec / dsk->SecPerClus;

            #endif

            // Determine the file system type based on the number of clusters used
            if(dsk->maxcls < 4085)
            {
                dsk->type = FAT12;
            }
            else
            {
                if(dsk->maxcls < 65525)
                {
                    dsk->type = FAT16;
                }
                else
                {
                    #ifdef SUPPORT_FAT32
                        dsk->type = FAT32;
                    #else
                        error = CE_CARDFAT32;
                        FSerrno = CE_CARDFAT32;
                    #endif
                }
            }

            #ifdef SUPPORT_FAT32
                if (dsk->type == FAT32)
                {
                    #ifdef __18CXX
                        FatRootDirClusterValue =  BSec->FAT.FAT_32.BootSec_RootClus;
                    #else
                        FatRootDirClusterValue = ReadDWord( dsk->buffer, BSI_ROOTCLUS );
                    #endif
                    dsk->data = dsk->root + RootDirSectors;
                }
                else
            #endif
            {
                FatRootDirClusterValue = 0;
                dsk->data = dsk->root + ( dsk->maxroot >> 4);
            }

            #ifdef __18CXX
                if(BSec->FAT.FAT_16.BootSec_BPS > MEDIA_SECTOR_SIZE)
            #else
                if(BytesPerSec > MEDIA_SECTOR_SIZE)
            #endif
                {
                    error = CE_UNSUPPORTED_SECTOR_SIZE;
                    FSerrno = CE_UNSUPPORTED_SECTOR_SIZE;
                }
        }
    }
    return(error);
}



/*************************************************************************
  Function:
    DWORD GetFullClusterNumber (DIRENTRY entry)
  Summary:
    Gets the cluster number from a directory entry
  Conditions:
    This function should not be called by the user.
  Input:
    entry - The cached directory entry to get the cluster number from
  Returns:
    The cluster value from the passed directory entry
  Side Effects:
    None.
  Description:
    This function will load both the high and low 16-bit first cluster
    values of a file from a directory entry and copy them into a 32-bit
    cluster number variable, which will be returned.
  Remarks:
    None
  *************************************************************************/

DWORD GetFullClusterNumber(DIRENTRY entry)
{

    DWORD TempFullClusterCalc = 0;

#ifndef SUPPORT_FAT32 // If FAT32 Not supported.
    entry->DIR_FstClusHI = 0; // If FAT32 is not supported then Higher Word of the address is "0"
#endif

    // Get the cluster
    TempFullClusterCalc = (entry->DIR_FstClusHI);
    TempFullClusterCalc = TempFullClusterCalc << 16;
    TempFullClusterCalc |= entry->DIR_FstClusLO;

    return TempFullClusterCalc;
}


#ifdef ALLOW_FORMATS
#ifdef ALLOW_WRITES


/*********************************************************************************
  Function:
    int FSCreateMBR (unsigned long firstSector, unsigned long numSectors)
  Summary:
    Creates a master boot record
  Conditions:
    The I/O pins for the device have been initialized by the InitIO function.
  Input:
    firstSector -  The first sector of the partition on the device (cannot
                   be 0; that's the MBR)
    numSectors -   The number of sectors available in memory (including the
                   MBR)
  Return Values:
    0 -   MBR was created successfully
    EOF - MBR could not be created
  Side Effects:
    None
  Description:
    This function can be used to create a master boot record for a device.  Note
    that this function should not be used on a device that is already formatted
    with a master boot record (i.e. most SD cards, CF cards, USB keys).  This
    function will fill the global data buffer with appropriate partition information
    for a FAT partition with a type determined by the number of sectors available
    to the partition.  It will then write the MBR information to the first sector
    on the device.  This function should be followed by a call to FSformat, which
    will create a boot sector, root dir, and FAT appropriate the the information
    contained in the new master boot record.  Note that FSformat only supports
    FAT12 and FAT16 formatting at this time, and so cannot be used to format a
    device with more than 0x3FFD5F sectors.
  Remarks:
    This function can damage the device being used, and should not be called
    unless the user is sure about the size of the device and the first sector value.
  *********************************************************************************/

int FSCreateMBR (unsigned long firstSector, unsigned long numSectors)
{
    PT_MBR  Partition;
    DWORD CyHdSc = 0x00000000;
    DWORD tempSector;

    if ((firstSector == 0) || (numSectors <= 1))
        return EOF;

    if (firstSector > (numSectors - 1))
        return EOF;

    if (gNeedDataWrite)
        if (flushData())
            return EOF;

    memset (gDataBuffer, 0x00, MEDIA_SECTOR_SIZE);

    Partition = (PT_MBR) gDataBuffer;

    // Set Cylinder-head-sector address of the first sector
    tempSector = firstSector;
    CyHdSc = (tempSector / (unsigned int)16065 ) << 14;
    tempSector %= 16065;
    CyHdSc |= (tempSector / 63) << 6;
    tempSector %= 63;
    CyHdSc |= tempSector + 1;
    gDataBuffer[447] = (BYTE)((CyHdSc >> 16) & 0xFF);
    gDataBuffer[448] = (BYTE)((CyHdSc >> 8) & 0xFF);
    gDataBuffer[449] = (BYTE)((CyHdSc) & 0xFF);

    // Set the count of sectors
    Partition->Partition0.PTE_NumSect = numSectors - firstSector;

    // Set the partition type
    // We only support creating FAT12 and FAT16 MBRs at this time
    if (Partition->Partition0.PTE_NumSect < 0x1039)
    {
        // FAT12
        Partition->Partition0.PTE_FSDesc = 0x01;
    }
    else if (Partition->Partition0.PTE_NumSect <= 0x3FFD5F)
    {
        // FAT16
        Partition->Partition0.PTE_FSDesc = 0x06;
    }
    else
        return EOF;

    // Set the LBA of the first sector
    Partition->Partition0.PTE_FrstSect = firstSector;

    // Set the Cylinder-head-sector address of the last sector
    tempSector = firstSector + numSectors - 1;
    CyHdSc = (tempSector / (unsigned int)16065 ) << 14;
    tempSector %= 16065;
    CyHdSc |= (tempSector / 63) << 6;
    tempSector %= 63;
    CyHdSc |= tempSector + 1;
    gDataBuffer[451] = (BYTE)((CyHdSc >> 16) & 0xFF);
    gDataBuffer[452] = (BYTE)((CyHdSc >> 8) & 0xFF);
    gDataBuffer[453] = (BYTE)((CyHdSc) & 0xFF);

    // Set the boot descriptor.  This will be 0, since we won't
    // be booting anything from our device probably
    Partition->Partition0.PTE_BootDes = 0x00;

    // Set the signature codes
    Partition->Signature0 = 0x55;
    Partition->Signature1 = 0xAA;

    if (MDD_SectorWrite (0x00, gDataBuffer, TRUE) != TRUE)
        return EOF;
    else
        return 0;

}



/*******************************************************************
  Function:
    int FSformat (char mode, long int serialNumber, char * volumeID)
  Summary:
    Formats a device
  Conditions:
    The device must possess a valid master boot record.
  Input:
    mode -          - 0 - Just erase the FAT and root
                    - 1 - Create a new boot sector
    serialNumber -  Serial number to write to the card
    volumeID -      Name of the card
  Return Values:
    0 -    Format was successful
    EOF -  Format was unsuccessful
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FSformat function can be used to create a new boot sector
    on a device, based on the information in the master boot record.
    This function will first initialize the I/O pins and the device,
    and then attempts to read the master boot record.  If the MBR
    cannot be loaded successfully, the function will fail.  Next, if
    the 'mode' argument is specified as '0' the existing boot sector
    information will be loaded.  If the 'mode' argument is '1' an
    entirely new boot sector will be constructed using the disk
    values from the master boot record.  Once the boot sector has
    been successfully loaded/created, the locations of the FAT and
    root will be loaded from it, and they will be completely
    erased.  If the user has specified a volumeID parameter, a
    VOLUME attribute entry will be created in the root directory
    to name the device.

    FAT12, FAT16 and FAT32 formatting are supported.

    Based on the number of sectors, the format function automatically
    compute the smallest possible value for the cluster size in order to
    accommodate the physical size of the media. In this case, if a media 
    with a big capacity is formatted, the format function may take a very
    long time to write all the FAT tables. 

    Therefore, the FORMAT_SECTORS_PER_CLUSTER macro may be used to 
    specify the exact cluster size (in multiples of sector size). This 
    macro can be defined in FSconfig.h

  Remarks:
    Only devices with a sector size of 512 bytes are supported by the 
    format function
  *******************************************************************/

int FSformat (char mode, long int serialNumber, char * volumeID)
{
    PT_MBR   masterBootRecord;
    DWORD    secCount, DataClusters, RootDirSectors;
    BootSec   BSec;
    DISK   d;
    DISK * disk = &d;
    WORD    j;
    DWORD   fatsize, test;
    DWORD Index;
    MEDIA_INFORMATION * mediaInfo;
#ifdef __18CXX
    // This is here because of a C18 compiler feature
    BYTE *  dataBufferPointer = gDataBuffer;
#endif

    FSerrno = CE_GOOD;

    gBufferZeroed = FALSE;
    gNeedFATWrite = FALSE;             
    gLastFATSectorRead = 0xFFFFFFFF;       
    gLastDataSectorRead = 0xFFFFFFFF;  

    disk->buffer = gDataBuffer;

    MDD_InitIO();

    mediaInfo = MDD_MediaInitialize();
    if (mediaInfo->errorCode != MEDIA_NO_ERROR)
    {
        FSerrno = CE_INIT_ERROR;
        return EOF;
    }

    if (MDD_SectorRead (0x00, gDataBuffer) == FALSE)
    {
        FSerrno = CE_BADCACHEREAD;
        return EOF;
    }

    // Check if the card has no MBR
    BSec = (BootSec) disk->buffer;
    if((BSec->Signature0 == FAT_GOOD_SIGN_0) && (BSec->Signature1 == FAT_GOOD_SIGN_1))
    {
        // Technically, the OEM name is not for indication
        // The alternative is to read the CIS from attribute
        // memory.  See the PCMCIA metaformat for more details
        if (ReadByte( disk->buffer, BSI_FSTYPE ) == 'F' && \
            ReadByte( disk->buffer, BSI_FSTYPE + 1 ) == 'A' && \
            ReadByte( disk->buffer, BSI_FSTYPE + 2 ) == 'T' && \
            ReadByte( disk->buffer, BSI_FSTYPE + 3 ) == '1' && \
            ReadByte( disk->buffer, BSI_BOOTSIG) == 0x29)
        {
            /* Mark that we do not have a MBR; 
                this is not actualy used - is here only to remove a compilation warning */
            masterBootRecord = (PT_MBR) NULL;
            switch (mode)
            {
                case 1:
                    // not enough info to construct our own boot sector
                    FSerrno = CE_INVALID_ARGUMENT;
                    return EOF;
                case 0:
                    // We have to determine the operating system, and the
                    // locations and sizes of the root dir and FAT, and the
                    // count of FATs
                    disk->firsts = 0;
                    if (LoadBootSector (disk) != CE_GOOD)
                    {
                        FSerrno = CE_BADCACHEREAD;
                        return EOF;
                    }
                default:
                    break;
            }
        }
        else
        {
            masterBootRecord = (PT_MBR) &gDataBuffer;
            disk->firsts = masterBootRecord->Partition0.PTE_FrstSect;
        }
    }
    else
    {
        /* If the signature is not correct, this is neither a MBR, nor a VBR */
        FSerrno = CE_BAD_PARTITION;
        return EOF;
    }

    switch (mode)
    {
        // True: Rewrite the whole boot sector
        case 1:
            secCount = masterBootRecord->Partition0.PTE_NumSect;

            if (secCount < 0x1039)
            {
                disk->type = FAT12;
                // Format to FAT12 only if there are too few sectors to format
                // as FAT16
                masterBootRecord->Partition0.PTE_FSDesc = 0x01;
                if (MDD_SectorWrite (0x00, gDataBuffer, TRUE) == FALSE)
                {
                    FSerrno = CE_WRITE_ERROR;
                    return EOF;
                }

                if (secCount >= 0x1028)
                {
                    // More than 0x18 sectors for FATs, 0x20 for root dir,
                    // 0x8 reserved, and 0xFED for data
                    // So double the number of sectors in a cluster to reduce
                    // the number of data clusters used
                    disk->SecPerClus = 2;
                }
                else
                {
                    // One sector per cluster
                    disk->SecPerClus = 1;
                }

                // Prepare a boot sector
                memset (gDataBuffer, 0x00, MEDIA_SECTOR_SIZE);

                // Last digit of file system name (FAT12   )
                gDataBuffer[58] = '2';

                // Calculate the size of the FAT
                fatsize = (secCount - 0x21  + (2*disk->SecPerClus));
                test =   (341 * disk->SecPerClus) + 2;
                fatsize = (fatsize + (test-1)) / test;
    
                disk->fatcopy = 0x02;
                disk->maxroot = 0x200;
    
                disk->fatsize = fatsize;

            }
            else if (secCount <= 0x3FFD5F)
            {
                disk->type = FAT16;
                // Format to FAT16
                masterBootRecord->Partition0.PTE_FSDesc = 0x06;
                if (MDD_SectorWrite (0x00, gDataBuffer, TRUE) == FALSE)
                {
                    FSerrno = CE_WRITE_ERROR;
                    return EOF;
                }

                DataClusters = secCount - 0x218;
                // Figure out how many sectors per cluster we need
                disk->SecPerClus = 1;
                while (DataClusters > 0xFFED)
                {
                    disk->SecPerClus *= 2;
                    DataClusters /= 2;
                }
                // This shouldnt happen
                if (disk->SecPerClus > 128)
                {
                    FSerrno = CE_BAD_PARTITION;
                    return EOF;
                }

                // Prepare a boot sector
                memset (gDataBuffer, 0x00, MEDIA_SECTOR_SIZE);

                // Last digit of file system name (FAT16   )
                gDataBuffer[58] = '6';

                // Calculate the size of the FAT
                fatsize = (secCount - 0x21  + (2*disk->SecPerClus));
                test =    (256  * disk->SecPerClus) + 2;
                fatsize = (fatsize + (test-1)) / test;
    
                disk->fatcopy = 0x02;
                disk->maxroot = 0x200;
    
                disk->fatsize = fatsize;
            }
            else
            {
                disk->type = FAT32;
                // Format to FAT32
                masterBootRecord->Partition0.PTE_FSDesc = 0x0B;
                if (MDD_SectorWrite (0x00, gDataBuffer, TRUE) == FALSE)
                {
                    FSerrno = CE_WRITE_ERROR;
                    return EOF;
                }

                #ifdef FORMAT_SECTORS_PER_CLUSTER
                    disk->SecPerClus = FORMAT_SECTORS_PER_CLUSTER;
                    DataClusters = secCount / disk->SecPerClus;

                    /* FAT32: 65526 < Number of clusters < 4177918 */
                    if ((DataClusters <= 65526) || (DataClusters >= 4177918))
                    {
                        FSerrno = CE_BAD_PARTITION;
                        return EOF;
                    }
                #else               
                    /*  FAT32: 65526 < Number of clusters < 4177918 */
                    DataClusters = secCount;
                    // Figure out how many sectors per cluster we need
                    disk->SecPerClus = 1;
                    while (DataClusters > 0x3FBFFE)
                    {
                        disk->SecPerClus *= 2;
                        DataClusters /= 2;
                    }
                #endif
                // Check the cluster size: FAT32 supports 512, 1024, 2048, 4096, 8192, 16K, 32K, 64K
                if (disk->SecPerClus > 128)
                {
                    FSerrno = CE_BAD_PARTITION;
                    return EOF;
                }

                // Prepare a boot sector
                memset (gDataBuffer, 0x00, MEDIA_SECTOR_SIZE);

               // Calculate the size of the FAT
                fatsize = (secCount - 0x20);
                test =    (128  * disk->SecPerClus) + 1;
                fatsize = (fatsize + (test-1)) / test;
    
                disk->fatcopy = 0x02;
                disk->maxroot = 0x200;
    
                disk->fatsize = fatsize;
            }

            // Non-file system specific values
            gDataBuffer[0] = 0xEB;         //Jump instruction
            gDataBuffer[1] = 0x3C;
            gDataBuffer[2] = 0x90;
            gDataBuffer[3] =  'M';         //OEM Name "MCHP FAT"
            gDataBuffer[4] =  'C';
            gDataBuffer[5] =  'H';
            gDataBuffer[6] =  'P';
            gDataBuffer[7] =  ' ';
            gDataBuffer[8] =  'F';
            gDataBuffer[9] =  'A';
            gDataBuffer[10] = 'T';

            gDataBuffer[11] = 0x00;             //Sector size 
            gDataBuffer[12] = 0x02;

            gDataBuffer[13] = disk->SecPerClus;   //Sectors per cluster

            if (disk->type == FAT12 || disk->type == FAT16)
            {
                gDataBuffer[14] = 0x08;         //Reserved sector count
                gDataBuffer[15] = 0x00;
                disk->fat = 0x08 + disk->firsts;

                gDataBuffer[16] = 0x02;         //number of FATs

                gDataBuffer[17] = 0x00;          //Max number of root directory entries - 512 files allowed
                gDataBuffer[18] = 0x02;

                gDataBuffer[19] = 0x00;         //total sectors
                gDataBuffer[20] = 0x00;

                gDataBuffer[21] = 0xF8;         //Media Descriptor

                gDataBuffer[22] = fatsize & 0xFF;         //Sectors per FAT
                gDataBuffer[23] = (fatsize >> 8) & 0xFF;

                gDataBuffer[24] = 0x3F;           //Sectors per track
                gDataBuffer[25] = 0x00;
    
                gDataBuffer[26] = 0xFF;         //Number of heads
                gDataBuffer[27] = 0x00;
    
                // Hidden sectors = sectors between the MBR and the boot sector
                gDataBuffer[28] = (BYTE)(disk->firsts & 0xFF);
                gDataBuffer[29] = (BYTE)((disk->firsts / 0x100) & 0xFF);
                gDataBuffer[30] = (BYTE)((disk->firsts / 0x10000) & 0xFF);
                gDataBuffer[31] = (BYTE)((disk->firsts / 0x1000000) & 0xFF);
    
                // Total Sectors = same as sectors in the partition from MBR
                gDataBuffer[32] = (BYTE)(secCount & 0xFF);
                gDataBuffer[33] = (BYTE)((secCount / 0x100) & 0xFF);
                gDataBuffer[34] = (BYTE)((secCount / 0x10000) & 0xFF);
                gDataBuffer[35] = (BYTE)((secCount / 0x1000000) & 0xFF);

                gDataBuffer[36] = 0x00;         // Physical drive number

                gDataBuffer[37] = 0x00;         // Reserved (current head)

                gDataBuffer[38] = 0x29;         // Signature code

                gDataBuffer[39] = (BYTE)(serialNumber & 0xFF);
                gDataBuffer[40] = (BYTE)((serialNumber / 0x100) & 0xFF);
                gDataBuffer[41] = (BYTE)((serialNumber / 0x10000) & 0xFF);
                gDataBuffer[42] = (BYTE)((serialNumber / 0x1000000) & 0xFF);

                // Volume ID
                if (volumeID != NULL)
                {
                    for (Index = 0; (*(volumeID + Index) != 0) && (Index < 11); Index++)
                    {
                        gDataBuffer[Index + 43] = *(volumeID + Index);
                    }
                    while (Index < 11)
                    {
                        gDataBuffer[43 + Index++] = 0x20;
                    }
                }
                else
                {
                    for (Index = 0; Index < 11; Index++)
                    {
                        gDataBuffer[Index+43] = 0;
                    }
                }

                gDataBuffer[54] = 'F';
                gDataBuffer[55] = 'A';
                gDataBuffer[56] = 'T';
                gDataBuffer[57] = '1';
                gDataBuffer[59] = ' ';
                gDataBuffer[60] = ' ';
                gDataBuffer[61] = ' ';

            }
            else //FAT32
            {
                gDataBuffer[14] = 0x20;         //Reserved sector count
                gDataBuffer[15] = 0x00;
                disk->fat = 0x20 + disk->firsts;

                gDataBuffer[16] = 0x02;         //number of FATs

                gDataBuffer[17] = 0x00;          //Max number of root directory entries - 512 files allowed
                gDataBuffer[18] = 0x00;

                gDataBuffer[19] = 0x00;         //total sectors
                gDataBuffer[20] = 0x00;

                gDataBuffer[21] = 0xF8;         //Media Descriptor

                gDataBuffer[22] = 0x00;         //Sectors per FAT
                gDataBuffer[23] = 0x00;

                gDataBuffer[24] = 0x3F;         //Sectors per track
                gDataBuffer[25] = 0x00;
    
                gDataBuffer[26] = 0xFF;         //Number of heads
                gDataBuffer[27] = 0x00;
    
                // Hidden sectors = sectors between the MBR and the boot sector
                gDataBuffer[28] = (BYTE)(disk->firsts & 0xFF);
                gDataBuffer[29] = (BYTE)((disk->firsts / 0x100) & 0xFF);
                gDataBuffer[30] = (BYTE)((disk->firsts / 0x10000) & 0xFF);
                gDataBuffer[31] = (BYTE)((disk->firsts / 0x1000000) & 0xFF);
    
                // Total Sectors = same as sectors in the partition from MBR
                gDataBuffer[32] = (BYTE)(secCount & 0xFF);
                gDataBuffer[33] = (BYTE)((secCount / 0x100) & 0xFF);
                gDataBuffer[34] = (BYTE)((secCount / 0x10000) & 0xFF);
                gDataBuffer[35] = (BYTE)((secCount / 0x1000000) & 0xFF);

                gDataBuffer[36] = fatsize & 0xFF;         //Sectors per FAT
                gDataBuffer[37] = (fatsize >>  8) & 0xFF;
                gDataBuffer[38] = (fatsize >> 16) & 0xFF;         
                gDataBuffer[39] = (fatsize >> 24) & 0xFF;

                gDataBuffer[40] = 0x00;         //Active FAT
                gDataBuffer[41] = 0x00;

                gDataBuffer[42] = 0x00;         //File System version  
                gDataBuffer[43] = 0x00;

                gDataBuffer[44] = 0x02;         //First cluster of the root directory
                gDataBuffer[45] = 0x00;
                gDataBuffer[46] = 0x00;
                gDataBuffer[47] = 0x00;

                gDataBuffer[48] = 0x01;         //FSInfo
                gDataBuffer[49] = 0x00;

                gDataBuffer[50] = 0x00;         //Backup Boot Sector
                gDataBuffer[51] = 0x00;

                gDataBuffer[52] = 0x00;         //Reserved for future expansion
                gDataBuffer[53] = 0x00;
                gDataBuffer[54] = 0x00;                   
                gDataBuffer[55] = 0x00;
                gDataBuffer[56] = 0x00;                   
                gDataBuffer[57] = 0x00;
                gDataBuffer[58] = 0x00;                   
                gDataBuffer[59] = 0x00;
                gDataBuffer[60] = 0x00;                   
                gDataBuffer[61] = 0x00;
                gDataBuffer[62] = 0x00;                   
                gDataBuffer[63] = 0x00;

                gDataBuffer[64] = 0x00;         // Physical drive number

                gDataBuffer[65] = 0x00;         // Reserved (current head)

                gDataBuffer[66] = 0x29;         // Signature code

                gDataBuffer[67] = (BYTE)(serialNumber & 0xFF);
                gDataBuffer[68] = (BYTE)((serialNumber / 0x100) & 0xFF);
                gDataBuffer[69] = (BYTE)((serialNumber / 0x10000) & 0xFF);
                gDataBuffer[70] = (BYTE)((serialNumber / 0x1000000) & 0xFF);

                // Volume ID
                if (volumeID != NULL)
                {
                    for (Index = 0; (*(volumeID + Index) != 0) && (Index < 11); Index++)
                    {
                        gDataBuffer[Index + 71] = *(volumeID + Index);
                    }
                    while (Index < 11)
                    {
                        gDataBuffer[71 + Index++] = 0x20;
                    }
                }
                else
                {
                    for (Index = 0; Index < 11; Index++)
                    {
                        gDataBuffer[Index+71] = 0;
                    }
                }

                gDataBuffer[82] = 'F';
                gDataBuffer[83] = 'A';
                gDataBuffer[84] = 'T';
                gDataBuffer[85] = '3';
                gDataBuffer[86] = '2';
                gDataBuffer[87] = ' ';
                gDataBuffer[88] = ' ';
                gDataBuffer[89] = ' ';


            }

#ifdef __18CXX
            // C18 can't reference a value greater than 256
            // using an array name pointer
            *(dataBufferPointer + 510) = 0x55;
            *(dataBufferPointer + 511) = 0xAA;
#else
            gDataBuffer[510] = 0x55;
            gDataBuffer[511] = 0xAA;
#endif

            disk->root = disk->fat + (disk->fatcopy * disk->fatsize);

            if (MDD_SectorWrite (disk->firsts, gDataBuffer, FALSE) == FALSE)
            {
                FSerrno = CE_WRITE_ERROR;
                return EOF;
            }

            break;
        case 0:
            if (LoadBootSector (disk) != CE_GOOD)
            {
                FSerrno = CE_BADCACHEREAD;
                return EOF;
            }
            break;
        default:
            FSerrno = CE_INVALID_ARGUMENT;
            return EOF;
    }

    // Erase the FAT
    memset (gDataBuffer, 0x00, MEDIA_SECTOR_SIZE);

    if (disk->type == FAT32)
    {
        gDataBuffer[0] = 0xF8;          //BPB_Media byte value in its low 8 bits, and all other bits are set to 1
        gDataBuffer[1] = 0xFF;
        gDataBuffer[2] = 0xFF;
        gDataBuffer[3] = 0xFF;

        gDataBuffer[4] = 0x00;          //Disk is clean and no read/write errors were encountered
        gDataBuffer[5] = 0x00;
        gDataBuffer[6] = 0x00;
        gDataBuffer[7] = 0x0C;

        gDataBuffer[8]  = 0xFF;         //Root Directory EOF  
        gDataBuffer[9]  = 0xFF;
        gDataBuffer[10] = 0xFF;
        gDataBuffer[11] = 0xFF;

        for (j = disk->fatcopy - 1; j != 0xFFFF; j--)
        {
            if (MDD_SectorWrite (disk->fat + (j * disk->fatsize), gDataBuffer, FALSE) == FALSE)
                return EOF;
        }
    
        memset (gDataBuffer, 0x00, 12);
    
        for (Index = disk->fat + 1; Index < (disk->fat + disk->fatsize); Index++)
        {
            for (j = disk->fatcopy - 1; j != 0xFFFF; j--)
            {
                if (MDD_SectorWrite (Index + (j * disk->fatsize), gDataBuffer, FALSE) == FALSE)
                    return EOF;
            }
        }
    
        // Erase the root directory
        for (Index = 1; Index < disk->SecPerClus; Index++)
        {
            if (MDD_SectorWrite (disk->root + Index, gDataBuffer, FALSE) == FALSE)
                return EOF;
        }
    
        if (volumeID != NULL)
        {
            // Create a drive name entry in the root dir
            Index = 0;
            while ((*(volumeID + Index) != 0) && (Index < 11))
            {
                gDataBuffer[Index] = *(volumeID + Index);
                Index++;
            }
            while (Index < 11)
            {
                gDataBuffer[Index++] = ' ';
            }
            gDataBuffer[11] = 0x08;
            gDataBuffer[17] = 0x11;
            gDataBuffer[19] = 0x11;
            gDataBuffer[23] = 0x11;
    
            if (MDD_SectorWrite (disk->root, gDataBuffer, FALSE) == FALSE)
                return EOF;
        }
        else
        {
            if (MDD_SectorWrite (disk->root, gDataBuffer, FALSE) == FALSE)
                return EOF;
        }
    
        return 0;
    }
    else
    {
        gDataBuffer[0] = 0xF8;
        gDataBuffer[1] = 0xFF;
        gDataBuffer[2] = 0xFF;
        if (disk->type == FAT16)
            gDataBuffer[3] = 0xFF;
    
        for (j = disk->fatcopy - 1; j != 0xFFFF; j--)
        {
            if (MDD_SectorWrite (disk->fat + (j * disk->fatsize), gDataBuffer, FALSE) == FALSE)
                return EOF;
        }
    
        memset (gDataBuffer, 0x00, 4);
    
        for (Index = disk->fat + 1; Index < (disk->fat + disk->fatsize); Index++)
        {
            for (j = disk->fatcopy - 1; j != 0xFFFF; j--)
            {
                if (MDD_SectorWrite (Index + (j * disk->fatsize), gDataBuffer, FALSE) == FALSE)
                    return EOF;
            }
        }
    
        // Erase the root directory
        RootDirSectors = ((disk->maxroot * 32) + (disk->sectorSize - 1)) / disk->sectorSize;
    
        for (Index = 1; Index < RootDirSectors; Index++)
        {
            if (MDD_SectorWrite (disk->root + Index, gDataBuffer, FALSE) == FALSE)
                return EOF;
        }
    
        if (volumeID != NULL)
        {
            // Create a drive name entry in the root dir
            Index = 0;
            while ((*(volumeID + Index) != 0) && (Index < 11))
            {
                gDataBuffer[Index] = *(volumeID + Index);
                Index++;
            }
            while (Index < 11)
            {
                gDataBuffer[Index++] = ' ';
            }
            gDataBuffer[11] = 0x08;
            gDataBuffer[17] = 0x11;
            gDataBuffer[19] = 0x11;
            gDataBuffer[23] = 0x11;
    
            if (MDD_SectorWrite (disk->root, gDataBuffer, FALSE) == FALSE)
                return EOF;
        }
        else
        {
            if (MDD_SectorWrite (disk->root, gDataBuffer, FALSE) == FALSE)
                return EOF;
        }
    
        return 0;
    }
}
#endif
#endif


/*******************************************************
  Function:
    BYTE Write_File_Entry( FILEOBJ fo, WORD * curEntry)
  Summary:
    Write dir entry info into a specified entry
  Conditions:
    This function should not be called by the user.
  Input:
    fo -        \File structure
    curEntry -  Write destination
  Return Values:
    TRUE - Operation successful
    FALSE - Operation failed
  Side Effects:
    None
  Description:
    This function will calculate the sector of the
    directory (whose base sector is pointed to by the
    dirccls value in the FSFILE object 'fo') that contains
    a directory entry whose offset is indicated by the
    curEntry parameter.  It will then write the data
    in the global data buffer (which should already
    contain the entries for that sector) to the device.
  Remarks:
    None
  *******************************************************/

#ifdef ALLOW_WRITES
BYTE Write_File_Entry( FILEOBJ fo, WORD * curEntry)
{
    DISK   *dsk;
    BYTE   status;
    BYTE   offset2;
    DWORD   sector;
    DWORD   ccls;

    dsk = fo->dsk;

    // get the cluster of this entry
    ccls = fo->dirccls;

     // figure out the offset from the base sector
    offset2  = (*curEntry / (dsk->sectorSize/32));

    /* Settings based on FAT type */
    switch (dsk->type)
    {
#ifdef SUPPORT_FAT32 // If FAT32 supported.
        case FAT32:
            // Root is always cluster-based in FAT32
            offset2 = offset2 % (dsk->SecPerClus);
            break;
#endif
        case FAT12:
        case FAT16:
            if(ccls != FatRootDirClusterValue)
                offset2 = offset2 % (dsk->SecPerClus);
            break;
    }

    sector = Cluster2Sector(dsk,ccls);

    // Now write it
    // "Offset" ensures writing of data belonging to a file entry only. Hence it doesn't change other file entries.
    if ( !MDD_SectorWrite( sector + offset2, dsk->buffer, FALSE))
        status = FALSE;
    else
        status = TRUE;

    return(status);
} // Write_File_Entry
#endif


/**********************************************************
  Function:
    BYTE FAT_erase_cluster_chain (WORD cluster, DISK * dsk)
  Summary:
    Erase a chain of clusters
  Conditions:
    This function should not be called by the user.
  Input:
    cluster -  The cluster number
    dsk -      The disk structure
  Return Values:
    TRUE -  Operation successful
    FALSE - Operation failed
  Side Effects:
    None
  Description:
    This function will parse through a cluster chain
    starting with the cluster pointed to by 'cluster' and
    mark all of the FAT entries as empty until the end of
    the chain has been reached or an error occurs.
  Remarks:
    None
  **********************************************************/

#ifdef ALLOW_WRITES
BYTE FAT_erase_cluster_chain (DWORD cluster, DISK * dsk)
{
    DWORD     c,c2,ClusterFailValue;
    enum    _status {Good, Fail, Exit}status;

    status = Good;

    /* Settings based on FAT type */
    switch (dsk->type)
    {

#ifdef SUPPORT_FAT32 // If FAT32 supported.
        case FAT32:
            ClusterFailValue = CLUSTER_FAIL_FAT32;
            c2 =  LAST_CLUSTER_FAT32;
            break;
#endif
        case FAT12:
            ClusterFailValue = CLUSTER_FAIL_FAT16; // FAT16 value itself
            c2 =  LAST_CLUSTER_FAT12;
            break;
        case FAT16:
        default:
            ClusterFailValue = CLUSTER_FAIL_FAT16;
            c2 =  LAST_CLUSTER_FAT16;
            break;
    }

    // Make sure there is actually a cluster assigned
    if(cluster == 0 || cluster == 1)  // Cluster assigned can't be "0" and "1"
    {
        status = Exit;
    }
    else
    {
        while(status == Good)
        {
            // Get the FAT entry
            if((c = ReadFAT( dsk, cluster)) == ClusterFailValue)
                status = Fail;
            else
            {
                if(c == 0 || c == 1)  // Cluster assigned can't be "0" and "1"
                {
                    status = Exit;
                }
                else
                {
                    // compare against max value of a cluster in FATxx
                    // look for the last cluster in the chain
                    if ( c >= c2)
                        status = Exit;

                    // Now erase this FAT entry
                    if(WriteFAT(dsk, cluster, CLUSTER_EMPTY, FALSE) == ClusterFailValue)
                        status = Fail;

                    // now update what the current cluster is
                    cluster = c;
                }
            }
        }// while status
    }// cluster == 0

    WriteFAT (dsk, 0, 0, TRUE);

    if(status == Exit)
        return(TRUE);
    else
        return(FALSE);
} // Erase cluster
#endif

/**************************************************************************
  Function:
    DIRENTRY Cache_File_Entry( FILEOBJ fo, WORD * curEntry, BYTE ForceRead)
  Summary:
    Load a file entry
  Conditions:
    This function should not be called by the user.
  Input:
    fo -         File information
    curEntry -   Offset of the directory entry to load.
    ForceRead -  Forces loading of a new sector of the directory.
  Return:
    DIRENTRY - Pointer to the directory entry that was loaded.
  Side Effects:
    Any unwritten data in the data buffer will be written to the device.
  Description:
    Load the sector containing the file entry pointed to by 'curEntry'
    from the directory pointed to by the variables in 'fo.'
  Remarks:
    Any modification of this function is extremely likely to
    break something.
  **************************************************************************/

DIRENTRY Cache_File_Entry( FILEOBJ fo, WORD * curEntry, BYTE ForceRead)
{
    DIRENTRY dir;
    DISK *dsk;
    DWORD sector;
    DWORD cluster, LastClusterLimit;
    DWORD ccls;
    BYTE offset2;
    BYTE numofclus;

    dsk = fo->dsk;

    // get the base sector of this directory
    cluster = fo->dirclus;
    ccls = fo->dirccls;

     // figure out the offset from the base sector
    offset2  = (*curEntry / (dsk->sectorSize/32));

    offset2 = offset2; // emulator issue

    /* Settings based on FAT type */
    switch (dsk->type)
    {
#ifdef SUPPORT_FAT32 // If FAT32 supported.
        case FAT32:
            // the ROOT is always cluster based in FAT32
            /* In FAT32: There is no ROOT region. Root etries are made in DATA region only.
            Every cluster of DATA which is accupied by ROOT is tracked by FAT table/entry so the ROOT can grow
            to an amount which is restricted only by available free DATA region. */
            offset2  = offset2 % (dsk->SecPerClus);   // figure out the offset
            LastClusterLimit = LAST_CLUSTER_FAT32;
            break;
#endif
        case FAT12:
        case FAT16:
        default:
            // if its the root its not cluster based
            if(cluster != 0)
                offset2  = offset2 % (dsk->SecPerClus);   // figure out the offset
            LastClusterLimit = LAST_CLUSTER_FAT16;
            break;
    }

    // check if a new sector of the root must be loaded
    if (ForceRead || (*curEntry & MASK_MAX_FILE_ENTRY_LIMIT_BITS) == 0)     // only 16 entries per sector
    {
        // see if we have to load a new cluster
        if((offset2 == 0 && (*curEntry) >= DIRENTRIES_PER_SECTOR) || ForceRead)
        {
            if(cluster == 0)
            {
                ccls = 0;
            }
            else
            {
                // If ForceRead, read the number of sectors from 0
                if(ForceRead)
                    numofclus = ((WORD)(*curEntry) / (WORD)(((WORD)DIRENTRIES_PER_SECTOR) * (WORD)dsk->SecPerClus));
                // Otherwise just read the next sector
                else
                    numofclus = 1;

                // move to the correct cluster
                while(numofclus)
                {
                    ccls = ReadFAT(dsk, ccls);

                    if(ccls >= LastClusterLimit)
                        break;
                    else
                        numofclus--;
                }
            }
        }

        // see if that we have a valid cluster number
        if(ccls < LastClusterLimit)
        {
            fo->dirccls = ccls; // write it back

            sector = Cluster2Sector(dsk,ccls);

            /* see if we are root and about to go pass our boundaries
            FAT32 stores the root directory in the Data Region along with files and other directories,
            allowing it to grow without such a restraint */
            if((ccls == FatRootDirClusterValue) && ((sector + offset2) >= dsk->data) && (FAT32 != dsk->type))
            {
                dir = ((DIRENTRY)NULL);   // reached the end of the root
            }
            else
            {
#ifdef ALLOW_WRITES
                if (gNeedDataWrite)
                    if (flushData())
                        return NULL;
#endif
                gBufferOwner = NULL;
                gBufferZeroed = FALSE;

                if ( MDD_SectorRead( sector + offset2, dsk->buffer) != TRUE) // if FALSE: sector could not be read.
                {
                    dir = ((DIRENTRY)NULL);
                }
                else // Sector has been read properly, Copy the root entry info of the file searched.
                {
                    if(ForceRead)    // Buffer holds all 16 root entry info. Point to the one required.
                        dir = (DIRENTRY)((DIRENTRY)dsk->buffer) + ((*curEntry)%DIRENTRIES_PER_SECTOR);
                    else
                        dir = (DIRENTRY)dsk->buffer;
                }
                gLastDataSectorRead = 0xFFFFFFFF;
            }
        }
        else
        {
            nextClusterIsLast = TRUE;
            dir = ((DIRENTRY)NULL);
        }
    }
    else
        dir = (DIRENTRY)((DIRENTRY)dsk->buffer) + ((*curEntry)%DIRENTRIES_PER_SECTOR);

    return(dir);
} // Cache_File_Entry


/*************************************************************************
  Function:
    CETYPE CreateFileEntry(FILEOBJ fo, WORD *fHandle)
  Summary:
    Create a new file entry
  Conditions:
    Should not be called by the user.
  Input:
    fo -       Pointer to file structure
    fHandle -  Location to create file
  Return Values:
    CE_GOOD -        File Creation successful
    CE_DIR_FULL -    All root directory entries are taken
    CE_WRITE_ERROR - The head cluster of the file could not be created.
  Side Effects:
    Modifies the FSerrno variable.
  Description:
    With the data passed within fo, create a new file entry in the current
    directory.  This function will first search for empty file entries.
    Once an empty entry is found, the entry will be populated with data
    for a file or directory entry.  Finally, the first cluster of the
    new file will be located and allocated, and its value will be
    written into the file entry.
  Remarks:
    None
  *************************************************************************/

#ifdef ALLOW_WRITES
CETYPE CreateFileEntry(FILEOBJ fo, WORD *fHandle, BYTE mode)
{
    BYTE    index;
    CETYPE  error = CE_GOOD;
    char    name[11];

    FSerrno = CE_GOOD;

    for (index = 0; index < FILE_NAME_SIZE; index ++)
    {
        name[index] = fo->name[index];
    }

    *fHandle = 0;

    // figure out where to put this file in the directory stucture
    if(FindEmptyEntries(fo, fHandle))
    {
        // found the entry, now populate it
        if((error = PopulateEntries(fo, name ,fHandle, mode)) == CE_GOOD)
        {
            // if everything is ok, create a first cluster
            error = CreateFirstCluster(fo);
        }
    }
    else
    {
        error = CE_DIR_FULL;
    }

    FSerrno = error;

    return(error);
}
#endif

/******************************************************
  Function:
    CETYPE CreateFirstCluster(FILEOBJ fo)
  Summary:
    Create the first cluster for a file
  Conditions:
    This function should not be called by the user.
  Input:
    fo -  The file that contains the first cluster
  Return Values:
    CE_GOOD -        First cluster created successfully
    CE_WRITE_ERROR - Cluster creation failed
  Side Effects:
    None
  Description:
    This function will find an unused cluster, link it to
    a file's directory entry, and write the entry back
    to the device.
  Remarks:
    None.
  ******************************************************/

#ifdef ALLOW_WRITES
CETYPE CreateFirstCluster(FILEOBJ fo)
{
    CETYPE       error;
    DWORD      cluster,TempMsbCluster;
    WORD        fHandle;
    DIRENTRY   dir;
    fHandle = fo->entry;

    // Now create the first cluster (head cluster)
    if((error = FILECreateHeadCluster(fo,&cluster)) == CE_GOOD)
    {
        // load the file entry so the new cluster can be linked to it
        dir = LoadDirAttrib(fo, &fHandle);

        // Now update the new cluster
        dir->DIR_FstClusLO = (cluster & 0x0000FFFF);


#ifdef SUPPORT_FAT32 // If FAT32 supported.
        // Get the higher part of cluster and store it in directory entry.
       TempMsbCluster = (cluster & 0x0FFF0000);    // Since only 28 bits usedin FAT32. Mask the higher MSB nibble.
       TempMsbCluster = TempMsbCluster >> 16;      // Get the date into Lsb place.
       dir->DIR_FstClusHI = TempMsbCluster;
#else // If FAT32 support not enabled
       TempMsbCluster = 0;                         // Just to avoid compiler warnigng.
       dir->DIR_FstClusHI = 0;
#endif

        // now write it
        if(Write_File_Entry(fo, &fHandle) != TRUE)
            error = CE_WRITE_ERROR;
    } // Create Cluster

    return(error);
}// End of CreateFirstCluster
#endif

/**********************************************************
  Function:
    BYTE FindEmptyEntries(FILEOBJ fo, WORD *fHandle)
  Summary:
    Find an empty dir entry
  Conditions:
    This function should not be called by the user.
  Input:
    fo -       Pointer to file structure
    fHandle -  Start of entries
  Return Values:
    TRUE - One found
    FALSE - None found
  Side Effects:
    None
  Description:
    This function will cache directory entries, starting
    with the one pointed to by the fHandle argument.  It will
    then search through the entries until an unused one
    is found.  If the end of the cluster chain for the
    directory is reached, a new cluster will be allocated
    to the directory (unless it's a FAT12 or FAT16 root)
    and the first entry of the new cluster will be used.
  Remarks:
    None.
  **********************************************************/

#ifdef ALLOW_WRITES
BYTE FindEmptyEntries(FILEOBJ fo, WORD *fHandle)
{
    BYTE   status = NOT_FOUND;
    BYTE   amountfound;
    BYTE   a;
    WORD   bHandle;
    DWORD b;
    DIRENTRY    dir;

    fo->dirccls = fo->dirclus;
    if((dir = Cache_File_Entry( fo, fHandle, TRUE)) == NULL)
    {
        status = CE_BADCACHEREAD;
    }
    else
    {
        // while its still not found
        while(status == NOT_FOUND)
        {
            amountfound = 0;
            bHandle = *fHandle;

            // find (number) continuous entries
            do
            {
                // Get the entry
                dir = Cache_File_Entry( fo, fHandle, FALSE);

                // Read the first char of the file name
                if(dir != NULL) // Last entry of the cluster
                {
                    a = dir->DIR_Name[0];
                }
                // increase number
                (*fHandle)++;
            }while((a == DIR_DEL || a == DIR_EMPTY) && (dir != (DIRENTRY)NULL) &&  (++amountfound < 1));

            // --- now why did we exit?
            if(dir == NULL) // Last entry of the cluster
            {
                //setup the current cluster
                b = fo->dirccls; // write it back

                // make sure we are not the root directory
                if(b == FatRootDirClusterValue)
                {
                    if (fo->dsk->type != FAT32)
                        status = NO_MORE;
                    else
                    {
                        fo->ccls = b;

                        if(FILEallocate_new_cluster(fo, 1) == CE_DISK_FULL)
                            status = NO_MORE;
                        else
                        {
                            *fHandle = bHandle;
                            status = FOUND;     // a new cluster will surely hold a new file name
                        }
                    }
                }
                else
                {
                    fo->ccls = b;

                    if(FILEallocate_new_cluster(fo, 1) == CE_DISK_FULL)
                        status = NO_MORE;
                    else
                    {
                        *fHandle = bHandle;
                        status = FOUND;     // a new cluster will surely hold a new file name
                    }
                }
            }
            else
            {
                if(amountfound == 1)
                {
                    status = FOUND;
                    *fHandle = bHandle;
                }
            }
        }// while

        // copy the base handle over
        *fHandle = bHandle;
    }

    if(status == FOUND)
        return(TRUE);
    else
        return(FALSE);
}
#endif

/**************************************************************************
  Function:
    BYTE PopulateEntries(FILEOBJ fo, char *name , WORD *fHandle)
  Summary:
    Populate a dir entry with data
  Conditions:
    Should not be called by the user.
  Input:
    fo -      Pointer to file structure
    name -    Name of the file
    fHandle - Location of the file
  Return Values:
    CE_GOOD - Population successful
  Side Effects:
    None
  Description:
    This function will write data into a new file entry.  It will also
    load timestamp data (based on the method selected by the user) and
    update the timestamp variables.
  Remarks:
    None.
  **************************************************************************/

#ifdef ALLOW_WRITES
BYTE PopulateEntries(FILEOBJ fo, char *name , WORD *fHandle, BYTE mode)
{
    BYTE error = CE_GOOD;
    DIRENTRY    dir;

    fo->dirccls = fo->dirclus;
    dir = Cache_File_Entry( fo, fHandle, TRUE);

    if (dir == NULL)
        return CE_BADCACHEREAD;

    // copy the contents over
    strncpy(dir->DIR_Name,name,DIR_NAMECOMP);

    // setup no attributes
    if (mode == DIRECTORY)
        dir->DIR_Attr = ATTR_DIRECTORY;
    else
        dir->DIR_Attr   = ATTR_ARCHIVE;

    dir->DIR_NTRes  = 0x00;              // nt reserved
    dir->DIR_FstClusHI =    0x0000;      // high word of this enty's first cluster number
    dir->DIR_FstClusLO =    0x0000;      // low word of this entry's first cluster number
    dir->DIR_FileSize =     0x0;         // file size in DWORD

   // Timing information for uncontrolled clock mode
#ifdef INCREMENTTIMESTAMP
    dir->DIR_CrtTimeTenth = 0xB2;        // millisecond stamp
    dir->DIR_CrtTime =      0x7278;      // time created
    dir->DIR_CrtDate =      0x32B0;      // date created
    dir->DIR_LstAccDate =   0x32B0;      // Last Access date
    dir->DIR_WrtTime =      0x7279;      // last update time
    dir->DIR_WrtDate =      0x32B0;      // last update date
#endif

#ifdef USEREALTIMECLOCK
    CacheTime();
    dir->DIR_CrtTimeTenth = gTimeCrtMS;        // millisecond stamp
    dir->DIR_CrtTime =      gTimeCrtTime;      // time created //
    dir->DIR_CrtDate =      gTimeCrtDate;      // date created (1/1/2004)
    dir->DIR_LstAccDate =   gTimeAccDate;      // Last Access date
    dir->DIR_WrtTime =      gTimeWrtTime;      // last update time
    dir->DIR_WrtDate =      gTimeWrtDate;      // last update date
#endif

#ifdef USERDEFINEDCLOCK
    // The user will have set the time before this funciton is called
    dir->DIR_CrtTimeTenth = gTimeCrtMS;
    dir->DIR_CrtTime =      gTimeCrtTime;
    dir->DIR_CrtDate =       gTimeCrtDate;
    dir->DIR_LstAccDate =   gTimeAccDate;
    dir->DIR_WrtTime =       gTimeWrtTime;
    dir->DIR_WrtDate =      gTimeWrtDate;
#endif

    fo->size        = dir->DIR_FileSize;
    fo->time        = dir->DIR_CrtTime;
    fo->date        = dir->DIR_CrtDate;
    fo->attributes  = dir->DIR_Attr;
    fo->entry       = *fHandle;

    // just write the last entry in
    if (Write_File_Entry(fo,fHandle) != TRUE)
        error = CE_WRITE_ERROR;

    return(error);
}

#ifdef USEREALTIMECLOCK

/*************************************************************************
  Function:
    void CacheTime (void)
  Summary:
    Automatically store timestamp information from the RTCC
  Conditions:
    RTCC module enabled.  Should not be called by the user.
  Return Values:
    None
  Side Effects:
    Modifies global timing variables
  Description:
    This function will automatically load information from an RTCC
    module and use it to update the global timing variables.  These can
    then be used to update file timestamps.
  Remarks:
    None.
  *************************************************************************/

void CacheTime (void)
{
    WORD    year, monthday, weekhour, minsec, c, result;
//    BYTE    ptr1, ptr0;

	unsigned int	t0, t1;
	unsigned int	d0, d1;

	do  // Get the time
	{
		t0=RTCTIME;
		t1=RTCTIME;
	}while(t0!=t1);

	do  // Get the date
	{
		d0=RTCDATE;
		d1=RTCDATE;
	}while(d0!=d1);

    // Put them in place.
    year        = (WORD)(d0 >> 24);
    monthday    = (WORD)(d0 >> 8);
    weekhour    = (WORD)((d0 & 0x0F) << 8);
    weekhour   |= (WORD)(t0 >> 24);
    minsec      = (WORD)(t0 >> 8);


    c = 0;
    c += (year & 0x0F);
    c += ((year & 0xF0) >> 4) * 10;
    // c equals the last 2 digits of the year from 2000 to 2099
    // Add 20 to adjust it to FAT time (from 1980 to 2107)
    c += 20;
    // shift the result to bits
    result = c << 9;

    if ((monthday & 0x1000) == 0x1000)
    {
        c = 10;
    }
    else
    {
        c = 0;
    }
    c += ((monthday & 0x0F00) >> 8);
    c <<= 5;
    result |= c;

    c = (monthday & 0x00F0) >> 4;
    c *= 10;
    c += (monthday & 0x000F);

    result |= c;

    gTimeCrtDate = result;
    gTimeWrtDate = result;
    gTimeAccDate = result;

    c = ((weekhour & 0x00F0) >> 4) * 10;
    c += (weekhour & 0x000F);
    result = c << 11;
    c = ((minsec & 0xF000) >> 12) * 10;
    c += (minsec & 0x0F00) >> 8;
    result |= (c << 5);
    c = ((minsec & 0x00F0) >> 4) * 10;
    c += (minsec & 0x000F);

    // If seconds mod 2 is 1, add 1000 ms
    if (c % 2)
        gTimeCrtMS = 100;
    else
        gTimeCrtMS = 0;

    c >>= 1;
    result |= c;

    gTimeCrtTime = result;
    gTimeWrtTime = result;
}
#endif

#ifdef USERDEFINEDCLOCK

/***********************************************************************************************************
  Function:
    int SetClockVars (unsigned int year, unsigned char month, unsigned char day, unsigned char hour, unsigned char minute, unsigned char second)
  Summary:
    Manually set timestamp variables
  Conditions:
    USERDEFINEDCLOCK macro defined in FSconfig.h.
  Input:
    year -     The year (1980\-2107)
    month -   The month (1\-12)
    day -     The day of the month (1\-31)
    hour -    The hour (0\-23)
    minute -  The minute (0\-59)
    second -  The second (0\-59)
  Return Values:
    None
  Side Effects:
    Modifies global timing variables
  Description:
    Lets the user manually set the timing variables.  The values passed in will be converted to the format
    used by the FAT timestamps.
  Remarks:
    Call this before creating a file or directory (set create time) and
    before closing a file (set last access time, last modified time)
  ***********************************************************************************************************/

int SetClockVars (unsigned int year, unsigned char month, unsigned char day, unsigned char hour, unsigned char minute, unsigned char second)
{
    unsigned int result;

    if ((year < 1980) || (year > 2107) || (month < 1) || (month > 12) ||
        (day < 1) || (day > 31) || (hour > 23) || (minute > 59) || (second > 59))
    {
        FSerrno = CE_INVALID_ARGUMENT;
        return -1;
    }

    result = (year - 1980) << 9;
    result |= (unsigned int)((unsigned int)month << 5);
    result |= (day);

    gTimeAccDate = result;
    gTimeCrtDate = result;
    gTimeWrtDate = result;

    result = ((unsigned int)hour << 11);
    result |= (unsigned int)((unsigned int)minute << 5);
    result |= (second/2);

    gTimeCrtTime = result;
    gTimeWrtTime = result;

    if (second % 2)
        gTimeCrtMS = 100;
    else
        gTimeCrtMS = 0;

    FSerrno = CE_GOOD;
    return 0;
}
#endif

#endif

/***********************************************************************
  Function:
    BYTE FILEallocate_new_cluster( FILEOBJ fo, BYTE mode)
  Summary;
    Allocate a new cluster to a file
  Conditions:
    Should not be called by the user.
  Input:
    fo -    Pointer to file structure
    mode -
         - 0 - Allocate a cluster to a file
         - 1 - Allocate a cluster to a directory
  Return Values:
    CE_GOOD -      Cluster allocated
    CE_DISK_FULL - No clusters available
  Side Effects:
    None
  Description:
    This function will find an empty cluster on the device using the
    FATfindEmptyCluster function.  It will then mark it as the last
    cluster in the file in the FAT chain, and link the current last
    cluster of the passed file to the new cluster.  If the new
    cluster is a directory cluster, it will be erased (so there are no
    extraneous directory entries).  If it's allocated to a non-directory
    file, it doesn't need to be erased; extraneous data in the cluster
    will be unviewable because of the file size parameter.
  Remarks:
    None.
  ***********************************************************************/

#ifdef ALLOW_WRITES
BYTE FILEallocate_new_cluster( FILEOBJ fo, BYTE mode)
{
    DISK *      dsk;
    DWORD c,curcls;

    dsk = fo->dsk;
    c = fo->ccls;

    // find the next empty cluster
    c = FATfindEmptyCluster(fo);
    if (c == 0)      // "0" is just an indication as Disk full in the fn "FATfindEmptyCluster()"
        return CE_DISK_FULL;


    // mark the cluster as taken, and last in chain
    if(dsk->type == FAT12)
        WriteFAT( dsk, c, LAST_CLUSTER_FAT12, FALSE);
    else if (dsk->type == FAT16)
        WriteFAT( dsk, c, LAST_CLUSTER_FAT16, FALSE);

#ifdef SUPPORT_FAT32 // If FAT32 supported.
    else
        WriteFAT( dsk, c, LAST_CLUSTER_FAT32, FALSE);
#endif

    // link current cluster to the new one
    curcls = fo->ccls;

    WriteFAT( dsk, curcls, c, FALSE);

    // update the FILE structure
    fo->ccls = c;

    // IF this is a dir, we need to erase the cluster
    // If it's a file, we can leave it- the file size
    // will limit the data we see to the data that's been
    // written
    if (mode == 1)
        return (EraseCluster(dsk, c));
    else
        return CE_GOOD;

} // allocate new cluster
#endif

/***********************************************
  Function:
    DWORD FATfindEmptyCluster(FILEOBJ fo)
  Summary:
    Find the next available cluster on the device
  Conditions:
    This function should not be called by the
    user.
  Input:
    fo -  Pointer to file structure
  Return Values:
    DWORD - Address of empty cluster
    0 -     Could not find empty cluster
  Side Effects:
    None
  Description:
    This function will search through the FAT to
    find the next available cluster on the device.
  Remarks:
    Should not be called by user
  ***********************************************/

#ifdef ALLOW_WRITES
DWORD FATfindEmptyCluster(FILEOBJ fo)
{
    DISK *   disk;
    DWORD    value = 0x0;
    DWORD    c,curcls, EndClusterLimit, ClusterFailValue;

    disk = fo->dsk;
    c = fo->ccls;

    /* Settings based on FAT type */
    switch (disk->type)
    {
#ifdef SUPPORT_FAT32 // If FAT32 supported.
        case FAT32:
            EndClusterLimit = END_CLUSTER_FAT32;
            ClusterFailValue = CLUSTER_FAIL_FAT32;
            break;
#endif
        case FAT12:
            EndClusterLimit = END_CLUSTER_FAT12;
            ClusterFailValue = CLUSTER_FAIL_FAT16;
            break;
        case FAT16:
        default:
            EndClusterLimit = END_CLUSTER_FAT16;
            ClusterFailValue = CLUSTER_FAIL_FAT16;
            break;
    }

    // just in case
    if(c < 2)
        c = 2;

    curcls = c;
    ReadFAT(disk, c);

    // sequentially scan through the FAT looking for an empty cluster
    while(c)
    {
        // look at its value
        if ( (value = ReadFAT(disk, c)) == ClusterFailValue)
        {
            c = 0;
            break;
        }

        // check if empty cluster found
        if (value == CLUSTER_EMPTY)
            break;

        c++;    // check next cluster in FAT
        // check if reached last cluster in FAT, re-start from top
        if (value == EndClusterLimit || c >= (disk->maxcls+2))
            c = 2;

        // check if full circle done, disk full
        if ( c == curcls)
        {
            c = 0;
            break;
        }
    }  // scanning for an empty cluster

    return(c);
}
#endif


/*********************************************************************************
  Function:
    void FSGetDiskProperties(FS_DISK_PROPERTIES* properties)
  Summary:
    Allows user to get the disk properties (size of disk, free space, etc)
  Conditions:
    1) ALLOW_GET_DISK_PROPERTIES must be defined in FSconfig.h
    2) a FS_DISK_PROPERTIES object must be created before the function is called
    3) the new_request member of the FS_DISK_PROPERTIES object must be set before
        calling the function for the first time.  This will start a new search.
    4) this function should not be called while there is a file open.  Close all
        files before calling this function.
  Input:
    properties - a pointer to a FS_DISK_PROPERTIES object where the results should
      be stored.
  Return Values:
    This function returns void.  The properties_status of the previous call of this 
      function is located in the properties.status field.  This field has the 
      following possible values:

    FS_GET_PROPERTIES_NO_ERRORS - operation completed without error.  Results
      are in the properties object passed into the function.
    FS_GET_PROPERTIES_DISK_NOT_MOUNTED - there is no mounted disk.  Results in
      properties object is not valid
    FS_GET_PROPERTIES_CLUSTER_FAILURE - there was a failure trying to read a 
      cluster from the drive.  The results in the properties object is a partial
      result up until the point of the failure.
    FS_GET_PROPERTIES_STILL_WORKING - the search for free sectors is still in
      process.  Continue calling this function with the same properties pointer 
      until either the function completes or until the partial results meets the
      application needs.  The properties object contains the partial results of
      the search and can be used by the application.  
  Side Effects:
    Can cause errors if called when files are open.  Close all files before
    calling this function.

    Calling this function without setting the new_request member on the first
    call can result in undefined behavior and results.

    Calling this function after a result is returned other than
    FS_GET_PROPERTIES_STILL_WORKING can result in undefined behavior and results.
  Description:  
    This function returns the information about the mounted drive.  The results 
    member of the properties object passed into the function is populated with 
    the information about the drive.    

    Before starting a new request, the new_request member of the properties
    input parameter should be set to TRUE.  This will initiate a new search
    request.

    This function will return before the search is complete with partial results.
    All of the results except the free_clusters will be correct after the first
    call.  The free_clusters will contain the number of free clusters found up
    until that point, thus the free_clusters result will continue to grow until
    the entire drive is searched.  If an application only needs to know that a 
    certain number of bytes is available and doesn't need to know the total free 
    size, then this function can be called until the required free size is
    verified.  To continue a search, pass a pointer to the same FS_DISK_PROPERTIES
    object that was passed in to create the search.

    A new search request sould be made once this function has returned a value 
    other than FS_GET_PROPERTIES_STILL_WORKING.  Continuing a completed search
    can result in undefined behavior or results.

    Typical Usage:
    <code>
    FS_DISK_PROPERTIES disk_properties;

    disk_properties.new_request = TRUE;

    do
    {
        FSGetDiskProperties(&disk_properties);
    } while (disk_properties.properties_status == FS_GET_PROPERTIES_STILL_WORKING);
    </code>

    results.disk_format - contains the format of the drive.  Valid results are 
      FAT12(1), FAT16(2), or FAT32(3).

    results.sector_size - the sector size of the mounted drive.  Valid values are
      512, 1024, 2048, and 4096.

    results.sectors_per_cluster - the number sectors per cluster.

    results.total_clusters - the number of total clusters on the drive.  This 
      can be used to calculate the total disk size (total_clusters * 
      sectors_per_cluster * sector_size = total size of drive in bytes)

    results.free_clusters - the number of free (unallocated) clusters on the drive.
      This can be used to calculate the total free disk size (free_clusters * 
      sectors_per_cluster * sector_size = total size of drive in bytes)

  Remarks:
    PIC24F size estimates:
      Flash - 400 bytes (-Os setting)

    PIC24F speed estimates:
      Search takes approximately 7 seconds per Gigabyte of drive space.  Speed
        will vary based on the number of sectors per cluster and the sector size.
  *********************************************************************************/
#if defined(ALLOW_GET_DISK_PROPERTIES)
void FSGetDiskProperties(FS_DISK_PROPERTIES* properties)
{
    BYTE    i;
    DWORD   value = 0x0;

    if(properties->new_request == TRUE)
    {
        properties->disk = &gDiskData;
        properties->results.free_clusters = 0;
        properties->new_request = FALSE;

        if(properties->disk->mount != TRUE)
        {
            properties->properties_status = FS_GET_PROPERTIES_DISK_NOT_MOUNTED;
            return;
        }

        properties->properties_status = FS_GET_PROPERTIES_STILL_WORKING;
   
        properties->results.disk_format = properties->disk->type;
        properties->results.sector_size = properties->disk->sectorSize;
        properties->results.sectors_per_cluster = properties->disk->SecPerClus;
        properties->results.total_clusters = properties->disk->maxcls;

        /* Settings based on FAT type */
        switch (properties->disk->type)
        {
    #ifdef SUPPORT_FAT32 // If FAT32 supported.
            case FAT32:
                properties->private.EndClusterLimit = END_CLUSTER_FAT32;
                properties->private.ClusterFailValue = CLUSTER_FAIL_FAT32;
                break;
    #endif
            case FAT16:
                properties->private.EndClusterLimit = END_CLUSTER_FAT16;
                properties->private.ClusterFailValue = CLUSTER_FAIL_FAT16;
                break;
            case FAT12:
                properties->private.EndClusterLimit = END_CLUSTER_FAT12;
                properties->private.ClusterFailValue = CLUSTER_FAIL_FAT16;
                break;
        }
    
        properties->private.c = 2;

        properties->private.curcls = properties->private.c;
        ReadFAT(properties->disk, properties->private.c);
    }

    if(properties->disk == NULL)
    {
        properties->properties_status = FS_GET_PROPERTIES_DISK_NOT_MOUNTED;
        return;
    }

    if(properties->properties_status != FS_GET_PROPERTIES_STILL_WORKING)
    {
        return;
    }

    // sequentially scan through the FAT looking for an empty cluster
    for(i=0;i<255;i++)
    {
        // look at its value
        if ( (value = ReadFAT(properties->disk, properties->private.c)) == properties->private.ClusterFailValue)
        {
            properties->properties_status = FS_GET_PROPERTIES_CLUSTER_FAILURE;
            return;
        }

        // check if empty cluster found
        if (value == CLUSTER_EMPTY)
        {
            properties->results.free_clusters++;
        }

        properties->private.c++;    // check next cluster in FAT
        // check if reached last cluster in FAT, re-start from top
        if (value == properties->private.EndClusterLimit || properties->private.c >= (properties->results.total_clusters + 2))
            properties->private.c = 2;

        // check if full circle done, disk full
        if ( properties->private.c == properties->private.curcls)
        {
            properties->properties_status = FS_GET_PROPERTIES_NO_ERRORS;
            return;
        }
    }  // scanning for an empty cluster

    properties->properties_status = FS_GET_PROPERTIES_STILL_WORKING;
    return;
}
#endif

/************************************************************
  Function:
    int FSfclose(FSFILE *fo)
  Summary:
    Update file information and free FSFILE objects
  Conditions:
    File opened
  Input:
    fo -  Pointer to the file to close
  Return Values:
    0 -   File closed successfully
    EOF - Error closing the file
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    This function will update the directory entry for the
    file pointed to by 'fo' with the information contained
    in 'fo,' including the new file size and attributes.
    Timestamp information will also be loaded based on the
    method selected by the user and written to the entry
    as the last modified time and date.  The file entry will
    then be written to the device.  Finally, the memory
    used for the specified file object will be freed from
    the dynamic heap or the array of FSFILE objects.
  Remarks:
    A function to flush data to the device without closing the
    file can be created by removing the portion of this
    function that frees the memory and the line that clears
    the write flag.
  ************************************************************/

int FSfclose(FSFILE   *fo)
{
    WORD        fHandle;
#ifndef FS_DYNAMIC_MEM
    WORD        fIndex;
#endif
    int        error = 72;
#ifdef ALLOW_WRITES
    DIRENTRY    dir;
#endif

    FSerrno = CE_GOOD;
    fHandle = fo->entry;

#ifdef ALLOW_WRITES
    if(fo->flags.write)
    {
        if (gNeedDataWrite)
            if (flushData())
            {
                FSerrno = CE_WRITE_ERROR;
                return EOF;
            }

        // Write the current FAT sector to the disk
        WriteFAT (fo->dsk, 0, 0, TRUE);

        // Get the file entry
        dir = LoadDirAttrib(fo, &fHandle);

        if (dir == NULL)
        {
            FSerrno = CE_BADCACHEREAD;
            error = EOF;
            return error;
        }

      // update the time
#ifdef INCREMENTTIMESTAMP
        IncrementTimeStamp(dir);
#elif defined USERDEFINEDCLOCK
        dir->DIR_WrtTime = gTimeWrtTime;
        dir->DIR_WrtDate = gTimeWrtDate;
#elif defined USEREALTIMECLOCK
        CacheTime();
        dir->DIR_WrtTime = gTimeWrtTime;
        dir->DIR_WrtDate = gTimeWrtDate;
#endif

        dir->DIR_FileSize = fo->size;

        dir->DIR_Attr = fo->attributes;

        // just write the last entry in
        if(Write_File_Entry(fo,&fHandle))
            error = 0;
        else
        {
            FSerrno = CE_WRITE_ERROR;
            error = EOF;
        }

        // it's now closed
        fo->flags.write = FALSE;
    }
#endif

#ifdef FS_DYNAMIC_MEM
    FS_free((unsigned char *)fo);
#else

    for( fIndex = 0; fIndex < FS_MAX_FILES_OPEN; fIndex++ )
    {
        if( fo == &gFileArray[fIndex] )
        {
            gFileSlotOpen[fIndex] = TRUE;
            break;
        }
    }
#endif

    // File opened in read mode
    if (error == 72)
        error = 0;

    return(error);
} // FSfclose




/*******************************************************
  Function:
    void IncrementTimeStamp(DIRENTRY dir)
  Summary:
    Automatically set the timestamp to "don't care" data
  Conditions:
    Should not be called by the user.
  Input:
    dir -  Pointer to directory structure
  Return Values:
    None
  Side Effects:
    None
  Description:
    This function will increment the timestamp variable in
    the 'dir' directory entry.  This is used for the
    don't-care timing method.
  Remarks:
    None
  *******************************************************/
#ifdef INCREMENTTIMESTAMP
void IncrementTimeStamp(DIRENTRY dir)
{
    BYTE          seconds;
    BYTE          minutes;
    BYTE          hours;

    BYTE          day;
    BYTE          month;
    BYTE          year;

    seconds = (dir->DIR_WrtTime & 0x1f);
    minutes = ((dir->DIR_WrtTime & 0x07E0) >> 5);
    hours   = ((dir->DIR_WrtTime & 0xF800) >> 11);

    day     = (dir->DIR_WrtDate & 0x1f);
    month   = ((dir->DIR_WrtDate & 0x01E0) >> 5);
    year    = ((dir->DIR_WrtDate & 0xFE00) >> 9);

    if(seconds < 29)
    {
        // Increment number of seconds by 2
        // This clock method isn't intended to be accurate anyway
        seconds++;
    }
    else
    {
        seconds = 0x00;

        if(minutes < 59)
        {
            minutes++;
        }
        else
        {
            minutes = 0;

            if(hours < 23)
            {
                hours++;
            }
            else
            {
                hours = 0;
                if(day < 30)
                {
                    day++;
                }
                else
                {
                    day = 1;

                    if(month < 12)
                    {
                        month++;
                    }
                    else
                    {
                        month = 1;
                        // new year
                        year++;
                        // This is only valid until 2107
                    }
                }
            }
        }
    }

    dir->DIR_WrtTime = (WORD)(seconds);
    dir->DIR_WrtTime |= ((WORD)(minutes) << 5);
    dir->DIR_WrtTime |= ((WORD)(hours) << 11);

    dir->DIR_WrtDate = (WORD)(day);
    dir->DIR_WrtDate |= ((WORD)(month) << 5);
    dir->DIR_WrtDate |= ((WORD)(year) << 9);
}
#endif

/*****************************************************************
  Function:
    BYTE Fill_File_Object(FILEOBJ fo, WORD *fHandle)
  Summary:
    Fill a file object with specified dir entry data
  Conditions:
    This function should not be called by the user.
  Input:
    fo -       Pointer to file structure
    fHandle -  Passed member's location
  Return Values:
    FOUND -     Operation successful
    NOT_FOUND - Operation failed
  Side Effects:
    None
  Description:
    This function will cache the sector of directory entries
    in the directory pointed to by the dirclus value in
    the FSFILE object 'fo' that contains the entry that
    corresponds to the fHandle offset.  It will then copy
    the file information for that entry into the 'fo' FSFILE
    object.
  Remarks:
    None.
  *****************************************************************/

BYTE Fill_File_Object(FILEOBJ fo, WORD *fHandle)
{
    DIRENTRY    dir;
    BYTE        index, a;
    BYTE        character;
    BYTE        status;
    BYTE        test = 0;

    // Get the entry
    if (((*fHandle & MASK_MAX_FILE_ENTRY_LIMIT_BITS) == 0) && (*fHandle != 0)) // 4-bit mask because 16-root entries max per sector
    {
        fo->dirccls = fo->dirclus;
        dir = Cache_File_Entry(fo, fHandle, TRUE);
    }
    else
    {
        dir = Cache_File_Entry (fo, fHandle, FALSE);
    }


    // Make sure there is a directory left
    if(dir == (DIRENTRY)NULL)
    {
        status = NO_MORE;
    }
    else
    {
        // Read the first char of the file name
        a = dir->DIR_Name[0];

        // Check for empty or deleted directory
        if ( a == DIR_DEL)
		{
            status = NOT_FOUND;
		}
		else if ( a == DIR_EMPTY)
		{
			status = NO_MORE;
		}
        else
        {
            // Get the attributes
            a = dir->DIR_Attr;

            // print the file name and extension
            for (index=0; index < DIR_NAMESIZE; index++)
            {
                character = dir->DIR_Name[index];
                character = (BYTE)toupper(character);
                fo->name[test++] = character;
            }

            // Get the attributes
            a = dir->DIR_Attr;

            // its possible to have an extension in a directory
            character = dir->DIR_Extension[0];

            // Get the file extension if its there
            for (index=0; index < DIR_EXTENSION; index++)
            {
                character = dir->DIR_Extension[index];
                character = (BYTE)toupper(character);
                fo->name[test++] = character;
            }

            // done and done with the name
            //         fo->name[++test] = (BYTE)'\0';

            // Now store the identifier
            fo->entry = *fHandle;

            // see if we are still a good file
            a = dir->DIR_Name[0];

            if(a == DIR_DEL)
                status = NOT_FOUND;
            else
                status = FOUND;

            // Now store the size
            fo->size = (dir->DIR_FileSize);

            fo->cluster = GetFullClusterNumber(dir); // Get Complete Cluster number.

            /// -Get and store the attributes
            a = dir->DIR_Attr;
            fo->attributes = a;

            // get the date and time
            if ((a & ATTR_DIRECTORY) != 0)
            {
                fo->time = dir->DIR_CrtTime;
                fo->date = dir->DIR_CrtDate;
            }
            else
            {
                fo->time = dir->DIR_WrtTime;
                fo->date = dir->DIR_WrtDate;
            }

        }// deleted directory
    }// Ensure we are still good
    return(status);
} // Fill_File_Object


/************************************************************************
  Function:
    DIRENTRY LoadDirAttrib(FILEOBJ fo, WORD *fHandle)
  Summary:
    Load file information from a directory entry and cache the entry
  Conditions:
    This function should not be called by the user.
  Input:
    fo -       Pointer to file structure
    fHandle -  Information location
  Return Values:
    DIRENTRY - Pointer to the directory entry
    NULL -     Directory entry could not be loaded
  Side Effects:
    None
  Description:
    This function will cache the sector of directory entries
    in the directory pointed to by the dirclus value in
    the FSFILE object 'fo' that contains the entry that
    corresponds to the fHandle offset.  It will then return a pointer
    to the directory entry in the global data buffer.
  Remarks:
    None.
  ************************************************************************/

DIRENTRY LoadDirAttrib(FILEOBJ fo, WORD *fHandle)
{
    DIRENTRY    dir;
    BYTE      a;

    fo->dirccls = fo->dirclus;
    // Get the entry
    dir = Cache_File_Entry( fo, fHandle, TRUE);
    if (dir == NULL)
        return NULL;

    // Read the first char of the file name
    a = dir->DIR_Name[0];

    // Make sure there is a directory left
    if(a == DIR_EMPTY)
        dir = (DIRENTRY)NULL;

    if(dir != (DIRENTRY)NULL)
    {
        // Check for empty or deleted directory
        if ( a == DIR_DEL)
            dir = (DIRENTRY)NULL;
        else
        {
            // Get the attributes
            a = dir->DIR_Attr;

            // scan through all the long dir entries
            while(a == ATTR_LONG_NAME)
            {
                (*fHandle)++;
                dir = Cache_File_Entry( fo, fHandle, FALSE);
                if (dir == NULL)
                    return NULL;
                a = dir->DIR_Attr;
            } // long file name while loop
        } // deleted dir
    }// Ensure we are still good

    return(dir);
} // LoadDirAttrib


/**************************************************************************
  Function:
    CETYPE FILEerase( FILEOBJ fo, WORD *fHandle, BYTE EraseClusters)
  Summary:
    Erase a file
  Conditions:
    This function should not be called by the user.
  Input:
    fo -            Pointer to file structure
    fHandle -       Location of file information
    EraseClusters - Remove cluster allocation from FAT?
  Return Values:
    CE_GOOD - File erased successfully
    CE_FILE_NOT_FOUND - Could not find the file on the card
    CE_ERASE_FAIL - Internal Card erase failed
  Side Effects:
    None
  Description:
    This function will cache the sector of directory entries in the directory
    pointed to by the dirclus value in the FSFILE object 'fo' that contains
    the entry that corresponds to the fHandle offset.  It will then mark that
    entry as deleted.  If the EraseClusters argument is TRUE, the chain of
    clusters for that file will be marked as unused in the FAT by the
    FAT_erase_cluster_chain function.
  Remarks:
    None.
  **************************************************************************/

#ifdef ALLOW_WRITES
CETYPE FILEerase( FILEOBJ fo, WORD *fHandle, BYTE EraseClusters)
{
    DIRENTRY    dir;
    BYTE        a;
    CETYPE      status = CE_GOOD;
    DWORD       clus;
    DISK *      disk;

    disk = fo->dsk;

    // reset the cluster
    clus = fo->dirclus;
    fo->dirccls = clus;

    // load the sector
    dir = Cache_File_Entry(fo, fHandle, TRUE);
    if (dir == NULL)
    {
        FSerrno = CE_ERASE_FAIL;
        return CE_BADCACHEREAD;
    }

    // Fill up the File Object with the information pointed to by fHandle
    a = dir->DIR_Name[0];

    // see if there is something in the dir
    if(dir == (DIRENTRY)NULL || a == DIR_EMPTY)
    {
        status = CE_FILE_NOT_FOUND;
    }
    else
    {
        // Check for empty or deleted directory
        if ( a == DIR_DEL)
        {
            status = CE_FILE_NOT_FOUND;
        }
        else
        {
            // Get the attributes
            a = dir->DIR_Attr;

            /* 8.3 File Name - entry*/
            dir->DIR_Name[0] = DIR_DEL; // mark as deleted

            // Get the starting cluster
            clus = GetFullClusterNumber(dir); // Get Complete Cluster number.

            // Now write it
            if(status != CE_GOOD || !(Write_File_Entry( fo, fHandle)))
            {
                status = CE_ERASE_FAIL;
            }
            else
            {
                if (clus != FatRootDirClusterValue) //
                {
                    if(EraseClusters)
                    {
                        /* Now remove the cluster allocation from the FAT */
                        status = ((FAT_erase_cluster_chain(clus, disk)) ? CE_GOOD : CE_ERASE_FAIL);
                    }
                }
            }
        } // Not already deleted
    }// Not existant

    if (status == CE_GOOD)
        FSerrno = CE_GOOD;
    else
        FSerrno = CE_ERASE_FAIL;

    return (status);
}
#endif

/***************************************************************
  Function:
    int FSrename (const rom char * fileName, FSFILE * fo)
  Summary:
    Change the name of a file or directory
  Conditions:
    File opened.
  Input:
    fileName -  The new name of the file
    fo -        The file to rename
  Return Values:
    0 -   File was renamed successfully
    EOF - File was not renamed
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FSrename function will rename a file.  First, it will
    search through the current working directory to ensure the
    specified new filename is not already in use.  If it isn't,
    the new filename will be written to the file entry of the
    file pointed to by 'fo.'
  Remarks:
    None
  ***************************************************************/

#ifdef ALLOW_WRITES

int FSrename (const char * fileName, FSFILE * fo)
{
    unsigned char j, k = 0;
    char string[12];
    WORD fHandle = 1, goodHandle;
    DIRENTRY    dir;

    FSerrno = CE_GOOD;

    if (fo == NULL)
    {
        FSerrno = CE_FILENOTOPENED;
        return -1;
    }
    // If fo != NULL, rename the file
    if (FormatFileName (fileName, fo->name, 0) == FALSE)
    {
        FSerrno = CE_INVALID_FILENAME;
        return -1;
    }
    else
    {
        for (j = 0; j < 11; j++)
        {
            string[j] = fo->name[j];
        }
        goodHandle = fo->entry;

        fHandle = 0;
        fo->dirccls = fo->dirclus;
        dir = Cache_File_Entry (fo, &fHandle, TRUE);
        if (dir == NULL)
        {
            FSerrno = CE_BADCACHEREAD;
            return -1;
        }
        // Check if the file name is already used
        for (j = 0; j < 11; j++)
        {
            if (dir->DIR_Name[j] != string[j])
                k = 1;
        }
        if (k == 0)
        {
            FSerrno = CE_FILENAME_EXISTS;
            return -1;
        }
        else
            k = 0;

        nextClusterIsLast = FALSE;
        while (1)
        {
            // Look through the entries until we get to the end
            // to make sure the name isn't taken
            dir = Cache_File_Entry (fo, &fHandle, FALSE);
            if (dir == NULL)
            {
                if (nextClusterIsLast == TRUE)
                {
                    break;
                }
                else
                {
                    FSerrno = CE_BADCACHEREAD;
                    return -1;
                }
            }
            if (dir->DIR_Name[0] == 0)
                break;
            for (j = 0; j < 11; j++)
            {
                if (dir->DIR_Name[j] != string[j])
                    k = 1;
            }
            if (k == 0)
            {
                FSerrno = CE_FILENAME_EXISTS;
                return -1;
            }
            else
                k = 0;
            fHandle++;
        }

        fHandle = goodHandle;
        fo->dirccls = fo->dirclus;

        // Get the file entry
        dir = LoadDirAttrib(fo, &fHandle);

        if (dir == NULL)
        {
            FSerrno = CE_BADCACHEREAD;
            return -1;
        }

        for (j = 0; j < 11; j++)
        {
            dir->DIR_Name[j] = fo->name[j];
        }

        // just write the last entry in
        if(!Write_File_Entry(fo,&fHandle))
        {
            FSerrno = CE_WRITE_ERROR;
            return -1;
        }
    }

    return 0;
}

#endif // Allow writes



/*********************************************************************
  Function:
    FSFILE * FSfopen (const char * fileName, const char *mode)
  Summary:
    Open a file
  Conditions:
    For read modes, file exists; FSInit performed
  Input:
    fileName -  The name of the file to open
    mode -
         - WRITE -      Create a new file or replace an existing file
         - READ -       Read data from an existing file
         - APPEND -     Append data to an existing file
         - WRITEPLUS -  Create a new file or replace an existing file (reads also enabled)
         - READPLUS -   Read data from an existing file (writes also enabled)
         - APPENDPLUS - Append data to an existing file (reads also enabled)
  Return Values:
    FSFILE * - The pointer to the file object
    NULL -     The file could not be opened
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    This function will open a file or directory.  First, RAM in the
    dynamic heap or static array will be allocated to a new FSFILE object.
    Then, the specified file name will be formatted to ensure that it's
    in 8.3 format.  Next, the FILEfind function will be used to search
    for the specified file name.  If the name is found, one of three
    things will happen: if the file was opened in read mode, its file
    info will be loaded using the FILEopen function; if it was opened in
    write mode, it will be erased, and a new file will be constructed in
    its place; if it was opened in append mode, its file info will be
    loaded with FILEopen and the current location will be moved to the
    end of the file using the FSfseek function.  If the file was not
    found by FILEfind, it will be created if the mode was specified as
    a write or append mode.  In these cases, a pointer to the heap or
    static FSFILE object array will be returned.  If the file was not
    found and the mode was specified as a read mode, the memory
    allocated to the file will be freed and the NULL pointer value
    will be returned.
  Remarks:
    None.
  *********************************************************************/

FSFILE * FSfopen( const char * fileName, const char *mode )
{
    FILEOBJ    filePtr;
#ifndef FS_DYNAMIC_MEM
    int      fIndex;
#endif
    BYTE   ModeC;
    WORD    fHandle;
    CETYPE   final;

#ifdef FS_DYNAMIC_MEM
    filePtr = (FILEOBJ) FS_malloc(sizeof(FSFILE));
#else

    filePtr = NULL;

    //Pick available file structure
    for( fIndex = 0; fIndex < FS_MAX_FILES_OPEN; fIndex++ )
    {
        if( gFileSlotOpen[fIndex] )   //this slot is available
        {
            gFileSlotOpen[fIndex] = FALSE;
            filePtr = &gFileArray[fIndex];
            break;
        }
    }

    if( filePtr == NULL )
    {
        FSerrno = CE_TOO_MANY_FILES_OPEN;
        return NULL;      //no file structure slot available
    }
#endif

    //Format the source string.
    if( !FormatFileName(fileName, filePtr->name, 0) )
    {
#ifdef FS_DYNAMIC_MEM
        FS_free( (unsigned char *)filePtr );
#else
        gFileSlotOpen[fIndex] = TRUE;   //put this slot back to the pool
#endif
        FSerrno = CE_INVALID_FILENAME;
        return NULL;   //bad filename
    }

    //Read the mode character
    ModeC = mode[0];

    filePtr->dsk = &gDiskData;
    filePtr->cluster = 0;
    filePtr->ccls    = 0;
    filePtr->entry = 0;
    filePtr->attributes = ATTR_ARCHIVE;

    // start at the current directory
#ifdef ALLOW_DIRS
    filePtr->dirclus    = cwdptr->dirclus;
    filePtr->dirccls    = cwdptr->dirccls;
#else
    filePtr->dirclus = FatRootDirClusterValue;
    filePtr->dirccls = FatRootDirClusterValue;
#endif

    // copy file object over
    FileObjectCopy(&gFileTemp, filePtr);

    // See if the file is found
    if(FILEfind (filePtr, &gFileTemp, LOOK_FOR_MATCHING_ENTRY, 0) == CE_GOOD)
    {
        // File is Found
        switch(ModeC)
        {
#ifdef ALLOW_WRITES
            case 'w':
            case 'W':
            {
                // File exists, we want to create a new one, remove it first
                fHandle = filePtr->entry;
                final = FILEerase(filePtr, &fHandle, TRUE);

                if (final == CE_GOOD)
                {
                    // now create a new one
                    final = CreateFileEntry (filePtr, &fHandle, 0);

                    if (final == CE_GOOD)
                    {
                        final = FILEopen (filePtr, &fHandle, 'w');

                        if (filePtr->attributes & ATTR_DIRECTORY)
                        {
                            FSerrno = CE_INVALID_ARGUMENT;
                            final = 0xFF;
                        }

                        if (final == CE_GOOD)
                        {
                            final = FSfseek (filePtr, 0, SEEK_END);
                            if (mode[1] == '+')
                                filePtr->flags.read = 1;
                        }
                    }
                }
                break;
            }

            case 'A':
            case 'a':
            {
                if(filePtr->size != 0)
                {
                    fHandle = filePtr->entry;

                    final = FILEopen (filePtr, &fHandle, 'w');

                    if (filePtr->attributes & ATTR_DIRECTORY)
                    {
                        FSerrno = CE_INVALID_ARGUMENT;
                        final = 0xFF;
                    }

                    if (final == CE_GOOD)
                    {
                        final = FSfseek (filePtr, 0, SEEK_END);
                        if (final != CE_GOOD)
                            FSerrno = CE_SEEK_ERROR;
                        else
                            ReadFAT (&gDiskData, filePtr->ccls);
                        if (mode[1] == '+')
                            filePtr->flags.read = 1;
                    }
                }
                else
                {
                    fHandle = filePtr->entry;
                    final = FILEerase(filePtr, &fHandle, TRUE);

                    if (final == CE_GOOD)
                    {
                        // now create a new one
                        final = CreateFileEntry (filePtr, &fHandle, 0);

                        if (final == CE_GOOD)
                        {
                            final = FILEopen (filePtr, &fHandle, 'w');

                            if (filePtr->attributes & ATTR_DIRECTORY)
                            {
                                FSerrno = CE_INVALID_ARGUMENT;
                                final = 0xFF;
                            }

                            if (final == CE_GOOD)
                            {
                                final = FSfseek (filePtr, 0, SEEK_END);
                                if (final != CE_GOOD)
                                    FSerrno = CE_SEEK_ERROR;
                                if (mode[1] == '+')
                                    filePtr->flags.read = 1;
                            }
                        }
                    }
                }
                break;
            }
#endif
            case 'R':
            case 'r':
            {
                fHandle = filePtr->entry;

                final = FILEopen (filePtr, &fHandle, 'r');
#ifdef ALLOW_WRITES
                if ((mode[1] == '+') && !(filePtr->attributes & ATTR_DIRECTORY))
                    filePtr->flags.write = 1;
#endif
                break;
            }

            default:
                FSerrno = CE_INVALID_ARGUMENT;
                final = 0xFF;;  //indicate error condition
                break;
        }
    }
    else
    {
#ifdef ALLOW_WRITES
        // the file was not found, reset to the default asked
        FileObjectCopy(filePtr, &gFileTemp);

        // File is not Found
        if(ModeC == 'w' || ModeC == 'W' || ModeC == 'a' || ModeC == 'A')
        {
            // use the user requested name
            fHandle = 0;
            final = CreateFileEntry (filePtr, &fHandle, 0);

            if (final == CE_GOOD)
            {
                final = FILEopen (filePtr, &fHandle, 'w');
                if (filePtr->attributes & ATTR_DIRECTORY)
                {
                    FSerrno = CE_INVALID_ARGUMENT;
                    final = 0xFF;
                }

                if (final == CE_GOOD)
                {
                    final = FSfseek (filePtr, 0, SEEK_END);
                    if (final != CE_GOOD)
                        FSerrno = CE_SEEK_ERROR;
                    if (mode[1] == '+')
                        filePtr->flags.read = 1;
                }
            }
        }
        else
#endif
            final = CE_FILE_NOT_FOUND;
    }

    if (MDD_WriteProtectState())
    {
        filePtr->flags.write = 0;;
    }

#ifdef FS_DYNAMIC_MEM
    if( final != CE_GOOD )
    {
        FS_free( (unsigned char *)filePtr );
        filePtr = NULL;
    }
#else
    if( final != CE_GOOD )
    {
        gFileSlotOpen[fIndex] = TRUE;   //put this slot back to the pool
        filePtr = NULL;
    }
#endif
    else
    {
        FSerrno = CE_GOOD;
    }

    return filePtr;
}

/*******************************************************************
  Function:
    long FSftell (FSFILE * fo)
  Summary:
    Determine the current location in a file
  Conditions:
    File opened
  Input:
    fo -  Pointer to file structure
  Return: Current location in the file
  Side Effects:
    The FSerrno variable will be changed
  Description:
    The FSftell function will return the current position in the
    file pointed to by 'fo' by returning the 'seek' variable in the
    FSFILE object, which is used to keep track of the absolute
    location of the current position in the file.
  Remarks:
    None
  *******************************************************************/

long FSftell (FSFILE * fo)
{
    FSerrno = CE_GOOD;
    return (fo->seek);
}


#ifdef ALLOW_WRITES

/*********************************************************************
  Function:
    int FSremove (const char * fileName)
  Summary:
    Delete a file
  Conditions:
    File not opened, file exists
  Input:
    fileName -  Name of the file to erase
  Return Values:
    0 -   File removed
    EOF - File was not removed
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FSremove function will attempt to find the specified file with
    the FILEfind function.  If the file is found, it will be erased
    using the FILEerase function.
  Remarks:
    None
  **********************************************************************/

int FSremove (const char * fileName)
{
    FILEOBJ fo = &tempCWDobj;
    CETYPE result;

    FSerrno = CE_GOOD;

    if (MDD_WriteProtectState())
    {
        FSerrno = CE_WRITE_PROTECTED;
        return (-1);
    }

    //Format the source string
    if( !FormatFileName(fileName, fo->name, 0) )
    {
        FSerrno = CE_INVALID_FILENAME;
        return -1;
    }

    fo->dsk = &gDiskData;
    fo->cluster = 0;
    fo->ccls    = 0;
    fo->entry = 0;
    fo->attributes = ATTR_ARCHIVE;

#ifndef ALLOW_DIRS
    // start at the root directory
    fo->dirclus    = FatRootDirClusterValue;
    fo->dirccls    = FatRootDirClusterValue;
#else
    fo->dirclus = cwdptr->dirclus;
    fo->dirccls = cwdptr->dirccls;
#endif

    // copy file object over
    FileObjectCopy(&gFileTemp, fo);

    // See if the file is found
    result = FILEfind (fo, &gFileTemp, LOOK_FOR_MATCHING_ENTRY, 0);

    if (result != CE_GOOD)
    {
        FSerrno = CE_FILE_NOT_FOUND;
        return -1;
    }

    if (fo->attributes & ATTR_DIRECTORY)
    {
        FSerrno = CE_DELETE_DIR;
        return -1;
    }

    result = FILEerase(fo, &fo->entry, TRUE);
    if( result == CE_GOOD )
        return 0;
    else
    {
        FSerrno = CE_ERASE_FAIL;
        return -1;
    }
}
#endif

/*********************************************************
  Function:
    void FSrewind (FSFILE * fo)
  Summary:
    Set the current position in a file to the beginning
  Conditions:
    File opened.
  Input:
    fo -  Pointer to file structure
  Return Values:
    None
  Side Effects:
    None.
  Description:
    The FSrewind funciton will reset the position of the
    specified file to the beginning of the file.  This
    functionality is faster than using FSfseek to reset
    the position in the file.
  Remarks:
    None.
  *********************************************************/

void FSrewind (FSFILE * fo)
{
#ifdef ALLOW_WRITES
    if (gNeedDataWrite)
        flushData();
#endif
    fo->seek = 0;
    fo->pos = 0;
    fo->sec = 0;
    fo->ccls = fo->cluster;
    gBufferOwner = NULL;
    return;
}

/**************************************************************************
  Function:
    int FSerror (void)
  Summary:
    Return an error code for the last function call
  Conditions:
    The return value depends on the last function called.
  Input:
    None
  Side Effects:
    None.
  Return Values:
    FSInit       -
                 - CE_GOOD �                  No Error
                 - CE_INIT_ERROR �            The physical media could not be initialized
                 - CE_BAD_SECTOR_READ �       The MBR or the boot sector could not be
                                              read correctly
                 - CE_BAD_PARITION �          The MBR signature code was incorrect.
                 - CE_NOT_FORMATTED �         The boot sector signature code was incorrect or
                                              indicates an invalid number of bytes per sector.
                 - CE_UNSUPPORTED_SECTOR_SIZE - The number of bytes per sector is unsupported
                 - CE_CARDFAT32 �             The physical media is FAT32 type (only an error
                                              when FAT32 support is disabled).
                 - CE_UNSUPPORTED_FS �        The device is formatted with an unsupported file
                                              system (not FAT12 or 16).
    FSfopen      -
                 - CE_GOOD �                  No Error
                 - CE_NOT_INIT �              The device has not been initialized.
                 - CE_TOO_MANY_FILES_OPEN �   The function could not allocate any
                                              additional file information to the array
                                              of FSFILE structures or the heap.
                 - CE_INVALID_FILENAME �      The file name argument was invalid.
                 - CE_INVALID_ARGUMENT �      The user attempted to open a directory in a
                                              write mode or specified an invalid mode argument.
                 - CE_FILE_NOT_FOUND �        The specified file (which was to be opened in read
                                              mode) does not exist on the device.
                 - CE_BADCACHEREAD �          A read from the device failed.
                 - CE_ERASE_FAIL �            The existing file could not be erased (when opening
                                              a file in WRITE mode).
                 - CE_DIR_FULL �              The directory is full.
                 - CE_DISK_FULL�              The data memory section is full.
                 - CE_WRITE_ERROR �           A write to the device failed.
                 - CE_SEEK_ERROR �            The current position in the file could not be set to
                                              the end (when the file was opened in APPEND mode).
    FSfclose     -
                 - CE_GOOD �                  No Error
                 - CE_WRITE_ERROR �           The existing data in the data buffer or the new file
                                              entry information could not be written to the device.
                 - CE_BADCACHEREAD �          The file entry information could not be cached
    FSfread      -
                 - CE_GOOD �                  No Error
                 - CE_WRITEONLY �             The file was opened in a write-only mode.
                 - CE_WRITE_ERROR �           The existing data in the data buffer could not be
                                              written to the device.
                 - CE_BAD_SECTOR_READ �       The data sector could not be read.
                 - CE_EOF �                   The end of the file was reached.
                 - CE_COULD_NOT_GET_CLUSTER � Additional clusters in the file could not be loaded.
    FSfwrite     -
                 - CE_GOOD �                  No Error
                 - CE_READONLY �              The file was opened in a read-only mode.
                 - CE_WRITE_PROTECTED �       The device write-protect check function indicated
                                              that the device has been write-protected.
                 - CE_WRITE_ERROR �           There was an error writing data to the device.
                 - CE_BADCACHEREAD �          The data sector to be modified could not be read from
                                              the device.
                 - CE_DISK_FULL �             All data clusters on the device are in use.
    FSfseek      -
                 - CE_GOOD �                  No Error
                 - CE_WRITE_ERROR �           The existing data in the data buffer could not be
                                              written to the device.
                 - CE_INVALID_ARGUMENT �      The specified offset exceeds the size of the file.
                 - CE_BADCACHEREAD �          The sector that contains the new current position
                                              could not be loaded.
                 - CE_COULD_NOT_GET_CLUSTER � Additional clusters in the file could not be
                                              loaded/allocated.
    FSftell      -
                 - CE_GOOD �                  No Error
    FSattrib     -
                 - CE_GOOD �                  No Error
                 - CE_INVALID_ARGUMENT �      The attribute argument was invalid.
                 - CE_BADCACHEREAD �          The existing file entry information could not be
                                              loaded.
                 - CE_WRITE_ERROR �           The file entry information could not be written to
                                              the device.
    FSrename     -
                 - CE_GOOD �                  No Error
                 - CE_FILENOTOPENED �         A null file pointer was passed into the function.
                 - CE_INVALID_FILENAME �      The file name passed into the function was invalid.
                 - CE_BADCACHEREAD �          A read from the device failed.
                 - CE_FILENAME_EXISTS �       A file with the specified name already exists.
                 - CE_WRITE_ERROR �           The new file entry data could not be written to the
                                              device.
    FSfeof       -
                 - CE_GOOD �                  No Error
    FSformat     -
                 - CE_GOOD �                  No Error
                 - CE_INIT_ERROR �            The device could not be initialized.
                 - CE_BADCACHEREAD �          The master boot record or boot sector could not be
                                              loaded successfully.
                 - CE_INVALID_ARGUMENT �      The user selected to create their own boot sector on
                                              a device that has no master boot record, or the mode
                                              argument was invalid.
                 - CE_WRITE_ERROR �           The updated MBR/Boot sector could not be written to
                                              the device.
                 - CE_BAD_PARTITION �         The calculated number of sectors per clusters was
                                              invalid.
                 - CE_NONSUPPORTED_SIZE �     The card has too many sectors to be formatted as
                                              FAT12 or FAT16.
    FSremove     -
                 - CE_GOOD �                  No Error
                 - CE_WRITE_PROTECTED �       The device write-protect check function indicated
                                              that the device has been write-protected.
                 - CE_INVALID_FILENAME �      The specified filename was invalid.
                 - CE_FILE_NOT_FOUND �        The specified file could not be found.
                 - CE_ERASE_FAIL �            The file could not be erased.
    FSchdir      -
                 - CE_GOOD �                  No Error
                 - CE_INVALID_ARGUMENT �      The path string was mis-formed or the user tried to
                                              change to a non-directory file.
                 - CE_BADCACHEREAD �          A directory entry could not be cached.
                 - CE_DIR_NOT_FOUND �         Could not find a directory in the path.
    FSgetcwd     -
                 - CE_GOOD �                  No Error
                 - CE_INVALID_ARGUMENT �      The user passed a 0-length buffer into the function.
                 - CE_BADCACHEREAD �          A directory entry could not be cached.
                 - CE_BAD_SECTOR_READ �       The function could not determine a previous directory
                                              of the current working directory.
    FSmkdir      -
                 - CE_GOOD �                  No Error
                 - CE_WRITE_PROTECTED �       The device write-protect check function indicated
                                              that the device has been write-protected.
                 - CE_INVALID_ARGUMENT �      The path string was mis-formed.
                 - CE_BADCACHEREAD �          Could not successfully change to a recently created
                                              directory to store its dir entry information, or
                                              could not cache directory entry information.
                 - CE_INVALID_FILENAME �      One or more of the directory names has an invalid
                                              format.
                 - CE_WRITE_ERROR �           The existing data in the data buffer could not be
                                              written to the device or the dot/dotdot entries could
                                              not be written to a newly created directory.
                 - CE_DIR_FULL �              There are no available dir entries in the CWD.
                 - CE_DISK_FULL �             There are no available clusters in the data region of
                                              the device.
    FSrmdir      -
                 - CE_GOOD �                  No Error
                 - CE_DIR_NOT_FOUND �         The directory specified could not be found or the
                                              function could not change to a subdirectory within
                                              the directory to be deleted (when recursive delete is
                                              enabled).
                 - CE_INVALID_ARGUMENT �      The user tried to remove the CWD or root directory.
                 - CE_BADCACHEREAD �          A directory entry could not be cached.
                 - CE_DIR_NOT_EMPTY �         The directory to be deleted was not empty and
                                              recursive subdirectory removal was disabled.
                 - CE_ERASE_FAIL �            The directory or one of the directories or files
                                              within it could not be deleted.
                 - CE_BAD_SECTOR_READ �       The function could not determine a previous directory
                                              of the CWD.
    SetClockVars -
                 - CE_GOOD �                  No Error
                 - CE_INVALID_ARGUMENT �      The time values passed into the function were
                                              invalid.
    FindFirst    -
                 - CE_GOOD �                  No Error
                 - CE_INVALID_FILENAME �      The specified filename was invalid.
                 - CE_FILE_NOT_FOUND �        No file matching the specified criteria was found.
                 - CE_BADCACHEREAD �          The file information for the file that was found
                                              could not be cached.
    FindNext     -
                 - CE_GOOD �                  No Error
                 - CE_NOT_INIT �              The SearchRec object was not initialized by a call to
                                              FindFirst.
                 - CE_INVALID_ARGUMENT �      The SearchRec object was initialized in a different
                                              directory from the CWD.
                 - CE_INVALID_FILENAME �      The filename is invalid.
                 - CE_FILE_NOT_FOUND �        No file matching the specified criteria was found.
    FSfprintf    -
                 - CE_GOOD �                  No Error
                 - CE_WRITE_ERROR �           Characters could not be written to the file.
  Description:
    The FSerror function will return the FSerrno variable.  This global
    variable will have been set to an error value during the last call of a
    library function.
  Remarks:
    None
  **************************************************************************/

int FSerror (void)
{
    return FSerrno;
}


/**************************************************************
  Function:
    void FileObjectCopy(FILEOBJ foDest,FILEOBJ foSource)
  Summary:
    Copy a file object
  Conditions:
    This function should not be called by the user.
  Input:
    foDest -    The destination
    foSource -  the source
  Return:
    None
  Side Effects:
    None
  Description:
    The FileObjectCopy function will make an exacy copy of
    a specified FSFILE object.
  Remarks:
    None
  **************************************************************/

void FileObjectCopy(FILEOBJ foDest,FILEOBJ foSource)
{
    BYTE size;
    BYTE *dest;
    BYTE *source;
    BYTE Index;

    dest = (BYTE *)foDest;
    source = (BYTE *)foSource;

    size = sizeof(FSFILE);

    for(Index=0;Index< size; Index++)
    {
        dest[Index] = source[Index];
    }
}

/*************************************************************************
  Function:
    CETYPE FILECreateHeadCluster( FILEOBJ fo, DWORD *cluster)
  Summary:
    Create the first cluster of a file
  Conditions:
    This function should not be called by the user.
  Input:
    fo -       Pointer to file structure
    cluster -  Cluster location
  Return Values:
    CE_GOOD - File closed successfully
    CE_WRITE_ERROR - Could not write to the sector
    CE_DISK_FULL - All clusters in partition are taken
  Side Effects:
    None
  Description:
    The FILECreateHeadCluster function will create the first cluster
    of a file.  First, it will find an empty cluster with the
    FATfindEmptyCluster function and mark it as the last cluster in the
    file.  It will then erase the cluster using the EraseCluster function.
  Remarks:
    None.
  *************************************************************************/

#ifdef ALLOW_WRITES
CETYPE FILECreateHeadCluster( FILEOBJ fo, DWORD *cluster)
{
    DISK *      disk;
    CETYPE        error = CE_GOOD;

    disk = fo->dsk;

    // find the next empty cluster
    *cluster = FATfindEmptyCluster(fo);

    if(*cluster == 0)  // "0" is just an indication as Disk full in the fn "FATfindEmptyCluster()"
    {
        error = CE_DISK_FULL;
    }
    else
    {
        // mark the cluster as taken, and last in chain
        if(disk->type == FAT12)
        {
            if(WriteFAT( disk, *cluster, LAST_CLUSTER_FAT12, FALSE) == CLUSTER_FAIL_FAT16)
            {
                error = CE_WRITE_ERROR;
            }
        }
        else if(disk->type == FAT16)
        {
            if(WriteFAT( disk, *cluster, LAST_CLUSTER_FAT16, FALSE) == CLUSTER_FAIL_FAT16)
            {
                error = CE_WRITE_ERROR;
            }
        }

 #ifdef SUPPORT_FAT32 // If FAT32 supported.
        else
        {
            if(WriteFAT( disk, *cluster, LAST_CLUSTER_FAT32, FALSE) == CLUSTER_FAIL_FAT32)
            {
                error = CE_WRITE_ERROR;
            }
        }
#endif

        // lets erase this cluster
        if(error == CE_GOOD)
        {
            error = EraseCluster(disk,*cluster);
        }
    }

    return(error);
} // allocate head cluster
#endif

/*************************************************************************
  Function:
    BYTE EraseCluster(DISK *disk, DWORD cluster)
  Summary:
    Erase a cluster
  Conditions:
    This function should not be called by the user.
  Input:
    dsk -      Disk structure
    cluster -  Cluster to be erased
  Return Values:
    CE_GOOD - File closed successfully
    CE_WRITE_ERROR - Could not write to the sector
  Side Effects:
    None
  Description:
    The EraseCluster function will write a 0 value into every byte of
    the specified cluster.
  Remarks:
    None.
  *************************************************************************/

#ifdef ALLOW_WRITES
BYTE EraseCluster(DISK *disk, DWORD cluster)
{
    BYTE index;
    DWORD SectorAddress;
    BYTE error = CE_GOOD;

    SectorAddress = Cluster2Sector(disk,cluster);
    if (gNeedDataWrite)
        if (flushData())
            return CE_WRITE_ERROR;

    gBufferOwner = NULL;

    if (gBufferZeroed == FALSE)
    {
        // clear out the memory first
        memset(disk->buffer, 0x00, MEDIA_SECTOR_SIZE);
        gBufferZeroed = TRUE;
    }

    // Now clear them out
    for(index = 0; index < disk->SecPerClus && error == CE_GOOD; index++)
    {
        if (MDD_SectorWrite( SectorAddress++, disk->buffer, FALSE) != TRUE)
            error = CE_WRITE_ERROR;
    }

    return(error);
}
#endif



/***************************************************
  Function:
    BYTE ReadByte(BYTE * pBuffer, WORD index)
  Summary:
    Read a byte from a buffer
  Conditions:
    This function should not be called by the user.
  Input:
    pBuffer -  pointer to a buffer to read from
    index -    index in the buffer to read to
  Return:
    BYTE - the byte read
  Side Effects:
    None
  Description:
    Reads a byte from a buffer
  Remarks:
    None.
  ***************************************************/

BYTE ReadByte( BYTE* pBuffer, WORD index )
{
    return( pBuffer[index] );
}


/***************************************************
  Function:
    BYTE ReadWord(BYTE * pBuffer, WORD index)
  Summary:
    Read a 16-bit word from a buffer
  Conditions:
    This function should not be called by the user.
  Input:
    pBuffer -  pointer to a buffer to read from
    index -    index in the buffer to read to
  Return:
    WORD - the word read
  Side Effects:
    None
  Description:
    Reads a 16-bit word from a buffer
  Remarks:
    None.
  ***************************************************/

WORD ReadWord( BYTE* pBuffer, WORD index )
{
    BYTE loByte, hiByte;
    WORD res;

    loByte = pBuffer[index];
    hiByte = pBuffer[index+1];
    res = hiByte;
    res *= 0x100;
    res |= loByte;
    return( res );
}


/****************************************************
  Function:
    BYTE ReadDWord(BYTE * pBuffer, WORD index)
  Summary:
    Read a 32-bit double word from a buffer
  Conditions:
    This function should not be called by the user.
  Input:
    pBuffer -  pointer to a buffer to read from
    index -    index in the buffer to read to
  Return:
    DWORD - the double word read
  Side Effects:
    None
  Description:
    Reads a 32-bit double word from a buffer
  Remarks:
    None.
  ****************************************************/

DWORD ReadDWord( BYTE* pBuffer, WORD index )
{
    WORD loWord, hiWord;
    DWORD result;

    loWord = ReadWord( pBuffer, index );
    hiWord = ReadWord( pBuffer, index+2 );

    result = hiWord;
    result *= 0x10000;
    result |= loWord;
    return result;
}




/****************************************************
  Function:
    DWORD Cluster2Sector(DISK * dsk, DWORD cluster)
  Summary:
    Convert a cluster number to the corresponding sector
  Conditions:
    This function should not be called by the user.
  Input:
    disk -     Disk structure
    cluster -  Cluster to be converted
  Return:
    sector - Sector that corresponds to given cluster
  Side Effects:
    None
  Description:
    The Cluster2Sector function will calculate the
    sector number that corresponds to the first sector
    of the cluster whose value was passed into the
    function.
  Remarks:
    None.
  ****************************************************/

DWORD Cluster2Sector(DISK * dsk, DWORD cluster)
{
    DWORD sector;

    /* Rt: Settings based on FAT type */
    switch (dsk->type)
    {
#ifdef SUPPORT_FAT32 // If FAT32 supported.
        case FAT32:
            /* In FAT32, there is no separate ROOT region. It is as well stored in DATA region */
            sector = (((DWORD)cluster-2) * dsk->SecPerClus) + dsk->data;
            break;
#endif
        case FAT12:
        case FAT16:
        default:
            // The root dir takes up cluster 0 and 1
            if(cluster == 0 ||cluster == 1)
                sector = dsk->root + cluster;
            else
                sector = (((DWORD)cluster-2) * dsk->SecPerClus) + dsk->data;
            break;
    }

    return(sector);

}


/***************************************************************************
  Function:
    int FSattrib (FSFILE * file, unsigned char attributes)
  Summary:
    Change the attributes of a file
  Conditions:
    File opened
  Input:
    file -        Pointer to file structure
    attributes -  The attributes to set for the file
               -  Attribute -      Value - Indications
               -  ATTR_READ_ONLY - 0x01  - The read-only attribute
               -  ATTR_HIDDEN -    0x02  - The hidden attribute
               -  ATTR_SYSTEM -    0x04  - The system attribute
               -  ATTR_ARCHIVE -   0x20  - The archive attribute
  Return Values:
    0 -  Attribute change was successful
    -1 - Attribute change was unsuccessful
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FSattrib funciton will set the attributes of the specified file
    to the attributes passed in by the user.  This function will load the
    file entry, replace the attributes with the ones specified, and write
    the attributes back.  If the specified file is a directory, the
    directory attribute will be preserved.
  Remarks:
    None
  ***************************************************************************/

#ifdef ALLOW_WRITES
int FSattrib (FSFILE * file, unsigned char attributes)
{
    WORD fHandle;
    DIRENTRY dir;

    FSerrno = CE_GOOD;

    // Check for valid attributes
    if ((attributes & ~0x27) != 0)
    {
        FSerrno = CE_INVALID_ARGUMENT;
        return -1;
    }

    fHandle = file->entry;

    file->dirccls = file->dirclus;

    // Get the file entry
    dir = LoadDirAttrib(file, &fHandle);

    if (dir == NULL)
    {
        FSerrno = CE_BADCACHEREAD;
        return -1;
    }

    // Ensure that we aren't trying to change the
    // attributes of a volume entry
    if (dir->DIR_Attr & ATTR_VOLUME)
    {
        FSerrno = CE_INVALID_ARGUMENT;
        return -1;
    }

    // Don't remove the directory attribute from DIR files
    if (file->attributes & ATTR_DIRECTORY)
        file->attributes = attributes | ATTR_DIRECTORY;
    else
        file->attributes = attributes;

    // just write the last entry in
    if(!Write_File_Entry(file,&fHandle))
    {
        FSerrno = CE_WRITE_ERROR;
        return -1;
    }

    return 0;
}
#endif


/*********************************************************************************
  Function:
    size_t FSfwrite(const void *ptr, size_t size, size_t n, FSFILE *stream)
  Summary:
    Write data to a file
  Conditions:
    File opened in WRITE, APPEND, WRITE+, APPEND+, READ+ mode
  Input:
    ptr -     Pointer to source buffer
    size -    Size of units in bytes
    n -       Number of units to transfer
    stream -  Pointer to file structure
  Return:
    size_t - number of units written
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FSfwrite function will write data to a file.  First, the sector that
    corresponds to the current position in the file will be loaded (if it hasn't
    already been cached in the global data buffer).  Data will then be written to
    the device from the specified buffer until the specified amount has been written.
    If the end of a cluster is reached, the next cluster will be loaded, unless
    the end-of-file flag for the specified file has been set.  If it has, a new
    cluster will be allocated to the file.  Finally, the new position and filezize
    will be stored in the FSFILE object.  The parameters 'size' and 'n' indicate how
    much data to write.  'Size' refers to the size of one object to write (in bytes),
    and 'n' will refer to the number of these objects to write.  The value returned
    will be equal  to 'n' unless an error occured.
  Remarks:
    None.
  *********************************************************************************/

#ifdef ALLOW_WRITES
size_t FSfwrite(const void *ptr, size_t size, size_t n, FSFILE *stream)
{
    DWORD       count = size * n;
    BYTE   *    src = (BYTE *) ptr;
    DISK   *    dsk;                 // pointer to disk structure
    CETYPE      error = CE_GOOD;
    WORD        pos;
    DWORD       l;                     // absolute lba of sector to load
    DWORD       seek, filesize;
    WORD        writeCount = 0;

    // see if the file was opened in a write mode
    if(!(stream->flags.write))
    {
        FSerrno = CE_READONLY;
        error = CE_WRITE_ERROR;
        return 0;
    }

    if (count == 0)
        return 0;

    if (MDD_WriteProtectState())
    {
        FSerrno = CE_WRITE_PROTECTED;
        error = CE_WRITE_PROTECTED;
        return 0;
    }

    gBufferZeroed = FALSE;
    dsk = stream->dsk;
    // get the stated position
    pos = stream->pos;
    seek = stream->seek;
    l = Cluster2Sector(dsk,stream->ccls);
    l += (WORD)stream->sec;      // add the sector number to it

    // Check if the current stream was the last one to use the
    // buffer. If not, check if we need to write data from the
    // old stream
    if (gBufferOwner != stream)
    {
        if (gNeedDataWrite)
        {
            if (flushData())
            {
                FSerrno = CE_WRITE_ERROR;
                return 0;
            }
        }
        gBufferOwner = stream;
    }
    if (gLastDataSectorRead != l)
    {
        if (gNeedDataWrite)
        {
            if (flushData())
            {
                FSerrno = CE_WRITE_ERROR;
                return 0;
            }
        }

        gBufferZeroed = FALSE;
        if(!MDD_SectorRead( l, dsk->buffer) )
        {
            FSerrno = CE_BADCACHEREAD;
            error = CE_BAD_SECTOR_READ;
        }
        gLastDataSectorRead = l;
    }
    // exit loop if EOF reached
    filesize = stream->size;

    // Loop while writing bytes
    while (error == CE_GOOD && count > 0)
    {
        if( seek == filesize )
            stream->flags.FileWriteEOF = TRUE;

        // load a new sector if necessary, multiples of sector
        if (pos == dsk->sectorSize)
        {
            BYTE needRead = TRUE;

            if (gNeedDataWrite)
                if (flushData())
                {
                    FSerrno = CE_WRITE_ERROR;
                    return 0;
                }

            // reset position
            pos = 0;

            // point to the next sector
            stream->sec++;

            // get a new cluster if necessary
            if (stream->sec == dsk->SecPerClus)
            {
                stream->sec = 0;

                if(stream->flags.FileWriteEOF)
                {
                    error = FILEallocate_new_cluster(stream, 0);    // add new cluster to the file
                    needRead = FALSE;
                }
                else
                    error = FILEget_next_cluster( stream, 1);
            }

            if (error == CE_DISK_FULL)
            {
                FSerrno = CE_DISK_FULL;
                return 0;
            }

            if(error == CE_GOOD)
            {
                l = Cluster2Sector(dsk,stream->ccls);
                l += (WORD)stream->sec;      // add the sector number to it
                gBufferOwner = stream;
                // If we just allocated a new cluster, then the cluster will
                // contain garbage data, so it doesn't matter what we write to it
                // Whatever is in the buffer will work fine
                if (needRead)
                {
                    if( !MDD_SectorRead( l, dsk->buffer) )
                    {
                        FSerrno = CE_BADCACHEREAD;
                        error = CE_BAD_SECTOR_READ;
                        gLastDataSectorRead = 0xFFFFFFFF;
                        return 0;
                    }
                    else
                    {
                        gLastDataSectorRead = l;
                    }
                }
                else
                    gLastDataSectorRead = l;
            }
        } //  load new sector

        if(error == CE_GOOD)
        {
            // Write one byte at a time
            RAMwrite(dsk->buffer, pos++, *(char *)src);
            src = src + 1; // compiler bug
            seek++;
            count--;
            writeCount++;
            // now increment the size of the part
            if(stream->flags.FileWriteEOF)
                filesize++;
            gNeedDataWrite = TRUE;
        }
    } // while count

    // save off the positon
    stream->pos = pos;

    // save off the seek
    stream->seek = seek;

    // now the new size
    stream->size = filesize;

    return(writeCount / size);
} // fwrite
#endif


/**********************************************************
  Function:
    BYTE flushData (void)
  Summary:
    Flush unwritten data to a file
  Conditions:
    File opened in a write mode, data needs to be written
  Return Values:
    CE_GOOD -        Data was updated successfully
    CE_WRITE_ERROR - Data could not be updated
  Side Effects:
    None
  Description:
    The flushData function is called when it is necessary to
    read new data into the global data buffer and the
    gNeedDataWrite variable indicates that there is data
    in the buffer that hasn't been written to the device.
    The flushData function will write the data from the
    buffer into the current cluster of the FSFILE object
    that is stored in the gBufferOwner global variable.
  Remarks:
    None
  **********************************************************/

#ifdef ALLOW_WRITES
BYTE flushData (void)
{
    DWORD l;
    DISK * dsk;

    // This will either be the pointer to the last file, or the handle
    FILEOBJ stream = gBufferOwner;

    dsk = stream->dsk;

    // figure out the lba
    l = Cluster2Sector(dsk,stream->ccls);
    l += (WORD)stream->sec;      // add the sector number to it

    if(!MDD_SectorWrite( l, dsk->buffer, FALSE))
    {
        return CE_WRITE_ERROR;
    }

    gNeedDataWrite = FALSE;

    return CE_GOOD;
}
#endif

/****************************************************
  Function:
    int FSfeof( FSFILE * stream )
  Summary:
    Indicate whether the current file position is at the end
  Conditions:
    File is open in a read mode
  Input:
    stream -  Pointer to the target file
  Return Values:
    Non-Zero - EOF reached
    0 - Not at end of File
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FSfeof function will indicate that the end-of-
    file has been reached for the specified file by
    comparing the absolute location in the file to the
    size of the file.
  Remarks:
    None.
  ****************************************************/

int FSfeof( FSFILE * stream )
{
    FSerrno = CE_GOOD;
    return( stream->seek == stream->size );
}


/**************************************************************************
  Function:
    size_t FSfread(void *ptr, size_t size, size_t n, FSFILE *stream)
  Summary:
    Read data from a file
  Conditions:
    File is opened in a read mode
  Input:
    ptr -     Destination buffer for read bytes
    size -    Size of units in bytes
    n -       Number of units to be read
    stream -  File to be read from
  Return:
    size_t - number of units read
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FSfread function will read data from the specified file.  First,
    the appropriate sector of the file is loaded.  Then, data is read into
    the specified buffer until the specified number of bytes have been read.
    When a cluster boundary is reached, a new cluster will be loaded.  The
    parameters 'size' and 'n' indicate how much data to read.  'Size'
    refers to the size of one object to read (in bytes), and 'n' will refer
    to the number of these objects to read.  The value returned will be equal
    to 'n' unless an error occured or the user tried to read beyond the end
    of the file.
  Remarks:
    None.
  **************************************************************************/

size_t FSfread (void *ptr, size_t size, size_t n, FSFILE *stream)
{
    DWORD   len = size * n;
    BYTE    *pointer = (BYTE *) ptr;
    DISK    *dsk;               // Disk structure
    DWORD    seek, sec_sel;
    WORD    pos;       //position within sector
    CETYPE   error = CE_GOOD;
    WORD    readCount = 0;

    FSerrno = CE_GOOD;

    dsk    = (DISK *)stream->dsk;
    pos    = stream->pos;
    seek    = stream->seek;

    if( !stream->flags.read )
    {
        FSerrno = CE_WRITEONLY;
        return 0;   // CE_WRITEONLY
    }

#ifdef ALLOW_WRITES
    if (gNeedDataWrite)
        if (flushData())
        {
            FSerrno = CE_WRITE_ERROR;
            return 0;
        }
#endif

    // if it not my buffer, then get it from the disk.
    if( (gBufferOwner != stream) && (pos != dsk->sectorSize))
    {
        gBufferOwner = stream;
        sec_sel = Cluster2Sector(dsk,stream->ccls);
        sec_sel += (WORD)stream->sec;      // add the sector number to it

        gBufferZeroed = FALSE;
        if( !MDD_SectorRead( sec_sel, dsk->buffer) )
        {
            FSerrno = CE_BAD_SECTOR_READ;
            error = CE_BAD_SECTOR_READ;
            return 0;
        }
        gLastDataSectorRead = sec_sel;
    }

    //loop reading (count) bytes
    while( len )
    {
        if( seek == stream->size )
        {
            FSerrno = CE_EOF;
            error = CE_EOF;
            break;
        }

        // In fopen, pos is init to 0 and the sect is loaded
        if( pos == dsk->sectorSize )
        {
            // reset position
            pos = 0;
            // point to the next sector
            stream->sec++;

            // get a new cluster if necessary
            if( stream->sec == dsk->SecPerClus )
            {
                stream->sec = 0;
                if( (error = FILEget_next_cluster( stream, 1)) != CE_GOOD )
                {
                    FSerrno = CE_COULD_NOT_GET_CLUSTER;
                    break;
                }
            }

            sec_sel = Cluster2Sector(dsk,stream->ccls);
            sec_sel += (WORD)stream->sec;      // add the sector number to it


            gBufferOwner = stream;
            gBufferZeroed = FALSE;
            if( !MDD_SectorRead( sec_sel, dsk->buffer) )
            {
                FSerrno = CE_BAD_SECTOR_READ;
                error = CE_BAD_SECTOR_READ;
                break;
            }
            gLastDataSectorRead = sec_sel;
        }

        // copy one byte at a time
        *pointer = RAMread( dsk->buffer, pos++ );
        pointer++;
        seek++;
        readCount++;
        len--;
    }

    // save off the positon
    stream->pos = pos;
    // save off the seek
    stream->seek = seek;

    return(readCount / size);
} // fread


/***************************************************************************
  Function:
    BYTE FormatFileName( const char* fileName, char* fN2, BYTE mode )
  Summary:
    Format a file name into dir entry format
  Conditions:
    This function should not be called by the user.
  Input:
    fileName -  The name to be formatted
    fN2 -       The location the formatted name will be stored
    mode -      Non-zero if parital string search chars are allowed
  Return Values:
    TRUE - Name formatted successfully
    FALSE - File name could not be formatted
  Side Effects:
    None
  Description:
    Format an 8.3 filename into FSFILE structure format. If filename is less
    than 8 chars, then it will be padded with spaces. If the extension name is
    fewer than 3 chars, then it will also be oadded with spaces. The
    ValidateChars function is used to ensure the characters in the specified
    filename are valid in this filesystem.
  Remarks:
    None.
  ***************************************************************************/
BYTE FormatFileName( const char* fileName, char* fN2, BYTE mode)
{
    char * pExt;
    WORD temp;
    char szName[15];
    BYTE count;

    for (count = 0; count < 11; count++)
    {
        *(fN2 + count) = ' '; // Load destination filename to be space intially.
    }

    // Make sure we dont have an empty string or a name with only
    // an extension
    if (fileName[0] == '.' || fileName[0] == 0)
        return FALSE;

    temp = strlen( fileName );

    if( temp <= TOTAL_FILE_SIZE ) // 8+3+1
        strcpy( szName, fileName );  // copy to RAM in case fileName is located in flash
    else
        return FALSE; //long file name

    // Make sure the characters are valid
    if ( !ValidateChars(szName, mode) )
        return FALSE;

    //Look for '.' in the szName
    if( (pExt = strchr( szName, '.' )) != 0 )
    {
        *pExt = 0; // Assigning NULL here makes the "szName" to be terminated and "pExt" pointer to hold only extn characters.
        pExt++; // now pointing to extension

        if( strlen( pExt ) > 3 ) // make sure the extension is 3 bytes or fewer
            return FALSE;
    }

    if( strlen(szName) > 8 )
        return FALSE;

    //copy file name
    for (count = 0; count < strlen(szName); count++)
    {
        *(fN2 + count) = * (szName + count); // Destination filename initially filled with SPACE. Now copy only available chars.
    }

    //copy extension
    if(pExt && *pExt )
    {
        for (count = 0; count < strlen (pExt); count++)
        {
            *(fN2 + count + 8) = *(pExt + count); // Copy the extn to 8th position onwards. Ex: "FILE    .Tx "
        }
    }

    return TRUE;
}

#ifdef ALLOW_DIRS

/*************************************************************************
  Function:
    BYTE FormatDirName (char * string, BYTE mode)
  Summary:
    Format a dir name into dir entry format
  Conditions:
    This function should not be called by the user.
  Input:
    string -  The name to be formatted
    mode -
         - TRUE -  Partial string search characters are allowed
         - FALSE - Partial string search characters are forbidden
  Return Values:
    TRUE - The name was formatted correctly
    FALSE - The name contained invalid characters
  Side Effects:
    None
  Description:
    Format an 8.3 filename into directory structure format. If the name is less
    than 8 chars, then it will be padded with spaces. If the extension name is
    fewer than 3 chars, then it will also be oadded with spaces. The
    ValidateChars function is used to ensure the characters in the specified
    directory name are valid in this filesystem.
  Remarks:
    None.
  *************************************************************************/

BYTE FormatDirName (char * string, BYTE mode)
{
    unsigned char i, j;
    char tempString [12];

    if (ValidateChars (string, mode) == FALSE)
        return FALSE;

    for (i = 0; (i < 8) && (*(string + i) != '.') && (*(string + i) != 0); i++)
    {
        tempString[i] = *(string + i);
    }

    j = i;

    while (i < 8)
    {
        tempString [i++] = 0x20;
    }

    if (*(string + j) == '.')
    {
        j++;
        while (*(string + j) != 0)
        {
            tempString[i++] = *(string + j++);
        }
    }

    while (i < 11)
    {
        tempString[i++] = 0x20;
    }

    tempString[11] = 0;

    // Forbidden
    if (tempString[0] == 0x20)
    {
        tempString[0] = '_';
    }

    for (i = 0; i < 12; i++)
    {
        *(string + i) = tempString[i];
    }

    return TRUE;
}
#endif


/*************************************************************
  Function:
    BYTE ValidateChars( char * FileName, BYTE mode)
  Summary:
    Validate the characters in a given file name
  Conditions:
    This function should not be called by the user.
  Input:
    fileName -  The name to be validated
    mode -      Determines if partial string search is allowed
  Return Values:
    TRUE - Name was validated
    FALSE - File name was not valid
  Side Effects:
    None
  Description:
    The ValidateChars function will compare characters in a
    specified filename to determine if they're permissable
    in the FAT file system.  Lower-case characters will be
    converted to upper-case.  If the mode argument is specifed
    to be 'TRUE,' partial string search characters are allowed.
  Remarks:
    None.
  *************************************************************/
BYTE ValidateChars( char * FileName , BYTE mode)
{
    int StrSz, index;
    unsigned char radix = FALSE;

    StrSz = strlen(FileName);

    for( index = 0; index < StrSz; index++ )
    {
        if (((FileName[index] <= 0x20) && (FileName[index] != 0x05)) ||
            (FileName[index] == 0x22) || (FileName[index] == 0x2B) ||
            (FileName[index] == 0x2C) || (FileName[index] == 0x2F) ||
            (FileName[index] == 0x3A) || (FileName[index] == 0x3B) ||
            (FileName[index] == 0x3C) || (FileName[index] == 0x3D) ||
            (FileName[index] == 0x3E) || (FileName[index] == 0x5B) ||
            (FileName[index] == 0x5C) || (FileName[index] == 0x5D) ||
            (FileName[index] == 0x7C) || ((FileName[index] == 0x2E) && radix == TRUE))
        {
            return FALSE;
        }
        else
        {
            // Check for partial string search chars
            if (mode == FALSE)
            {
                if ((FileName[index] == '*') || (FileName[index] == '?'))
                    return FALSE;
            }
            // only one radix ('.') character is allowed
            if (FileName[index] == 0x2E)
            {
                radix = TRUE;
            }
            // Convert lower-case to upper-case
            if ((FileName[index] >= 0x61) && (FileName[index] <= 0x7A))
                FileName[index] -= 0x20;
        }
    }
    return TRUE;
}


/**********************************************************************
  Function:
    int FSfseek(FSFILE *stream, long offset, int whence)
  Summary:
    Change the current position in a file
  Conditions:
    File opened
  Input:
    stream -    Pointer to file structure
    offset -    Offset from base location
    whence -
           - SEEK_SET -  Seek from start of file
           - SEEK_CUR -  Seek from current location
           - SEEK_END -  Seek from end of file (subtract offset)
  Return Values:
    0 -  Operation successful
    -1 - Operation unsuccesful
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FSfseek function will change the current position in the file to
    one specified by the user.  First, an absolute offset is calculated
    using the offset and base location passed in by the user.  Then, the
    position variables are updated, and the sector number that corresponds
    to the new location.  That sector is then loaded.  If the offset
    falls exactly on a cluster boundary, a new cluster will be allocated
    to the file and the position will be set to the first byte of that
    cluster.
  Remarks:
    None
  **********************************************************************/

int FSfseek(FSFILE *stream, long offset, int whence)
{
    DWORD    numsector, temp;   // lba of first sector of first cluster
    DISK*   dsk;            // pointer to disk structure
    BYTE   test;
    long offset2 = offset;

    dsk = stream->dsk;

    switch(whence)
    {
        case SEEK_CUR:
            // Apply the offset to the current position
            offset2 += stream->seek;
            break;
        case SEEK_END:
            // Apply the offset to the end of the file
            offset2 = stream->size - offset2;
            break;
        case SEEK_SET:
            // automatically there
        default:
            break;
   }

#ifdef ALLOW_WRITES
    if (gNeedDataWrite)
        if (flushData())
        {
            FSerrno = CE_WRITE_ERROR;
            return EOF;
        }
#endif

    // start from the beginning
    temp = stream->cluster;
    stream->ccls = temp;

    temp = stream->size;

    if (offset2 > temp)
    {
        FSerrno = CE_INVALID_ARGUMENT;
        return (-1);      // past the limits
    }
    else
    {
        // if we are writing we are no longer at the end
        stream->flags.FileWriteEOF = FALSE;

        // set the new postion
        stream->seek = offset2;

        // figure out how many sectors
        numsector = offset2 / dsk->sectorSize;

        // figure out how many bytes off of the offset
        offset2 = offset2 - (numsector * dsk->sectorSize);
        stream->pos = offset2;

        // figure out how many clusters
        temp = numsector / dsk->SecPerClus;

        // figure out the stranded sectors
        numsector = numsector - (dsk->SecPerClus * temp);
        stream->sec = numsector;

        // if we are in the current cluster stay there
        if (temp > 0)
        {
            test = FILEget_next_cluster(stream, temp);
            if (test != CE_GOOD)
            {
                if (test == CE_FAT_EOF)
                {
#ifdef ALLOW_WRITES
                    if (stream->flags.write)
                    {
                        // load the previous cluster
                        stream->ccls = stream->cluster;
                        // Don't perform this operation if there's only one cluster
                        if (temp != 1)
                        test = FILEget_next_cluster(stream, temp - 1);
                        if (FILEallocate_new_cluster(stream, 0) != CE_GOOD)
                        {
                            FSerrno = CE_COULD_NOT_GET_CLUSTER;
                            return -1;
                        }
                        // sec and pos should already be zero
                    }
                    else
                    {
#endif
                        stream->ccls = stream->cluster;
                        test = FILEget_next_cluster(stream, temp - 1);
                        if (test != CE_GOOD)
                        {
                            FSerrno = CE_COULD_NOT_GET_CLUSTER;
                            return (-1);
                        }
                        stream->pos = dsk->sectorSize;
                        stream->sec = dsk->SecPerClus - 1;
#ifdef ALLOW_WRITES
                    }
#endif
                }
                else
                {
                    FSerrno = CE_COULD_NOT_GET_CLUSTER;
                    return (-1);   // past the limits
                }
            }
        }

        // Determine the lba of the selected sector and load
        temp = Cluster2Sector(dsk,stream->ccls);

        // now the extra sectors
        numsector = stream->sec;
        temp += numsector;

        gBufferOwner = NULL;
        gBufferZeroed = FALSE;
        if( !MDD_SectorRead(temp, dsk->buffer) )
        {
            FSerrno = CE_BADCACHEREAD;
            return (-1);   // Bad read
        }
        gLastDataSectorRead = temp;
    }

    FSerrno = CE_GOOD;

    return (0);
}


// FSfopenpgm, FSremovepgm, and FSrenamepgm will only work on PIC18s
#ifdef __18CXX
#ifdef ALLOW_PGMFUNCTIONS

#ifdef ALLOW_WRITES

/*****************************************************************
  Function:
    int FSrenamepgm(const rom char * fileName, FSFILE * fo)
  Summary:
    Rename a file named with a ROM string on PIC18
  Conditions:
    File opened.
  Input:
    fileName -  The new name of the file (in ROM)
    fo -        The file to rename
  Return Values:
    0 -  File renamed successfully
    -1 - File could not be renamed
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The Fsrenamepgm function will copy the rom fileName specified
    by the user into a RAM array and pass that array into the
    FSrename function.
  Remarks:
    This function is for use with PIC18 when passing arguments in ROM.
  *****************************************************************/

int FSrenamepgm (const rom char * fileName, FSFILE * fo)
{
    char F[13];
    BYTE count;

    for (count = 0; count < 13; count++)
    {
        F[count] = *(fileName + count);
    }

    return FSrename (F, fo);
}
#endif

/******************************************************************************
  Function:
    FSFILE * FSfopenpgm(const rom char * fileName, const rom char *mode)
  Summary:
    Open a file named with a ROM string on PIC18
  Conditions:
    For read modes, file exists; FSInit performed
  Input:
    fileName -  The name of the file to be opened (ROM)
    mode -      The mode the file will be opened in (ROM)
  Return Values:
    FSFILE * - A pointer to the file object
    NULL -     File could not be opened
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FSfopenpgm function will copy a PIC18 ROM fileName and mode argument
    into RAM arrays, and then pass those arrays to the FSfopen function.
  Remarks:
    This function is for use with PIC18 when passing arguments in ROM.
  ******************************************************************************/


FSFILE * FSfopenpgm(const rom char * fileName, const rom char *mode)
{
    char F[13];
    char M[2];
    BYTE count;

    for (count = 0; count < 13; count++)
    {
        F[count] = *(fileName + count);
    }
    for (count = 0; count < 2; count++)
    {
        M[count] = *(mode + count);
    }

    return FSfopen(F, M);
}

/*************************************************************
  Function:
    int FSremovepgm (const rom char * fileName)
  Summary:
    Delete a file named with a ROM string on PIC18
  Conditions:
    File not opened; file exists
  Input:
    fileName -  The name of the file to be deleted (ROM)
  Return Values:
    0 -  File was removed successfully
    -1 - File could not be removed
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FSremovepgm function will copy a PIC18 ROM fileName argument
    into a RAM array, and then pass that array to the FSremove function.
  Remarks:
    This function is for use with PIC18 when passing arguments in ROM.
  *************************************************************/
#ifdef ALLOW_WRITES
int FSremovepgm (const rom char * fileName)
{
    char F[13];
    BYTE count;

    *fileName;
    for(count = 0; count < sizeof(F); count++)
    {
        _asm TBLRDPOSTINC _endasm
        F[count] = TABLAT;
    }//end for(...)

    return FSremove (F);
}
#endif

/**************************************************************************************
  Function:
    int FindFirstpgm (const char * fileName, unsigned int attr, SearchRec * rec)
  Summary:
    Find a file named with a ROM string on PIC18
  Conditions:
    None
  Input:
    fileName -  The name of the file to be found (ROM)
    attr -      The attributes of the file to be found
    rec -       Pointer to a search record to store the file info in
  Return Values:
    0 -  File was found
    -1 - No file matching the given parameters was found
  Side Effects:
    Search criteria from previous FindFirst call on passed SearchRec object will be lost.
    The FSerrno variable will be changed.
  Description:
    The FindFirstpgm function will copy a PIC18 ROM fileName argument
    into a RAM array, and then pass that array to the FindFirst function.
  Remarks:
    Call FindFirstpgm or FindFirst before calling FindNext.
    This function is for use with PIC18 when passing arguments in ROM.
  **************************************************************************************/
#ifdef ALLOW_FILESEARCH
int FindFirstpgm (const rom char * fileName, unsigned int attr, SearchRec * rec)
{
    char F[13];
    BYTE count;

    *fileName;
    for(count = 0; count < sizeof(F); count++)
    {
        _asm TBLRDPOSTINC _endasm
        F[count] = TABLAT;
    }//end for

    return FindFirst (F,attr,rec);
}
#endif
#endif
#endif


/***********************************************
  Function:
    DWORD ReadFAT (DISK *dsk, DWORD ccls)
  Summary:
    Read the next entry from the FAT
  Conditions:
    This function should not be called by the user.
  Input:
    dsk -   The disk structure
    ccls -  The current cluster
  Return:
    DWORD - The next cluster in a file chain
  Side Effects:
    None
  Description:
    The ReadFAT function will read the FAT and
    determine the next cluster value after the
    cluster specified by 'ccls.' Note that the
    FAT sector that is read is stored in the
    global FAT cache buffer.
  Remarks:
    None.
  ***********************************************/

DWORD ReadFAT (DISK *dsk, DWORD ccls)
{
    BYTE q;
    DWORD p, l;  // "l" is the sector Address
    DWORD c = 0, d, ClusterFailValue,LastClusterLimit;   // ClusterEntries

    gBufferZeroed = FALSE;

    /* Settings based on FAT type */
    switch (dsk->type)
    {
#ifdef SUPPORT_FAT32 // If FAT32 supported.
        case FAT32:
            p = (DWORD)ccls * 4;
            q = 0; // "q" not used for FAT32, only initialized to remove a warning
            ClusterFailValue = CLUSTER_FAIL_FAT32;
            LastClusterLimit = LAST_CLUSTER_FAT32;
            break;
#endif
        case FAT12:
            p = (DWORD) ccls *3;  // Mulby1.5 to find cluster pos in FAT
            q = p&1;
            p >>= 1;
            ClusterFailValue = CLUSTER_FAIL_FAT16;
            LastClusterLimit = LAST_CLUSTER_FAT12;
            break;
        case FAT16:
        default:
            p = (DWORD)ccls *2;     // Mulby 2 to find cluster pos in FAT
            q = 0; // "q" not used for FAT16, only initialized to remove a warning
            ClusterFailValue = CLUSTER_FAIL_FAT16;
            LastClusterLimit = LAST_CLUSTER_FAT16;
            break;
    }

    l = dsk->fat + (p / dsk->sectorSize);     //
    p &= dsk->sectorSize - 1;                 // Restrict 'p' within the FATbuffer size

    // Check if the appropriate FAT sector is already loaded
    if (gLastFATSectorRead == l)
    {
#ifdef SUPPORT_FAT32 // If FAT32 supported.
        if (dsk->type == FAT32)
            c = RAMreadD (gFATBuffer, p);
        else
#endif
            if(dsk->type == FAT16)
                c = RAMreadW (gFATBuffer, p);
            else if(dsk->type == FAT12)
            {
                c = RAMread (gFATBuffer, p);
                if (q)
                {
                    c >>= 4;
                }
                // Check if the MSB is across the sector boundry
                p = (p +1) & (dsk->sectorSize-1);
                if (p == 0)
                {
                    // Start by writing the sector we just worked on to the card
                    // if we need to
#ifdef ALLOW_WRITES
                    if (gNeedFATWrite)
                        if(WriteFAT (dsk, 0, 0, TRUE))
                            return ClusterFailValue;
#endif
                    if (!MDD_SectorRead (l+1, gFATBuffer))
                    {
                        gLastFATSectorRead = 0xFFFF;
                        return ClusterFailValue;
                    }
                    else
                    {
                        gLastFATSectorRead = l +1;
                    }
                }
                d = RAMread (gFATBuffer, p);
                if (q)
                {
                    c += (d <<4);
                }
                else
                {
                    c += ((d & 0x0F)<<8);
                }
            }
        }
        else
        {
            // If there's a currently open FAT sector,
            // write it back before reading into the buffer
#ifdef ALLOW_WRITES
            if (gNeedFATWrite)
            {
                if(WriteFAT (dsk, 0, 0, TRUE))
                    return ClusterFailValue;
            }
#endif
            if (!MDD_SectorRead (l, gFATBuffer))
            {
                gLastFATSectorRead = 0xFFFF;  // Note: It is Sector not Cluster.
                return ClusterFailValue;
            }
            else
            {
                gLastFATSectorRead = l;

#ifdef SUPPORT_FAT32 // If FAT32 supported.
                if (dsk->type == FAT32)
                    c = RAMreadD (gFATBuffer, p);
                else
#endif
                    if(dsk->type == FAT16)
                        c = RAMreadW (gFATBuffer, p);
                    else if (dsk->type == FAT12)
                    {
                        c = RAMread (gFATBuffer, p);
                        if (q)
                        {
                            c >>= 4;
                        }
                        p = (p +1) & (dsk->sectorSize-1);
                        d = RAMread (gFATBuffer, p);
                        if (q)
                        {
                            c += (d <<4);
                        }
                        else
                        {
                            c += ((d & 0x0F)<<8);
                        }
                    }
            }
    }

    // Normalize it so 0xFFFF is an error
    if (c >= LastClusterLimit)
        c = LastClusterLimit;

   return c;
}   // ReadFAT



/****************************************************************************
  Function:
    WORD WriteFAT (DISK *dsk, DWORD ccls, WORD value, BYTE forceWrite)
  Summary:
    Write an entry to the FAT
  Conditions:
    This function should not be called by the user.
  Input:
    dsk -         The disk structure
    ccls -        The current cluster
    value -       The value to write in
    forceWrite -  Force the function to write the current FAT sector
  Return:
    0 -    The FAT write was successful
    FAIL - The FAT could not be written
  Side Effects:
    None
  Description:
    The WriteFAT function writes an entry to the FAT.  If the function
    is called and the 'forceWrite' argument is TRUE, the function will
    write the existing FAT data to the device.  Otherwise, the function
    will replace a single entry in the FAT buffer (indicated by 'ccls')
    with a new value (indicated by 'value.')
  Remarks:
    None.
  ****************************************************************************/

#ifdef ALLOW_WRITES
DWORD WriteFAT (DISK *dsk, DWORD ccls, DWORD value, BYTE forceWrite)
{
    BYTE i, q, c;
    DWORD p, li, l, ClusterFailValue;

#ifdef SUPPORT_FAT32 // If FAT32 supported.
    if (dsk->type != FAT32 && dsk->type != FAT16 && dsk->type != FAT12)
        return CLUSTER_FAIL_FAT32;
#else // If FAT32 support not enabled
    if (dsk->type != FAT16 && dsk->type != FAT12)
        return CLUSTER_FAIL_FAT16;
#endif

    /* Settings based on FAT type */
    switch (dsk->type)
    {
#ifdef SUPPORT_FAT32 // If FAT32 supported.
        case FAT32:
            ClusterFailValue = CLUSTER_FAIL_FAT32;
            break;
#endif
        case FAT12:
        case FAT16:
        default:
            ClusterFailValue = CLUSTER_FAIL_FAT16;
            break;
    }

    gBufferZeroed = FALSE;

    // The only purpose for calling this function with forceWrite
    // is to write the current FAT sector to the card
    if (forceWrite)
    {
        for (i = 0, li = gLastFATSectorRead; i < dsk->fatcopy; i++, li += dsk->fatsize)
        {
            if (!MDD_SectorWrite (li, gFATBuffer, FALSE))
            {
                return ClusterFailValue;
            }
        }

        gNeedFATWrite = FALSE;

        return 0;
    }

    /* Settings based on FAT type */
    switch (dsk->type)
    {
#ifdef SUPPORT_FAT32 // If FAT32 supported.
        case FAT32:
            p = (DWORD)ccls *4;   // "p" is the position in "gFATBuffer" for corresponding cluster.
            q = 0;      // "q" not used for FAT32, only initialized to remove a warning
            break;
#endif
        case FAT12:
            p = (DWORD) ccls * 3; // "p" is the position in "gFATBuffer" for corresponding cluster.
            q = p & 1;   // Odd or even?
            p >>= 1;
            break;
        case FAT16:
        default:
            p = (DWORD) ccls *2;   // "p" is the position in "gFATBuffer" for corresponding cluster.
            q = 0;      // "q" not used for FAT16, only initialized to remove a warning
            break;
    }

    l = dsk->fat + (p / dsk->sectorSize);     //
    p &= dsk->sectorSize - 1;                 // Restrict 'p' within the FATbuffer size

    if (gLastFATSectorRead != l)
    {
        // If we are loading a new sector then write
        // the current one to the card if we need to
        if (gNeedFATWrite)
        {
            for (i = 0, li = gLastFATSectorRead; i < dsk->fatcopy; i++, li += dsk->fatsize)
            {
                if (!MDD_SectorWrite (li, gFATBuffer, FALSE))
                {
                    return ClusterFailValue;
                }
            }

            gNeedFATWrite = FALSE;
        }

        // Load the new sector
        if (!MDD_SectorRead (l, gFATBuffer))
        {
            gLastFATSectorRead = 0xFFFF;
            return ClusterFailValue;
        }
        else
        {
            gLastFATSectorRead = l;
        }
    }

#ifdef SUPPORT_FAT32 // If FAT32 supported.
    if (dsk->type == FAT32)  // Refer page 16 of FAT requirement.
    {
        RAMwrite (gFATBuffer, p,   ((value & 0x000000ff)));         // lsb,1st byte of cluster value
        RAMwrite (gFATBuffer, p+1, ((value & 0x0000ff00) >> 8));
        RAMwrite (gFATBuffer, p+2, ((value & 0x00ff0000) >> 16));
        RAMwrite (gFATBuffer, p+3, ((value & 0x0f000000) >> 24));   // the MSB nibble is supposed to be "0" in FAT32. So mask it.
    }
    else
    
#endif
    {
        if (dsk->type == FAT16)
        {
            RAMwrite (gFATBuffer, p, value);            //lsB
            RAMwrite (gFATBuffer, p+1, ((value&0x0000ff00) >> 8));    // msB
        }
        else if (dsk->type == FAT12)
        {
            // Get the current byte from the FAT
            c = RAMread (gFATBuffer, p);
            if (q)
            {
                c = ((value & 0x0F) << 4) | ( c & 0x0F);
            }
            else
            {
                c = (value & 0xFF);
            }
            // Write in those bits
            RAMwrite (gFATBuffer, p, c);

            // FAT12 entries can cross sector boundaries
            // Check if we need to load a new sector
            p = (p +1) & (dsk->sectorSize-1);
            if (p == 0)
            {
                // call this function to update the FAT on the card
                if (WriteFAT (dsk, 0,0,TRUE))
                    return ClusterFailValue;

                // Load the next sector
                if (!MDD_SectorRead (l +1, gFATBuffer))
                {
                    gLastFATSectorRead = 0xFFFF;
                    return ClusterFailValue;
                }
                else
                {
                    gLastFATSectorRead = l + 1;
                }
            }

            // Get the second byte of the table entry
            c = RAMread (gFATBuffer, p);
            if (q)
            {
                c = (value >> 4);
            }
            else
            {
                c = ((value >> 8) & 0x0F) | (c & 0xF0);
            }
            RAMwrite (gFATBuffer, p, c);
        }
    }
    gNeedFATWrite = TRUE;

    return 0;
}
#endif


#ifdef ALLOW_DIRS

// This string is used by dir functions to hold dir names temporarily
char defaultString [13];



/**************************************************************************
  Function:
    int FSchdir (char * path)
  Summary:
    Change the current working directory
  Conditions:
    None
  Input:
    path - The path of the directory to change to.
  Return Values:
    0 -   The current working directory was changed successfully
    EOF - The current working directory could not be changed
  Side Effects:
    The current working directory may be changed. The FSerrno variable will
    be changed.
  Description:
    The FSchdir function passes a RAM pointer to the path to the
    chdirhelper function.
  Remarks:
    None
  **************************************************************************/

int FSchdir (char * path)
{
    return chdirhelper (0, path, NULL);
}

/**************************************************************************
  Function:
    int FSchdirpgm (const rom char * path)
  Summary:
    Changed the CWD with a path in ROM on PIC18
  Conditions:
    None
  Input:
    path - The path of the directory to change to (ROM)
  Return Values:
    0 -   The current working directory was changed successfully
    EOF - The current working directory could not be changed
  Side Effects:
    The current working directory may be changed. The FSerrno variable will
    be changed.
  Description:
    The FSchdirpgm function passes a PIC18 ROM path pointer to the
    chdirhelper function.
  Remarks:
    This function is for use with PIC18 when passing arguments in ROM
  **************************************************************************/

#ifdef ALLOW_PGMFUNCTIONS
int FSchdirpgm (const rom char * path)
{
    return chdirhelper (1, NULL, path);
}
#endif


/*************************************************************************
  Function:
    // PIC24/30/33/32
    int chdirhelper (BYTE mode, char * ramptr, char * romptr);
    // PIC18
    int chdirhelper (BYTE mode, char * ramptr, const rom char * romptr);
  Summary:
    Helper function for FSchdir
  Conditions:
    None
  Input:
    mode -    Indicates which path pointer to use
    ramptr -  Pointer to the path specified in RAM
    romptr -  Pointer to the path specified in ROM
  Return Values:
    0 -   Directory was changed successfully.
    EOF - Directory could not be changed.
  Side Effects:
    The current working directory will be changed. The FSerrno variable
    will be changed. Any unwritten data in the data buffer will be written
    to the device.
  Description:
    This helper function is used by the FSchdir function. If the path
    argument is specified in ROM for PIC18 this function will be able to
    parse it correctly.  The function will loop through a switch statement
    to process the tokens in the path string.  Dot or dotdot entries are
    handled in the first case statement.  A backslash character is handled
    in the second case statement (note that this case statement will only
    be used if backslash is the first character in the path; backslash
    token delimiters will automatically be skipped after each token in the
    path is processed).  The third case statement will handle actual
    directory name strings.
  Remarks:
    None.
  *************************************************************************/

#ifdef ALLOW_PGMFUNCTIONS
int chdirhelper (BYTE mode, char * ramptr, const rom char * romptr)
#else
int chdirhelper (BYTE mode, char * ramptr, char * romptr)
#endif
{
    BYTE i, j;
    WORD curent = 1;
    DIRENTRY entry;
    char * temppath = ramptr;
#ifdef ALLOW_PGMFUNCTIONS
    rom char * temppath2 = romptr;
#endif
    FSFILE tempCWDobj2;
    char tempArray[12];
    FILEOBJ tempCWD = &tempCWDobj2;
    FileObjectCopy (tempCWD, cwdptr);

    FSerrno = CE_GOOD;

   // Check the first char of the path
#ifdef ALLOW_PGMFUNCTIONS
    if (mode)
        i = *temppath2;
    else
#endif
        i = *temppath;
    if (i == 0)
    {
        FSerrno = CE_INVALID_ARGUMENT;
        return -1;
    }

    while(1)
    {
        switch (i)
        {
            // First case: dot or dotdot entry
            case '.':
                // Move past the dot
#ifdef ALLOW_PGMFUNCTIONS
                if (mode)
                {
                    temppath2++;
                    i = *temppath2;
                }
                else
                {
#endif
                    temppath++;
                    i = *temppath;
#ifdef ALLOW_PGMFUNCTIONS
                }
#endif
                // Check if it's a dotdot entry
                if (i == '.')
                {
                    // Increment the path variable
#ifdef ALLOW_PGMFUNCTIONS
                    if (mode)
                    {
                        temppath2++;
                        i = *temppath2;
                    }
                    else
                    {
#endif
                        temppath++;
                        i = *temppath;
#ifdef ALLOW_PGMFUNCTIONS
                    }
#endif
                    // Check if we're in the root
                    if (tempCWD->dirclus == FatRootDirClusterValue)
                    {
                        // Fails if there's a dotdot chdir from the root
                        FSerrno = CE_INVALID_ARGUMENT;
                        return -1;
                    }
                    else
                    {
                        // Cache the dotdot entry
                        tempCWD->dirccls = tempCWD->dirclus;
                        curent = 1;
                        entry = Cache_File_Entry (tempCWD, &curent, TRUE);
                        if (entry == NULL)
                        {
                            FSerrno = CE_BADCACHEREAD;
                            return -1;
                        }

                        // Get the cluster
                        tempCWD->dirclus = GetFullClusterNumber(entry); // Get Complete Cluster number.
                        tempCWD->dirccls = tempCWD->dirclus;

                        // If we changed to root, record the name
                        if (tempCWD->dirclus == VALUE_DOTDOT_CLUSTER_VALUE_FOR_ROOT) // "0" is the value of Dotdot entry for Root in both FAT types.
                        {
                            tempCWD->name[0] = '\\';
                            for (j = 1; j < 11; j++)
                            {
                                tempCWD->name[j] = 0x20;
                            }

                            /* While moving to Root, get the Root cluster value */
                            tempCWD->dirccls = FatRootDirClusterValue;
                            tempCWD->dirclus = FatRootDirClusterValue;
                        }
                        else
                        {
                            // Otherwise set the name to ..
                            tempCWD->name[0] = '.';
                            tempCWD->name[1] = '.';
                            for (j = 2; j < 11; j++)
                            {
                                tempCWD->name[j] = 0x20;
                            }
                        }
                        // Cache the dot entry
                        curent = 0;
                        if (Cache_File_Entry(tempCWD, &curent, TRUE) == NULL)
                        {
                            FSerrno = CE_BADCACHEREAD;
                            return -1;
                        }
                        // Move past the next backslash, if necessary
                        while (i == '\\')
                        {
#ifdef ALLOW_PGMFUNCTIONS
                            if (mode)
                            {
                                temppath2++;
                                i = *temppath2;
                            }
                            else
                            {
#endif
                                temppath++;
                                i = *temppath;
#ifdef ALLOW_PGMFUNCTIONS
                            }
#endif
                        }
                        // Copy and return, if we're at the end
                        if (i == 0)
                        {
                            FileObjectCopy (cwdptr, tempCWD);
                            return 0;
                        }
                    }
                }
                else
                {
                    // If we ended with a . entry,
                    // just return what we have
                    if (i == 0)
                    {
                        FileObjectCopy (cwdptr, tempCWD);
                        return 0;
                    }
                    else
                    {
                        if (i == '\\')
                        {
                            while (i == '\\')
                            {
#ifdef ALLOW_PGMFUNCTIONS
                                if (mode)
                                {
                                    temppath2++;
                                    i = *temppath2;
                                }
                                else
                                {
#endif
                                    temppath++;
                                    i = *temppath;
#ifdef ALLOW_PGMFUNCTIONS
                                }
#endif
                            }
                            if (i == 0)
                            {
                                FileObjectCopy (cwdptr, tempCWD);
                                return 0;
                            }
                        }
                        else
                        {
                            // Anything else after a dot doesn't make sense
                            FSerrno = CE_INVALID_ARGUMENT;
                            return -1;
                        }
                    }
                }

                break;

            // Second case: the first char is the root backslash
            // We will ONLY switch to this case if the first char
            // of the path is a backslash
            case '\\':
            // Increment pointer to second char
#ifdef ALLOW_PGMFUNCTIONS
            if (mode)
            {
                temppath2++;
                i = *temppath2;
            }
            else
            {
#endif
                temppath++;
                i = *temppath;
#ifdef ALLOW_PGMFUNCTIONS
            }
#endif
            // Can't start the path with multiple backslashes
            if (i == '\\')
            {
                FSerrno = CE_INVALID_ARGUMENT;
                return -1;
            }

            if (i == 0)
            {
                // The user is changing directory to
                // the root
                cwdptr->dirclus = FatRootDirClusterValue;
                cwdptr->dirccls = FatRootDirClusterValue;
                cwdptr->name[0] = '\\';
                for (j = 1; j < 11; j++)
                {
                    cwdptr->name[j] = 0x20;
                }
                return 0;
            }
            else
            {
                // Our first char is the root dir switch
                tempCWD->dirclus = FatRootDirClusterValue;
                tempCWD->dirccls = FatRootDirClusterValue;
                tempCWD->name[0] = '\\';
                for (j = 1; j < 11; j++)
                {
                    tempCWD->name[j] = 0x20;
                }
            }
            break;

        default:
            // We should be at the beginning of a string of letters/numbers
            j = 0;
#ifdef ALLOW_PGMFUNCTIONS
            if (mode)
            {
                while ((i != 0) && (i != '\\') && (j < 12))
                {
                    defaultString[j++] = i;
                    i = *(++temppath2);
                }
            }
            else
            {
#endif
                while ((i != 0) && (i != '\\') && (j < 12))
                {
                    defaultString[j++] = i;
                    i = *(++temppath);
                }
#ifdef ALLOW_PGMFUNCTIONS
            }
#endif
            // We got a whole 12 chars
            // There could be more- truncate it
            if (j == 12)
            {
                while ((i != 0) && (i != '\\'))
                {
#ifdef ALLOW_PGMFUNCTIONS
                    if (mode)
                    {
                        i = *(++temppath2);
                    }
                    else
                    {
#endif
                        i = *(++temppath);
#ifdef ALLOW_PGMFUNCTIONS
                    }
#endif
                }
            }

            defaultString[j] = 0;

            if (FormatDirName (defaultString, 0) == FALSE)
                return -1;

            for (j = 0; j < 11; j++)
            {
                tempArray[j] = tempCWD->name[j];
                tempCWD->name[j] = defaultString[j];
            }

            // copy file object over
            FileObjectCopy(&gFileTemp, tempCWD);

            // See if the directory is there
            if(FILEfind (&gFileTemp, tempCWD, LOOK_FOR_MATCHING_ENTRY, 0) != CE_GOOD)
            {
                // Couldn't find the DIR
                FSerrno = CE_DIR_NOT_FOUND;
                return -1;
            }
            else
            {
                // Found the file
                // Check to make sure it's actually a directory
                if (gFileTemp.attributes != ATTR_DIRECTORY)
                {
                    FSerrno = CE_INVALID_ARGUMENT;
                    return -1;
                }

                // Get the new name
                for (j = 0; j < 11; j++)
                {
                    tempCWD->name[j] = gFileTemp.name[j];
                }
                tempCWD->dirclus = gFileTemp.cluster;
                tempCWD->dirccls = tempCWD->dirclus;
            }

            if (i == 0)
            {
                // If we're at the end of the string, we're done
                FileObjectCopy (cwdptr, tempCWD);
                return 0;
            }
            else
            {
                while (i == '\\')
                {
                    // If we get to another backslash, increment past it
#ifdef ALLOW_PGMFUNCTIONS
                    if (mode)
                    {
                        temppath2++;
                        i = *temppath2;
                    }
                    else
                    {
#endif
                        temppath++;
                        i = *temppath;
#ifdef ALLOW_PGMFUNCTIONS
                    }
#endif
                    if (i == 0)
                    {
                        FileObjectCopy (cwdptr, tempCWD);
                        return 0;
                    }
                }
            }
            break;
        }
    } // loop
}



// This string is used by FSgetcwd to return the cwd name if the path
// passed into the function is NULL
char defaultArray [10];


/**************************************************************
  Function:
    char * FSgetcwd (char * path, int numchars)
  Summary:
    Get the current working directory name
  Conditions:
    None
  Input:
    path -      Pointer to the array to return the cwd name in
    numchars -  Number of chars in the path
  Return Values:
    char * - The cwd name string pointer (path or defaultArray)
    NULL -   The current working directory name could not be loaded.
  Side Effects:
    The FSerrno variable will be changed
  Description:
    The FSgetcwd function will get the name of the current
    working directory and return it to the user.  The name
    will be copied into the buffer pointed to by 'path,'
    starting at the root directory and copying as many chars
    as possible before the end of the buffer.  The buffer
    size is indicated by the 'numchars' argument.  The first
    thing this function will do is load the name of the current
    working directory, if it isn't already present.  This could
    occur if the user switched to the dotdot entry of a
    subdirectory immediately before calling this function.  The
    function will then copy the current working directory name
    into the buffer backwards, and insert a backslash character.
    Next, the function will continuously switch to the previous
    directories and copy their names backwards into the buffer
    until it reaches the root.  If the buffer overflows, it
    will be treated as a circular buffer, and data will be
    copied over existing characters, starting at the beginning.
    Once the root directory is reached, the text in the buffer
    will be swapped, so that the buffer contains as much of the
    current working directory name as possible, starting at the
    root.
  Remarks:
    None
  **************************************************************/
char * FSgetcwd (char * path, int numchars)
{
    // If path is passed in as null, set up a default
    // array with 10 characters
    char totalchars = (path == NULL) ? 10 : numchars;
    char * returnPointer;
    char * bufferEnd;
    FILEOBJ tempCWD = &gFileTemp;
    BYTE bufferOverflow = FALSE;
    signed char j;
    DWORD curclus;
    WORD fHandle, tempindex;
    signed int i, index = 0;
    char aChar;
    DIRENTRY entry;

    FSerrno = CE_GOOD;

    // Set up the return value
    if (path == NULL)
        returnPointer = defaultArray;
    else
    {
        returnPointer = path;
        if (numchars == 0)
        {
            FSerrno = CE_INVALID_ARGUMENT;
            return NULL;
        }
    }

    bufferEnd = returnPointer + totalchars - 1;

    FileObjectCopy (tempCWD, cwdptr);

    if ((tempCWD->name[0] == '.') &&
        (tempCWD->name[1] == '.'))
    {
        // We last changed directory into a dotdot entry
        // Save the value of the current directory
        curclus = tempCWD->dirclus;
        // Put this dir's dotdot entry into the dirclus
        // Our cwd absolutely is not the root
        fHandle = 1;
        tempCWD->dirccls = tempCWD->dirclus;
        entry = Cache_File_Entry (tempCWD,&fHandle, TRUE);
        if (entry == NULL)
        {
            FSerrno = CE_BADCACHEREAD;
            return NULL;
        }


       // Get the cluster
       TempClusterCalc = GetFullClusterNumber(entry); // Get complete cluster number.

        // For FAT32, if the .. entry is 0, the cluster won't be 0
#ifdef SUPPORT_FAT32
        if (TempClusterCalc == VALUE_DOTDOT_CLUSTER_VALUE_FOR_ROOT)
        {
            tempCWD->dirclus = FatRootDirClusterValue;
        }
        else
#endif
            tempCWD->dirclus = TempClusterCalc;

        tempCWD->dirccls = tempCWD->dirclus;

        // Find the direntry for the entry we were just in
        fHandle = 0;
        entry = Cache_File_Entry (tempCWD, &fHandle, TRUE);
        if (entry == NULL)
        {
            FSerrno = CE_BADCACHEREAD;
            return NULL;
        }

        // Get the cluster
        TempClusterCalc = GetFullClusterNumber(entry); // Get complete cluster number.

        while ((TempClusterCalc != curclus) ||
            ((TempClusterCalc == curclus) &&
            (((unsigned char)entry->DIR_Name[0] == 0xE5) || (entry->DIR_Attr == ATTR_VOLUME) || (entry->DIR_Attr == ATTR_LONG_NAME))))
        {
            fHandle++;
            entry = Cache_File_Entry (tempCWD, &fHandle, FALSE);
            if (entry == NULL)
            {
                FSerrno = CE_BADCACHEREAD;
                return NULL;
            }

            // Get the cluster
            TempClusterCalc = GetFullClusterNumber(entry); // Get complete cluster number in a loop.
        }
        // We've found the entry for the dir we were in
        for (i = 0; i < 11; i++)
        {
            tempCWD->name[i] = entry->DIR_Name[i];
            cwdptr->name[i] = entry->DIR_Name[i];
        }
        // Reset our temp dir back to that cluster
        tempCWD->dirclus = curclus;
        tempCWD->dirccls = curclus;
        // This will set us at the cwd, but it will actually
        // have the name in the name field this time
    }
    // There's actually some kind of name value in the cwd
    if (tempCWD->name[0] == '\\')
    {
        // Easy, our CWD is the root
        *returnPointer = '\\';
        *(returnPointer + 1) = 0;
        return returnPointer;
    }
    else
    {
        // Loop until we get back to the root
        while (tempCWD->dirclus != FatRootDirClusterValue)
        {
            j = 10;
            while (tempCWD->name[j] == 0x20)
                j--;
            if (j >= 8)
            {
                while (j >= 8)
                {
                    *(returnPointer + index++) = tempCWD->name[j--];
                    // This is a circular buffer
                    // Any unnecessary values will be overwritten
                    if (index == totalchars)
                    {
                        index = 0;
                        bufferOverflow = TRUE;
                    }
                }
                *(returnPointer + index++) = '.';
                if (index == totalchars)
                {
                    index = 0;
                    bufferOverflow = TRUE;
                }
            }

            while (tempCWD->name[j] == 0x20)
                j--;

            while (j >= 0)
            {
                *(returnPointer + index++) = tempCWD->name[j--];
                // This is a circular buffer
                // Any unnecessary values will be overwritten
                if (index == totalchars)
                {
                    index = 0;
                    bufferOverflow = TRUE;
                }
            }

            // Put a backslash delimiter in front of the dir name
            *(returnPointer + index++) = '\\';
            if (index == totalchars)
            {
                index = 0;
                bufferOverflow = TRUE;
            }

            // Load the previous entry
            tempCWD->dirccls = tempCWD->dirclus;
            if (GetPreviousEntry (tempCWD))
            {
                FSerrno = CE_BAD_SECTOR_READ;
                return NULL;
            }
        }
    }

    // Point the index back at the last char in the string
    index--;

    i = 0;
    // Swap the chars in the buffer so they are in the right places
    if (bufferOverflow)
    {
        tempindex = index;
        // Swap the overflowed values in the buffer
        while ((index - i) > 0)
        {
             aChar = *(returnPointer + i);
             *(returnPointer + i) = * (returnPointer + index);
             *(returnPointer + index) = aChar;
             index--;
             i++;
        }

        // Point at the non-overflowed values
        i = tempindex + 1;
        index = bufferEnd - returnPointer;

        // Swap the non-overflowed values into the right places
        while ((index - i) > 0)
        {
             aChar = *(returnPointer + i);
             *(returnPointer + i) = * (returnPointer + index);
             *(returnPointer + index) = aChar;
             index--;
             i++;
        }
        // All the values should be in the right place now
        // Null-terminate the string
        *(bufferEnd) = 0;
    }
    else
    {
        // There was no overflow, just do one set of swaps
        tempindex = index;
        while ((index - i) > 0)
        {
            aChar = *(returnPointer + i);
            *(returnPointer + i) = * (returnPointer + index);
            *(returnPointer + index) = aChar;
            index--;
            i++;
        }
        *(returnPointer + tempindex + 1) = 0;
    }

    return returnPointer;
}


/**************************************************************************
  Function:
    void GetPreviousEntry (FSFILE * fo)
  Summary:
    Get the file entry info for the parent dir of the specified dir
  Conditions:
    Should not be called by the user.
  Input:
    fo -  The file to get the previous entry of
  Return Values:
    0 -  The previous entry was successfully retrieved
    -1 - The previous entry could not be retrieved
  Side Effects:
    None
  Description:
    The GetPreviousEntry function is used by the FSgetcwd function to
    load the previous (parent) directory.  This function will load the
    parent directory and then search through the file entries in that
    directory for one that matches the cluster number of the original
    directory.  When the matching entry is found, the name of the
    original directory is copied into the 'fo' FSFILE object.
  Remarks:
    None.
  **************************************************************************/

BYTE GetPreviousEntry (FSFILE * fo)
{
    BYTE i;
    WORD fHandle = 1;
    DWORD dirclus;
    DIRENTRY dirptr;

    // Load the previous entry
    dirptr = Cache_File_Entry (fo, &fHandle, TRUE);
    if (dirptr == NULL)
        return -1;

    // Get the cluster
    TempClusterCalc = GetFullClusterNumber(dirptr); // Get complete cluster number.

    if (TempClusterCalc == VALUE_DOTDOT_CLUSTER_VALUE_FOR_ROOT)
    {
        // The previous directory is the root
        fo->name[0] = '\\';
        for (i = 0; i < 11; i++)
        {
            fo->name[i] = 0x20;
        }
        fo->dirclus = FatRootDirClusterValue;
        fo->dirccls = FatRootDirClusterValue;
    }
    else
    {
        // Get the directory name
        // Save the previous cluster value
       // Get the cluster

        dirclus = TempClusterCalc;
        fo->dirclus = TempClusterCalc;
        fo->dirccls = TempClusterCalc;


        // Load the previous previous cluster
        dirptr = Cache_File_Entry (fo, &fHandle, TRUE);
        if (dirptr == NULL)
            return -1;

       // Get the cluster
        TempClusterCalc = GetFullClusterNumber(dirptr); // Get complete cluster number.
#ifdef SUPPORT_FAT32
        // If we're using FAT32 and the previous previous cluster is the root, the
        // value in the dotdot entry will be 0, but the actual cluster won't
        if (TempClusterCalc == VALUE_DOTDOT_CLUSTER_VALUE_FOR_ROOT)
        {
            fo->dirclus = FatRootDirClusterValue;
        }
        else
#endif
            fo->dirclus = TempClusterCalc;

        fo->dirccls = fo->dirclus;

        fHandle = 0;
        dirptr = Cache_File_Entry (fo, &fHandle, TRUE);
        if (dirptr == NULL)
            return -1;
        // Look through it until we get the name
        // of the previous cluster
        // Get the cluster
        TempClusterCalc = GetFullClusterNumber(dirptr); // Get complete cluster number.
        while ((TempClusterCalc != dirclus) ||
            ((TempClusterCalc == dirclus) &&
            (((unsigned char)dirptr->DIR_Name[0] == 0xE5) || (dirptr->DIR_Attr == ATTR_VOLUME) || (dirptr->DIR_Attr == ATTR_LONG_NAME))))
        {
            // Look through the entries until we get the
            // right one
            dirptr = Cache_File_Entry (fo, &fHandle, FALSE);
            if (dirptr == NULL)
                return -1;
            fHandle++;

           TempClusterCalc = GetFullClusterNumber(dirptr); // Get complete cluster number in a loop.
        }

        // The name should be in the entry now
        // Copy the actual directory location back
        for (i = 0; i < 11; i++)
        {
            fo->name[i] = dirptr->DIR_Name[i];
        }
        fo->dirclus = dirclus;
        fo->dirccls = dirclus;
    }
    return 0;
}


/**************************************************************************
  Function:
    int FSmkdir (char * path)
  Summary:
    Create a directory
  Conditions:
    None
  Input:
    path - The path of directories to create.
  Return Values:
    0 -   The specified directory was created successfully
    EOF - The specified directory could not be created
  Side Effects:
    Will create all non-existent directories in the path. The FSerrno
    variable will be changed.
  Description:
    The FSmkdir function passes a RAM pointer to the path to the
    mkdirhelper function.
  Remarks:
    None
  **************************************************************************/

#ifdef ALLOW_WRITES
int FSmkdir (char * path)
{
    return mkdirhelper (0, path, NULL);
}

/**************************************************************************
  Function:
    int FSmkdirpgm (const rom char * path)
  Summary:
    Create a directory with a path in ROM on PIC18
  Conditions:
    None
  Input:
    path - The path of directories to create (ROM)
  Return Values:
    0 -   The specified directory was created successfully
    EOF - The specified directory could not be created
  Side Effects:
    Will create all non-existent directories in the path. The FSerrno
    variable will be changed.
  Description:
    The FSmkdirpgm function passes a PIC18 ROM path pointer to the
    mkdirhelper function.
  Remarks:
    This function is for use with PIC18 when passing arugments in ROM
  **************************************************************************/

#ifdef ALLOW_PGMFUNCTIONS
int FSmkdirpgm (const rom char * path)
{
    return mkdirhelper (1, NULL, path);
}
#endif


/*************************************************************************
  Function:
    // PIC24/30/33/32
    int mkdirhelper (BYTE mode, char * ramptr, char * romptr)
    // PIC18
    int mkdirhelper (BYTE mode, char * ramptr, const rom char * romptr)
  Summary:
    Helper function for FSmkdir
  Conditions:
    None
  Input:
    mode -   Indicates which path pointer to use
    ramptr - Pointer to the path specified in RAM
    romptr - Pointer to the path specified in ROM
  Return Values:
    0 -  Directory was created
    -1 - Directory could not be created
  Side Effects:
    Will create all non-existant directories in the path.
    The FSerrno variable will be changed.
  Description:
    This helper function is used by the FSchdir function. If the path
    argument is specified in ROM for PIC18 this function will be able
    to parse it correctly.  This function will first scan through the path
    to ensure that any DIR names don't exceed 11 characters.  It will then
    backup the current working directory and begin changing directories
    through the path until it reaches a directory than can't be changed to.
    It will then create the specified directory and change directories to
    the new directory. The function will continue creating and changing to
    directories until the end of the path is reached.  The function will
    then restore the original current working directory.
  Remarks:
    None
  **************************************************************************/

#ifdef ALLOW_PGMFUNCTIONS
int mkdirhelper (BYTE mode, char * ramptr, const rom char * romptr)
#else
int mkdirhelper (BYTE mode, char * ramptr, char * romptr)
#endif
{
    BYTE i, j;
    char * temppath = ramptr;
#ifdef ALLOW_PGMFUNCTIONS
    rom char * temppath2 = romptr;
#endif
    char tempArray[13];
    FILEOBJ tempCWD = &tempCWDobj;

#ifdef __18CXX
    char dotdotPath[] = "..";
#endif

    FSerrno = CE_GOOD;

    if (MDD_WriteProtectState())
    {
        FSerrno = CE_WRITE_PROTECTED;
        return (-1);
    }

#ifdef ALLOW_PGMFUNCTIONS
    if (mode == 1)
    {
        // Scan for too-long file names
        while (1)
        {
            i = 0;
            while((*temppath2 != 0) && (*temppath2 != '.')&& (*temppath2 != '\\'))
            {
                temppath2++;
                i++;
            }
            if (i > 8)
            {
                FSerrno = CE_INVALID_ARGUMENT;
                return -1;
            }
            if (*temppath2 == '.')
            {
                temppath2++;
                i = 0;
                while ((*temppath2 != 0) && (*temppath2 != '\\'))
                {
                    temppath2++;
                    i++;
                }
                if (i > 3)
                {
                    FSerrno = CE_INVALID_ARGUMENT;
                    return -1;
                }
            }
            while (*temppath2 == '\\')
                temppath2++;
            if (*temppath2 == 0)
                break;
        }
    }
    else
#endif
        // Scan for too-long file names
        while (1)
        {
            i = 0;
            while((*temppath != 0) && (*temppath != '.')&& (*temppath != '\\'))
            {
                temppath++;
                i++;
            }
            if (i > 8)
            {
                FSerrno = CE_INVALID_ARGUMENT;
                return -1;
            }
            if (*temppath == '.')
            {
                temppath++;
                i = 0;
                while ((*temppath != 0) && (*temppath != '\\'))
                {
                    temppath++;
                    i++;
                }
                if (i > 3)
                {
                    FSerrno = CE_INVALID_ARGUMENT;
                    return -1;
                }
            }
            while (*temppath == '\\')
                temppath++;
            if (*temppath == 0)
                break;
        }

    temppath = ramptr;
#ifdef ALLOW_PGMFUNCTIONS
    temppath2 = romptr;
#endif

    // We're going to be moving the CWD
    // Back up the CWD
    FileObjectCopy (tempCWD, cwdptr);

    // get to the target directory
    while (1)
    {
#ifdef ALLOW_PGMFUNCTIONS
        if (mode == 1)
            i = *temppath2;
        else
#endif
            i = *temppath;

        if (i == '.')
        {
#ifdef ALLOW_PGMFUNCTIONS
            if (mode == 1)
            {
                temppath2++;
                i = *temppath2;
            }
            else
            {
#endif
                temppath++;
                i = *temppath;
#ifdef ALLOW_PGMFUNCTIONS
            }
#endif

            if ((i != '.') && (i != 0) && (i != '\\'))
            {
                FSerrno = CE_INVALID_ARGUMENT;
                return -1;
            }

            if (i == '.')
            {
                if (cwdptr->dirclus ==  FatRootDirClusterValue)
                {
                    // If we try to change to the .. from the
                    // root, operation fails
                    FSerrno = CE_INVALID_ARGUMENT;
                    return -1;
                }
#ifdef ALLOW_PGMFUNCTIONS
                if (mode == 1)
                {
                    temppath2++;
                    i = *temppath2;
                }
                else
                {
#endif
                    temppath++;
                    i = *temppath;
#ifdef ALLOW_PGMFUNCTIONS
                }
#endif
                if ((i != '\\') && (i != 0))
                {
                    FSerrno = CE_INVALID_ARGUMENT;
                    return -1;
                }
// dotdot entry
#ifndef __18CXX
                FSchdir ("..");
#else
                FSchdir (dotdotPath);
#endif
            }
            // Skip past any backslashes
            while (i == '\\')
            {
#ifdef ALLOW_PGMFUNCTIONS
                if (mode == 1)
                {
                    temppath2++;
                    i = *temppath2;
                }
                else
                {
#endif
                    temppath++;
                    i = *temppath;
#ifdef ALLOW_PGMFUNCTIONS
                }
#endif
            }
            if (i == 0)
            {
                // No point in creating a dot or dotdot entry directly
                FileObjectCopy (cwdptr, tempCWD);
                FSerrno = CE_INVALID_ARGUMENT;
                return -1;
            }
        }
        else
        {
            if (i == '\\')
            {
                // Start at the root
                cwdptr->dirclus = FatRootDirClusterValue;
                cwdptr->dirccls = FatRootDirClusterValue;
                cwdptr->name[0] = '\\';
                for (i = 1; i < 11; i++)
                {
                    cwdptr->name[i] = 0x20;
                }

#ifdef ALLOW_PGMFUNCTIONS
                if (mode == 1)
                {
                    temppath2++;
                    i = *temppath2;
                }
                else
                {
#endif
                    temppath++;
                    i = *temppath;
#ifdef ALLOW_PGMFUNCTIONS
                }
#endif
                // If we just got two backslashes in a row at the
                // beginning of the path, the function fails
                if (i == '\\')
                {
                    FileObjectCopy (cwdptr, tempCWD);
                    FSerrno = CE_INVALID_ARGUMENT;
                    return -1;
                }
                if (i == 0)
                {
                    // We can't make the root dir
                    FileObjectCopy (cwdptr, tempCWD);
                    FSerrno = CE_INVALID_ARGUMENT;
                    return -1;
                }
            }
            else
            {
                break;
            }
        }
    }

    tempArray[12] = 0;
    while (1)
    {
        while(1)
        {
#ifdef ALLOW_PGMFUNCTIONS
            if (mode == 1)
            {
                // Change directories as specified
                i = *temppath2;
                j = 0;
                // Parse the next token
                while ((i != 0) && (i != '\\') && (j < 12))
                {
                    tempArray[j++] = i;
                    temppath2++;
                    i = *temppath2;
                }
            }
            else
            {
#endif
                // Change directories as specified
                i = *temppath;
                j = 0;
                // Parse the next token
                while ((i != 0) && (i != '\\') && (j < 12))
                {
                    tempArray[j++] = i;
                    temppath++;
                    i = *temppath;
                }
#ifdef ALLOW_PGMFUNCTIONS
            }
#endif
            tempArray[j] = 0;

            if (tempArray[0] == '.')
            {
                if ((tempArray[1] != 0) && (tempArray[1] != '.'))
                {
                    FileObjectCopy (cwdptr, tempCWD);
                    FSerrno = CE_INVALID_ARGUMENT;
                    return -1;
                }
                if ((tempArray[1] == '.') && (tempArray[2] != 0))
                {
                    FileObjectCopy (cwdptr, tempCWD);
                    FSerrno = CE_INVALID_ARGUMENT;
                    return -1;
                }
            }

            // Try to change to it
            // If you can't we need to create it
            if (FSchdir (tempArray))
            {
                break;
            }
            else
            {
                // We changed into the directory
                while (i == '\\')
                {
                    // Next char is a backslash
                    // Move past it
#ifdef ALLOW_PGMFUNCTIONS
                    if (mode == 1)
                    {
                        temppath2++;
                        i = *temppath2;
                    }
                    else
                    {
#endif
                        temppath++;
                        i = *temppath;
#ifdef ALLOW_PGMFUNCTIONS
                    }
#endif
                }
                // If it's the last one, return success
                if (i == 0)
                {
                    FileObjectCopy (cwdptr, tempCWD);
                    return 0;
                }
            }
        }

        // Create a dir here
        if (!CreateDIR (tempArray))
        {
            FileObjectCopy (cwdptr, tempCWD);
            return -1;
        }

        // Try to change to that directory
        if (FSchdir (tempArray))
        {
            FileObjectCopy (cwdptr, tempCWD);
            FSerrno = CE_BADCACHEREAD;
            return -1;
        }

#ifdef ALLOW_PGMFUNCTIONS
        if (mode == 1)
        {
            // Check for another backslash
            while (*temppath2 == '\\')
            {
                temppath2++;
                i = *temppath2;
            }
        }
        else
        {
#endif
            while (*temppath == '\\')
            {
                temppath++;
                i = *temppath;
            }
#ifdef ALLOW_PGMFUNCTIONS
        }
#endif

        // Check to see if we're at the end of the path string
        if (i == 0)
        {
            // We already have one
            FileObjectCopy (cwdptr, tempCWD);
            return 0;
        }
    }
}


/**************************************************************************
  Function:
    int CreateDIR (char * path)
  Summary:
    FSmkdir helper function to create a directory
  Conditions:
    This function should not be called by the user.
  Input:
    path -  The name of the dir to create
  Return Values:
    TRUE -  Directory was created successfully
    FALSE - Directory could not be created.
  Side Effects:
    Any unwritten data in the data buffer or the FAT buffer will be written
    to the device.
  Description:
    The CreateDIR function is a helper function for the mkdirhelper
    function.  The CreateDIR function will create a new file entry for
    a directory and assign a cluster to it.  It will erase the cluster
    and write a dot and dotdot entry to it.
  Remarks:
    None.
  **************************************************************************/

int CreateDIR (char * path)
{
    FSFILE * dirEntryPtr = &gFileTemp;
    DIRENTRY dir;
    WORD handle = 0;
    DWORD dot, dotdot;
    BYTE i;

    for (i = 0; i < 12; i++)
    {
        defaultString[i] = *(path + i);
    }

    if (FormatDirName(defaultString, 0) == FALSE)
    {
        FSerrno = CE_INVALID_FILENAME;
        return FALSE;
    }

    // Copy name into file object
    for (i = 0; i < 11; i++)
    {
        dirEntryPtr->name[i] = defaultString[i];
    }

    dirEntryPtr->dirclus = cwdptr->dirclus;
    dirEntryPtr->dirccls = cwdptr->dirccls;
    dirEntryPtr->cluster = 0;
    dirEntryPtr->ccls = 0;
    dirEntryPtr->dsk = cwdptr->dsk;

    // Create a directory entry
    if(CreateFileEntry(dirEntryPtr, &handle, DIRECTORY) != CE_GOOD)
    {
        return FALSE;
    }
    else
    {
        if (gNeedFATWrite)
            if(WriteFAT (dirEntryPtr->dsk, 0, 0, TRUE))
            {
                FSerrno = CE_WRITE_ERROR;
                return FALSE;
            }
        // Zero that cluster
        if (dirEntryPtr->dirclus == FatRootDirClusterValue)
            dotdot = 0;
        else
            dotdot = dirEntryPtr->dirclus;
        dirEntryPtr->dirccls = dirEntryPtr->dirclus;
        dir = Cache_File_Entry(dirEntryPtr, &handle, TRUE);
        if (dir == NULL)
        {
            FSerrno = CE_BADCACHEREAD;
            return FALSE;
        }

        // Get the cluster
        dot = GetFullClusterNumber(dir); // Get complete cluster number.

        if (writeDotEntries (dirEntryPtr->dsk, dot, dotdot))
            return TRUE;
        else
            return FALSE;

    }
}


/***********************************************************************************
  Function:
    BYTE writeDotEntries (DISK * disk, DWORD dotAddress, DWORD dotdotAddress)
  Summary:
    Create dot and dotdot entries in a non-root directory
  Conditions:
    This function should not be called by the user.
  Input:
    disk -           The global disk structure
    dotAddress -     The cluster the current dir is in
    dotdotAddress -  The cluster the previous directory was in
  Return Values:
    TRUE -  The dot and dotdot entries were created
    FALSE - The dot and dotdot entries could not be created in the new directory
  Side Effects:
    None
  Description:
    The writeDotEntries function will create and write dot and dotdot entries
    to a newly created directory.
  Remarks:
    None.
  ***********************************************************************************/

BYTE writeDotEntries (DISK * disk, DWORD dotAddress, DWORD dotdotAddress)
{
    WORD i;
    WORD size;
    _DIRENTRY entry;
    DIRENTRY entryptr = &entry;
    DWORD sector;

    gBufferOwner = NULL;

    size = sizeof (_DIRENTRY);

    memset(disk->buffer, 0x00, MEDIA_SECTOR_SIZE);

    entry.DIR_Name[0] = '.';

    for (i = 1; i < 11; i++)
    {
        entry.DIR_Name[i] = 0x20;
    }
    entry.DIR_Attr = ATTR_DIRECTORY;
    entry.DIR_NTRes = 0x00;

    entry.DIR_FstClusLO = (WORD)(dotAddress & 0x0000FFFF); // Lower 16 bit address

#ifdef SUPPORT_FAT32 // If FAT32 supported.
    entry.DIR_FstClusHI = (WORD)((dotAddress & 0x0FFF0000)>> 16); // Higher 16 bit address. FAT32 uses only 28 bits. Mask even higher nibble also.
#else // If FAT32 support not enabled
    entry.DIR_FstClusHI = 0;
#endif

    entry.DIR_FileSize = 0x00;

// Times need to be the same as the times in the directory entry

// Set dir date for uncontrolled clock source
#ifdef INCREMENTTIMESTAMP
    entry.DIR_CrtTimeTenth = 0xB2;
    entry.DIR_CrtTime = 0x7278;
    entry.DIR_CrtDate = 0x32B0;
    entry.DIR_LstAccDate = 0x0000;
    entry.DIR_WrtTime = 0x0000;
    entry.DIR_WrtDate = 0x0000;
#endif

#ifdef USEREALTIMECLOCK
    entry.DIR_CrtTimeTenth = gTimeCrtMS;         // millisecond stamp
    entry.DIR_CrtTime =      gTimeCrtTime;      // time created //
    entry.DIR_CrtDate =      gTimeCrtDate;      // date created (1/1/2004)
    entry.DIR_LstAccDate =   0x0000;         // Last Access date
    entry.DIR_WrtTime =      0x0000;         // last update time
    entry.DIR_WrtDate =      0x0000;         // last update date
#endif

#ifdef USERDEFINEDCLOCK
    entry.DIR_CrtTimeTenth  =   gTimeCrtMS;         // millisecond stamp
    entry.DIR_CrtTime       =   gTimeCrtTime;       // time created //
    entry.DIR_CrtDate       =   gTimeCrtDate;       // date created (1/1/2004)
    entry.DIR_LstAccDate    =   0x0000;             // Last Access date
    entry.DIR_WrtTime       =   0x0000;             // last update time
    entry.DIR_WrtDate       =   0x0000;             // last update date
#endif

    for (i = 0; i < size; i++)
    {
        *(disk->buffer + i) = *((char *)entryptr + i);
    }
    entry.DIR_Name[1] = '.';

    entry.DIR_FstClusLO = (WORD)(dotdotAddress & 0x0000FFFF); // Lower 16 bit address

#ifdef SUPPORT_FAT32 // If FAT32 supported.
    entry.DIR_FstClusHI = (WORD)((dotdotAddress & 0x0FFF0000)>> 16); // Higher 16 bit address. FAT32 uses only 28 bits. Mask even higher nibble also.
#else // If FAT32 support not enabled
    entry.DIR_FstClusHI = 0;
#endif


    for (i = 0; i < size; i++)
    {
        *(disk->buffer + i + size) = *((char *)entryptr + i);
    }

    sector = Cluster2Sector (disk, dotAddress);

    if (MDD_SectorWrite(sector, disk->buffer, FALSE) == FALSE)
    {
        FSerrno = CE_WRITE_ERROR;
        return FALSE;
    }

    return TRUE;
}

// This array is used to prevent a stack frame error
#ifdef __18CXX
    char tempArray[13] = "           ";
#endif


/**************************************************************************
  Function:
    int FSrmdir (char * path)
  Summary:
    Delete a directory
  Conditions:
    None
  Input:
    path -      The path of the directory to remove
    rmsubdirs -
              - TRUE -  All sub-dirs and files in the target dir will be removed
              - FALSE - FSrmdir will not remove non-empty directories
  Return Values:
    0 -   The specified directory was deleted successfully
    EOF - The specified directory could not be deleted
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FSrmdir function passes a RAM pointer to the path to the
    rmdirhelper function.
  Remarks:
    None.
  **************************************************************************/

int FSrmdir (char * path, unsigned char rmsubdirs)
{
    return rmdirhelper (0, path, NULL, rmsubdirs);
}

/**************************************************************************
  Function:
    int FSrmdirpgm (const rom char * path)
  Summary:
    Delete a directory with a path in ROM on PIC18
  Conditions:
    None.
  Input:
    path -      The path of the directory to remove (ROM)
    rmsubdirs -
              - TRUE -  All sub-dirs and files in the target dir will be removed
              - FALSE - FSrmdir will not remove non-empty directories
  Return Values:
    0 -   The specified directory was deleted successfully
    EOF - The specified directory could not be deleted
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FSrmdirpgm function passes a PIC18 ROM path pointer to the
    rmdirhelper function.
  Remarks:
    This function is for use with PIC18 when passing arguments in ROM.
  **************************************************************************/

#ifdef ALLOW_PGMFUNCTIONS
int FSrmdirpgm (const rom char * path, unsigned char rmsubdirs)
{
    return rmdirhelper (1, NULL, path, rmsubdirs);
}
#endif

/************************************************************************************************
  Function:
    // PIC24/30/33/32
    int rmdirhelper (BYTE mode, char * ramptr, char * romptr, unsigned char rmsubdirs)
    // PIC18
    int rmdirhelper (BYTE mode, char * ramptr, const rom char * romptr, unsigned char rmsubdirs)
  Summary:
    Helper function for FSrmdir
  Conditions:
    This function should not be called by the user.
  Input:
    path -      The path of the dir to delete
    rmsubdirs -
              - TRUE -  Remove all sub-directories and files in the directory
              - FALSE - Non-empty directories can not be removed
  Return Values:
    0 -   The specified directory was successfully removed.
    EOF - The specified directory could not be removed.
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    This helper function is used by the FSmkdir function.  If the path
    argument is specified in ROM for PIC18 this function will be able
    to parse it correctly.  This function will first change to the
    specified directory.  If the rmsubdirs argument is FALSE the function
    will search through the directory to ensure that it is empty and then
    remove it.  If the rmsubdirs argument is TRUE the function will also
    search through the directory for subdirectories or files.  When the
    function finds a file, the file will be erased.  When the function
    finds a subdirectory, it will switch to the subdirectory and begin
    removing all of the files in that subdirectory.  Once the subdirectory
    is empty, the function will switch back to the original directory.
    return to the original position in that directory, and continue removing
    files.  Once the specified directory is empty, the function will
    change to the parent directory, search through it for the directory
    to remove, and then erase that directory.
  Remarks:
    None.
  ************************************************************************************************/

#ifdef ALLOW_PGMFUNCTIONS
int rmdirhelper (BYTE mode, char * ramptr, const rom char * romptr, unsigned char rmsubdirs)
#else
int rmdirhelper (BYTE mode, char * ramptr, char * romptr, unsigned char rmsubdirs)
#endif
{
    FILEOBJ tempCWD = &tempCWDobj;
    FILEOBJ fo = &gFileTemp;
    DIRENTRY entry;
    WORD handle = 0;
    WORD handle2;
    WORD subDirDepth;
    BYTE Index, Index2;

#ifndef __18CXX
    char tempArray[13] = "           ";
#else
    char dotdotname[] = "..";
#endif

    FSerrno = CE_GOOD;

    // Back up the current working directory
    FileObjectCopy (tempCWD, cwdptr);

#ifdef ALLOW_PGMFUNCTIONS
    if (mode)
    {
        if (chdirhelper (1, NULL, romptr))
        {
            FSerrno = CE_DIR_NOT_FOUND;
            return -1;
        }
    }
    else
    {
#endif
        if (FSchdir (ramptr))
        {
            FSerrno = CE_DIR_NOT_FOUND;
            return -1;
        }
#ifdef ALLOW_PGMFUNCTIONS
    }
#endif

    // Make sure we aren't trying to remove the root dir or the CWD
    if ((cwdptr->dirclus == FatRootDirClusterValue) || (cwdptr->dirclus == tempCWD->dirclus))
    {
        FileObjectCopy (cwdptr, tempCWD);
        FSerrno = CE_INVALID_ARGUMENT;
        return -1;
    }

    handle++;
    entry = Cache_File_Entry (cwdptr, &handle, TRUE);

    if (entry == NULL)
    {
        FileObjectCopy (cwdptr, tempCWD);
        FSerrno = CE_BADCACHEREAD;
        return -1;
    }

    handle++;
    entry = Cache_File_Entry (cwdptr, &handle, FALSE);
    if (entry == NULL)
    {
        FileObjectCopy (cwdptr, tempCWD);
        FSerrno = CE_BADCACHEREAD;
        return -1;
    }
    // Don't remove subdirectories and sub-files
    if (!rmsubdirs)
    {
        while (entry->DIR_Name[0] != 0)
        {
            if ((unsigned char)entry->DIR_Name[0] != 0xE5)
            {
                FileObjectCopy (cwdptr, tempCWD);
                FSerrno = CE_DIR_NOT_EMPTY;
                return -1;
            }
            handle++;
            entry = Cache_File_Entry (cwdptr, &handle, FALSE);
            if ((entry == NULL))
            {
                FileObjectCopy (cwdptr, tempCWD);
                FSerrno = CE_BADCACHEREAD;
                return -1;
            }
        }
    }
    else
    {
        // Do remove subdirectories and sub-files
        dirCleared = FALSE;
        subDirDepth = 0;

        while (!dirCleared)
        {
            if (entry->DIR_Name[0] != 0)
            {
                if (((unsigned char)entry->DIR_Name[0] != 0xE5) && (entry->DIR_Attr != ATTR_VOLUME) && (entry->DIR_Attr != ATTR_LONG_NAME))
                {
                    if ((entry->DIR_Attr & ATTR_DIRECTORY) == ATTR_DIRECTORY)
                    {
                        // We have a directory
                        subDirDepth++;
                        for (Index = 0; (Index < DIR_NAMESIZE) && (entry->DIR_Name[Index] != 0x20); Index++)
                        {
                            tempArray[Index] = entry->DIR_Name[Index];
                        }
                        if (entry->DIR_Name[8] != 0x20)
                        {
                            tempArray[Index++] = '.';
                            for (Index2 = 0; (Index2 < DIR_EXTENSION) && (entry->DIR_Name[Index2 + DIR_NAMESIZE] != 0x20); Index2++)
                            {
                                tempArray[Index++] = entry->DIR_Name[Index2 + DIR_NAMESIZE];
                            }
                        }
                        tempArray[Index] = 0;
                        // Change to the subdirectory
                        if (FSchdir (tempArray))
                        {
                            FileObjectCopy (cwdptr, tempCWD);
                            FSerrno = CE_DIR_NOT_FOUND;
                            return -1;
                        }
                        else
                        {
                            // Make sure we're not trying to delete the CWD
                            if (cwdptr->dirclus == tempCWD->dirclus)
                            {
                                FileObjectCopy (cwdptr, tempCWD);
                                FSerrno = CE_INVALID_ARGUMENT;
                                return -1;
                            }
                        }
                        handle = 2;
                        recache = TRUE;
                    }
                    else
                    {
                        memset (tempArray, 0x00, 12);
                        for (Index = 0; Index < 11; Index++)
                        {
                            fo->name[Index] = entry->DIR_Name[Index];
                        }

                        fo->dsk = &gDiskData;

                        fo->entry = handle;
                        fo->dirclus = cwdptr->dirclus;
                        fo->dirccls = cwdptr->dirccls;
                        fo->cluster = 0;
                        fo->ccls    = 0;

                        if (FILEerase(fo, &handle, TRUE))
                        {
                            FileObjectCopy (cwdptr, tempCWD);
                            FSerrno = CE_ERASE_FAIL;
                            return -1;
                        }
                        else
                        {
                            handle++;
                        }
                    } // Check to see if it's a DIR entry
                }// Check non-dir entry to see if its a valid file
                else
                {
                    handle++;
                }
                if (recache)
                {
                    recache = FALSE;
                    cwdptr->dirccls = cwdptr->dirclus;
                    entry = Cache_File_Entry (cwdptr, &handle, TRUE);
                }
                else
                {
                    entry = Cache_File_Entry (cwdptr, &handle, FALSE);
                }
                if (entry == NULL)
                {
                    FileObjectCopy (cwdptr, tempCWD);
                    FSerrno = CE_BADCACHEREAD;
                    return -1;
                }
            }
            else
            {
                // We have reached the end of the directory
                if (subDirDepth != 0)
                {
                    handle2 = 0;

                    cwdptr->dirccls = cwdptr->dirclus;
                    entry = Cache_File_Entry (cwdptr, &handle2, TRUE);
                    if (entry == NULL)
                    {
                        FileObjectCopy (cwdptr, tempCWD);
                        FSerrno = CE_BADCACHEREAD;
                        return -1;
                    }

                    // Get the cluster
                    handle2 = GetFullClusterNumber(entry); // Get complete cluster number.

#ifndef __18CXX
                    if (FSchdir (".."))
#else
                    if (FSchdir (dotdotname))
#endif
                    {
                        FileObjectCopy (cwdptr, tempCWD);
                        FSerrno = CE_DIR_NOT_FOUND;
                        return -1;
                    }
                    // Return to our previous position in this directory
                    handle = 2;
                    cwdptr->dirccls = cwdptr->dirclus;
                    entry = Cache_File_Entry (cwdptr, &handle, TRUE);
                    if (entry == NULL)
                    {
                        FileObjectCopy (cwdptr, tempCWD);
                        FSerrno = CE_BADCACHEREAD;
                        return -1;
                    }

                    // Get the cluster
                    TempClusterCalc = GetFullClusterNumber(entry); // Get complete cluster number.

                    while ((TempClusterCalc != handle2) ||
                    ((TempClusterCalc == handle2) &&
                    (((unsigned char)entry->DIR_Name[0] == 0xE5) || (entry->DIR_Attr == ATTR_VOLUME))))
                    {
                        handle++;
                        entry = Cache_File_Entry (cwdptr, &handle, FALSE);
                        if (entry == NULL)
                        {
                            FileObjectCopy (cwdptr, tempCWD);
                            FSerrno = CE_BADCACHEREAD;
                            return -1;
                        }
                        // Get the cluster
                        TempClusterCalc = GetFullClusterNumber(entry); // Get complete cluster number in a loop.
                    }
                    // Erase the directory that we just cleared the subdirectories out of
                    memset (tempArray, 0x00, 12);
                    for (Index = 0; Index < 11; Index++)
                    {
                        tempArray[Index] = entry->DIR_Name[Index];
                    }
                    if (eraseDir (tempArray))
                    {
                        FileObjectCopy (cwdptr, tempCWD);
                        FSerrno = CE_ERASE_FAIL;
                        return -1;
                    }
                    else
                    {
                        handle++;
                        cwdptr->dirccls = cwdptr->dirclus;
                        entry = Cache_File_Entry (cwdptr, &handle, TRUE);
                        if (entry == NULL)
                        {
                            FileObjectCopy (cwdptr, tempCWD);
                            FSerrno = CE_BADCACHEREAD;
                            return -1;
                        }
                    }

                    // Decrease the subdirectory depth
                    subDirDepth--;
                }
                else
                {
                    dirCleared = TRUE;
                } // Check subdirectory depth
            } // Check until we get an empty entry
        } // Loop until the whole dir is cleared
    }

    // Cache the current directory name
    // tempArray is used so we don't disturb the
    // global getcwd buffer
    if (FSgetcwd (tempArray, 12) == NULL)
    {
        FileObjectCopy (cwdptr, tempCWD);
        return -1;
    }

    memset(tempArray, 0x00, 12);

    for (Index = 0; Index < 11; Index++)
    {
        tempArray[Index] = cwdptr->name[Index];
    }

    // If we're here, this directory is empty
#ifndef __18CXX
    if (FSchdir (".."))
#else
    if (FSchdir (dotdotname))
#endif
    {
        FileObjectCopy (cwdptr, tempCWD);
        FSerrno = CE_DIR_NOT_FOUND;
        return -1;
    }

    if (eraseDir (tempArray))
    {
        FileObjectCopy (cwdptr, tempCWD);
        FSerrno = CE_ERASE_FAIL;
        return -1;
    }
    else
    {
        FileObjectCopy (cwdptr, tempCWD);
        return 0;
    }
}


/****************************************************************
  Function:
    int eraseDir (char * path)
  Summary:
    FSrmdir helper function to erase dirs
  Conditions:
    This function should not be called by the user.
  Input:
    path -  The name of the directory to delete
  Return Values:
    0 -  Dir was deleted successfully
    -1 - Dir could not be deleted.
  Side Effects:
    None
  Description:
    The eraseDir function is a helper function for the rmdirhelper
    function.  The eraseDir function will search for the
    directory that matches the specified path name and then erase
    it with the FILEerase function.
  Remarks:
    None.
  *****************************************************************/

int eraseDir (char * path)
{
    CETYPE result;
    BYTE Index;
    FSFILE tempCWDobj2;

    if (MDD_WriteProtectState())
    {
        return (-1);
    }

    // preserve CWD
    FileObjectCopy(&tempCWDobj2, cwdptr);

    for (Index = 0; Index <11; Index++)
    {
        cwdptr->name[Index] = *(path + Index);
    }

    // copy file object over
    FileObjectCopy(&gFileTemp, cwdptr);

    // See if the file is found
    result = FILEfind (cwdptr, &gFileTemp, LOOK_FOR_MATCHING_ENTRY, 0);

    if (result != CE_GOOD)
    {
        FileObjectCopy(cwdptr, &tempCWDobj2);
        return -1;
    }
    result = FILEerase(cwdptr, &cwdptr->entry, TRUE);
    if( result == CE_GOOD )
    {
        FileObjectCopy(cwdptr, &tempCWDobj2);
        return 0;
    }
    else
    {
        FileObjectCopy(cwdptr, &tempCWDobj2);
        return -1;
    }
}
#endif



#endif


#ifdef ALLOW_FILESEARCH


/***********************************************************************************
  Function:
    int FindFirst (const char * fileName, unsigned int attr, SearchRec * rec)
  Summary:
    Initial search function
  Conditions:
    None
  Input:
    fileName - The name to search for
             - Parital string search characters
             - * - Indicates the rest of the filename or extension can vary (e.g. FILE.*)
             - ? - Indicates that one character in a filename can vary (e.g. F?LE.T?T)
    attr -            The attributes that a found file may have
         - ATTR_READ_ONLY -  File may be read only
         - ATTR_HIDDEN -     File may be a hidden file
         - ATTR_SYSTEM -     File may be a system file
         - ATTR_VOLUME -     Entry may be a volume label
         - ATTR_DIRECTORY -  File may be a directory
         - ATTR_ARCHIVE -    File may have archive attribute
         - ATTR_MASK -       All attributes
    rec -             pointer to a structure to put the file information in
  Return Values:
    0 -  File was found
    -1 - No file matching the specified criteria was found
  Side Effects:
    Search criteria from previous FindFirst call on passed SearchRec object
    will be lost.  The FSerrno variable will be changed.
  Description:
    The FindFirst function will search for a file based on parameters passed in
    by the user.  This function will use the FILEfind function to parse through
    the current working directory searching for entries that match the specified
    parameters.  If a file is found, its parameters are copied into the SearchRec
    structure, as are the initial parameters passed in by the user and the position
    of the file entry in the current working directory.
  Remarks:
    Call FindFirst or FindFirstpgm before calling FindNext
  ***********************************************************************************/

int FindFirst (const char * fileName, unsigned int attr, SearchRec * rec)
{
    FSFILE f;
    FILEOBJ fo = &f;
    CETYPE result;
    WORD fHandle;
    BYTE j;
    BYTE Index;

    FSerrno = CE_GOOD;

    if( !FormatFileName(fileName, fo->name, 1) )
    {
        FSerrno = CE_INVALID_FILENAME;
        return -1;
    }

    rec->initialized = FALSE;

    for (Index = 0; (Index < 12) && (fileName[Index] != 0); Index++)
    {
        rec->searchname[Index] = fileName[Index];
    }
    rec->searchname[Index] = 0;
    rec->searchattr = attr;
#ifdef ALLOW_DIRS
    rec->cwdclus = cwdptr->dirclus;
#else
    rec->cwdclus = FatRootDirClusterValue;
#endif

    fo->dsk = &gDiskData;
    fo->cluster = 0;
    fo->ccls    = 0;
    fo->entry = 0;
    fo->attributes = attr;

#ifndef ALLOW_DIRS
    // start at the root directory
    fo->dirclus    = FatRootDirClusterValue;
    fo->dirccls    = FatRootDirClusterValue;
#else
    fo->dirclus = cwdptr->dirclus;
    fo->dirccls = cwdptr->dirccls;
#endif

    // copy file object over
    FileObjectCopy(&gFileTemp, fo);

    // See if the file is found
    result = FILEfind (fo, &gFileTemp,LOOK_FOR_MATCHING_ENTRY, 1);

    if (result != CE_GOOD)
    {
        FSerrno = CE_FILE_NOT_FOUND;
        return -1;
    }
    else
    {
        fHandle = fo->entry;
        result = FILEopen (fo, &fHandle, 'r');
    }
    if (result == CE_GOOD)
    {
        // Copy as much name as there is
        if (fo->attributes != ATTR_VOLUME)
        {
            for (Index = 0, j = 0; (j < 8) && (fo->name[j] != 0x20); Index++, j++)
            {
               rec->filename[Index] = fo->name[j];
            }
            // Add the radix if its not a dir
            if ((fo->name[8] != ' ') || (fo->name[9] != ' ') || (fo->name[10] != ' '))
               rec->filename[Index++] = '.';
            // Move to the extension, even if there are more space chars
            for (j = 8; (j < 11) && (fo->name[j] != 0x20); Index++, j++)
            {
               rec->filename[Index] = fo->name[j];
            }
            // Null terminate it
            rec->filename[Index] = 0;
        }
        else
        {
            for (Index = 0; Index < DIR_NAMECOMP; Index++)
            {
                rec->filename[Index] = fo->name[Index];
            }
            rec->filename[Index] = 0;
        }

        rec->attributes = fo->attributes;
        rec->filesize = fo->size;
        rec->timestamp = (DWORD)((DWORD)fo->date << 16) + fo->time;
        rec->entry = fo->entry;
        rec->initialized = TRUE;
        return 0;
    }
    else
    {
        FSerrno = CE_BADCACHEREAD;
        return -1;
    }
}


/**********************************************************************
  Function:
    int FindNext (SearchRec * rec)
  Summary:
    Sequential search function
  Conditions:
    None
  Input:
    rec -  The structure to store the file information in
  Return Values:
    0 -  File was found
    -1 - No additional files matching the specified criteria were found
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    The FindNext function performs the same function as the FindFirst
    funciton, except it does not copy any search parameters into the
    SearchRec structure (only info about found files) and it begins
    searching at the last directory entry offset at which a file was
    found, rather than at the beginning of the current working
    directory.
  Remarks:
    Call FindFirst or FindFirstpgm before calling this function
  **********************************************************************/

int FindNext (SearchRec * rec)
{
    FSFILE f;
    FILEOBJ fo = &f;
    CETYPE result;
    BYTE i, j;

    FSerrno = CE_GOOD;

    // Make sure we called FindFirst on this object
    if (rec->initialized == FALSE)
    {
        FSerrno = CE_NOT_INIT;
        return -1;
    }

    // Make sure we called FindFirst in the cwd
#ifdef ALLOW_DIRS
    if (rec->cwdclus != cwdptr->dirclus)
    {
        FSerrno = CE_INVALID_ARGUMENT;
        return -1;
    }
#endif

    if( !FormatFileName(rec->searchname, fo->name, 1) )
    {
        FSerrno = CE_INVALID_FILENAME;
        return -1;
    }

    /* Brn: Copy the formatted name to "fo" which is necesary before calling "FILEfind" function */
    //strcpy(fo->name,rec->searchname);

    fo->dsk = &gDiskData;
    fo->cluster = 0;
    fo->ccls    = 0;
    fo->entry = rec->entry + 1;
    fo->attributes = rec->searchattr;

#ifndef ALLOW_DIRS
    // start at the root directory
    fo->dirclus    = FatRootDirClusterValue;
    fo->dirccls    = FatRootDirClusterValue;
#else
    fo->dirclus = cwdptr->dirclus;
    fo->dirccls = cwdptr->dirccls;
#endif

    // copy file object over
    FileObjectCopy(&gFileTemp, fo);

    // See if the file is found
    result = FILEfind (fo, &gFileTemp,LOOK_FOR_MATCHING_ENTRY, 1);

    if (result != CE_GOOD)
    {
        FSerrno = CE_FILE_NOT_FOUND;
        return -1;
    }
    else
    {
        if (fo->attributes != ATTR_VOLUME)
        {
            for (i = 0, j = 0; (j < 8) && (fo->name[j] != 0x20); i++, j++)
            {
               rec->filename[i] = fo->name[j];
            }
            // Add the radix if its not a dir
            if ((fo->name[8] != ' ') || (fo->name[9] != ' ') || (fo->name[10] != ' '))
               rec->filename[i++] = '.';
            // Move to the extension, even if there are more space chars
            for (j = 8; (j < 11) && (fo->name[j] != 0x20); i++, j++)
            {
               rec->filename[i] = fo->name[j];
            }
            // Null terminate it
            rec->filename[i] = 0;
        }
        else
        {
            for (i = 0; i < DIR_NAMECOMP; i++)
            {
                rec->filename[i] = fo->name[i];
            }
            rec->filename[i] = 0;
        }

        rec->attributes = fo->attributes;
        rec->filesize = fo->size;
        rec->timestamp = (DWORD)((DWORD)fo->date << 16) + fo->time;
        rec->entry = fo->entry;
        return 0;
    }
}


#endif



#ifdef ALLOW_FSFPRINTF



/**********************************************************************
  Function:
    int FSputc (char c, FSFILE * file)
  Summary:
    FSfprintf helper function to write a char
  Conditions:
    This function should not be called by the user.
  Input:
    c - The character to write to the file.
    file - The file to write to.
  Return Values:
    0 -   The character was written successfully
    EOF - The character was not written to the file.
  Side Effects:
    None
  Description:
    This is a helper function for FSfprintf.  It will write one
    character to a file.
  Remarks:
    None
  **********************************************************************/

int FSputc (char c, FSFILE * file)
{
    if (FSfwrite ((void *)&c, 1, 1, file) != 1)
        return EOF;
    else
        return 0;
}


/**********************************************************************
  Function:
    int str_put_n_chars (FSFILE * handle, unsigned char n, char c)
  Summary:
    FSfprintf helper function to write a char multiple times
  Conditions:
    This function should not be called by the user.
  Input:
    handle - The file to write to.
    n -      The number of times to write that character to a file.
    c - The character to write to the file.
  Return Values:
    0 -   The characters were written successfully
    EOF - The characters were not written to the file.
  Side Effects:
    None
  Description:
    This funciton is used by the FSfprintf function to write multiple
    instances of a single character to a file (for example, when
    padding a format specifier with leading spacez or zeros).
  Remarks:
    None.
  **********************************************************************/


unsigned char str_put_n_chars (FSFILE * handle, unsigned char n, char c)
{
    while (n--)
    if (FSputc (c, handle) == EOF)
        return 1;
    return 0;
}


/**********************************************************************
  Function:
    // PIC24/30/33/32
    int FSfprintf (FSFILE * fptr, const char * fmt, ...)
    // PIC18
    int FSfpritnf (FSFILE * fptr, const rom char * fmt, ...)
  Summary:
    Function to write formatted strings to a file
  Conditions:
    For PIC18, integer promotion must be enabled in the project build
    options menu.  File opened in a write mode.
  Input:
    fptr - A pointer to the file to write to.
    fmt -  A string of characters and format specifiers to write to
           the file
    ... -  Additional arguments inserted in the string by format
           specifiers
  Returns:
    The number of characters written to the file
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    Writes a specially formatted string to a file.
  Remarks:
    Consult AN1045 for a full description of how to use format
    specifiers.
  **********************************************************************/

#ifdef __18CXX
int FSfprintf (FSFILE *fptr, const rom char *fmt, ...)
#else
int FSfprintf (FSFILE *fptr, const char * fmt, ...)
#endif
{
    va_list ap;
    int n;

    va_start (ap, fmt);
    n = FSvfprintf (fptr, fmt, ap);
    va_end (ap);
    return n;
}


/**********************************************************************
  Function:
    // PIC24/30/33/32
    int FSvfprintf (FSFILE * handle, const char * formatString, va_list ap)
    // PIC18
    int FSvfpritnf (auto FSFILE * handle, auto const rom char * formatString, auto va_list ap)
  Summary:
    Helper function for FSfprintf
  Conditions:
    This function should not be called by the user.
  Input:
    handle -        A pointer to the file to write to.
    formatString -  A string of characters and format specifiers to write to
                    the file
    ap -            A structure pointing to the arguments on the stack
  Returns:
    The number of characters written to the file
  Side Effects:
    The FSerrno variable will be changed.
  Description:
    This helper function will access the elements passed to FSfprintf
  Remarks:
    Consult AN1045 for a full description of how to use format
    specifiers.
  **********************************************************************/

#ifdef __18CXX
int FSvfprintf (auto FSFILE *handle, auto const rom char * formatString, auto va_list ap)
#else
int FSvfprintf (FSFILE *handle, const char * formatString, va_list ap)
#endif
{
    unsigned char c;
    int count = 0;

    for (c = *formatString; c; c = *++formatString)
    {
        if (c == '%')
        {
            unsigned char    flags = 0;
            unsigned char    width = 0;
            unsigned char    precision = 0;
            unsigned char    have_precision = 0;
            unsigned char    size = 0;
#ifndef __18CXX
            unsigned char   size2 = 0;
#endif
            unsigned char    space_cnt;
            unsigned char    cval;
#ifdef __18CXX
            unsigned long    larg;
            far rom char *   romstring;
#else
            unsigned long long larg;
#endif
            char *         ramstring;
            int n;

            FSerrno = CE_GOOD;

            c = *++formatString;

            while (c == '-' || c == '+' || c == ' ' || c == '#'
                || c == '0')
            {
                switch (c)
                {
                    case '-':
                        flags |= _FLAG_MINUS;
                        break;
                    case '+':
                        flags |= _FLAG_PLUS;
                        break;
                    case ' ':
                        flags |= _FLAG_SPACE;
                        break;
                    case '#':
                        flags |= _FLAG_OCTO;
                        break;
                    case '0':
                        flags |= _FLAG_ZERO;
                        break;
                }
                c = *++formatString;
            }
            /* the optional width field is next */
            if (c == '*')
            {
                n = va_arg (ap, int);
                if (n < 0)
                {
                    flags |= _FLAG_MINUS;
                    width = -n;
                }
                else
                    width = n;
                c = *++formatString;
            }
            else
            {
                cval = 0;
                while ((unsigned char) isdigit (c))
                {
                    cval = cval * 10 + c - '0';
                    c = *++formatString;
                }
                width = cval;
            }

            /* if '-' is specified, '0' is ignored */
            if (flags & _FLAG_MINUS)
                flags &= ~_FLAG_ZERO;

            /* the optional precision field is next */
            if (c == '.')
            {
                c = *++formatString;
                if (c == '*')
                {
                    n = va_arg (ap, int);
                    if (n >= 0)
                    {
                        precision = n;
                        have_precision = 1;
                    }
                    c = *++formatString;
                }
                else
                {
                    cval = 0;
                    while ((unsigned char) isdigit (c))
                    {
                        cval = cval * 10 + c - '0';
                        c = *++formatString;
                    }
                    precision = cval;
                    have_precision = 1;
                }
            }

            /* the optional 'h' specifier. since int and short int are
                the same size for MPLAB C18, this is a NOP for us. */
            if (c == 'h')
            {
                c = *++formatString;
                /* if 'c' is another 'h' character, this is an 'hh'
                    specifier and the size is 8 bits */
                if (c == 'h')
                {
                    size = _FMT_BYTE;
                    c = *++formatString;
                }
            }
            else if (c == 't' || c == 'z')
                c = *++formatString;
#ifdef __18CXX
            else if (c == 'H' || c == 'T' || c == 'Z')
            {
                size = _FMT_SHRTLONG;
                c = *++formatString;
            }
            else if (c == 'l' || c == 'j')
#else
            else if (c == 'q' || c == 'j')
            {
                size = _FMT_LONGLONG;
                c = *++formatString;
            }
            else if (c == 'l')
#endif
            {
                size = _FMT_LONG;
                c = *++formatString;
            }

            switch (c)
            {
                case '\0':
                /* this is undefined behaviour. we have a trailing '%' character
                    in the string, perhaps with some flags, width, precision
                    stuff as well, but no format specifier. We'll, arbitrarily,
                    back up a character so that the loop will terminate
                    properly when it loops back and we'll output a '%'
                    character. */
                    --formatString;
                /* fallthrough */
                case '%':
                    if (FSputc ('%', handle) == EOF)
                    {
                        FSerrno = CE_WRITE_ERROR;
                        return EOF;
                    }
                    ++count;
                    break;
                case 'c':
                    space_cnt = 0;
                    if (width > 1)
                    {
                        space_cnt = width - 1;
                        count += space_cnt;
                    }
                    if (space_cnt && !(flags & _FLAG_MINUS))
                    {
                        if (str_put_n_chars (handle, space_cnt, ' '))
                        {
                            FSerrno = CE_WRITE_ERROR;
                            return EOF;
                        }
                        space_cnt = 0;
                    }
                    c = va_arg (ap, int);
                    if (FSputc (c, handle) == EOF)
                    {
                        FSerrno = CE_WRITE_ERROR;
                        return EOF;
                    }
                    ++count;
                    if (str_put_n_chars (handle, space_cnt, ' '))
                    {
                        FSerrno = CE_WRITE_ERROR;
                        return EOF;
                    }
                    break;
                case 'S':
#ifdef __18CXX
                    if (size == _FMT_SHRTLONG)
                        romstring = va_arg (ap, rom far char *);
                    else
                        romstring = (far rom char*)va_arg (ap, rom near char *);
                    n = strlenpgm (romstring);
                    /* Normalize the width based on the length of the actual
                        string and the precision. */
                    if (have_precision && precision < (unsigned char) n)
                        n = precision;
                    if (width < (unsigned char) n)
                        width = n;
                    space_cnt = width - (unsigned char) n;
                    count += space_cnt;
                    /* we've already calculated the space count that the width
                        will require. now we want the width field to have the
                        number of character to display from the string itself,
                        limited by the length of the actual string and the
                        specified precision. */
                    if (have_precision && precision < width)
                        width = precision;
                    /* if right justified, we print the spaces before the
                        string */
                    if (!(flags & _FLAG_MINUS))
                    {
                        if (str_put_n_chars (handle, space_cnt, ' '))
                        {
                            FSerrno = CE_WRITE_ERROR;
                            return EOF;
                        }
                        space_cnt = 0;
                    }
                    cval = 0;
                    for (c = *romstring; c && cval < width; c = *++romstring)
                    {
                        if (FSputc (c, handle) == EOF)
                        {
                            FSerrno = CE_WRITE_ERROR;
                            return EOF;
                        }
                        ++count;
                        ++cval;
                    }
                    /* If there are spaces left, it's left justified.
                        Either way, calling the function unconditionally
                        is smaller code. */
                    if (str_put_n_chars (handle, space_cnt, ' '))
                    {
                        FSerrno = CE_WRITE_ERROR;
                        return EOF;
                    }
                    break;
#endif
                case 's':
                    ramstring = va_arg (ap, char *);
                    n = strlen (ramstring);
                    /* Normalize the width based on the length of the actual
                        string and the precision. */
                    if (have_precision && precision < (unsigned char) n)
                        n = precision;
                    if (width < (unsigned char) n)
                        width = n;
                    space_cnt = width - (unsigned char) n;
                    count += space_cnt;
                    /* we've already calculated the space count that the width
                        will require. now we want the width field to have the
                        number of character to display from the string itself,
                        limited by the length of the actual string and the
                        specified precision. */
                    if (have_precision && precision < width)
                        width = precision;
                    /* if right justified, we print the spaces before the string */
                    if (!(flags & _FLAG_MINUS))
                    {
                        if (str_put_n_chars (handle, space_cnt, ' '))
                        {
                            FSerrno = CE_WRITE_ERROR;
                            return EOF;
                        }
                        space_cnt = 0;
                    }
                    cval = 0;
                    for (c = *ramstring; c && cval < width; c = *++ramstring)
                    {
                        if (FSputc (c, handle) == EOF)
                        {
                            FSerrno = CE_WRITE_ERROR;
                            return EOF;
                        }
                        ++count;
                        ++cval;
                    }
                    /* If there are spaces left, it's left justified.
                        Either way, calling the function unconditionally
                        is smaller code. */
                    if (str_put_n_chars (handle, space_cnt, ' '))
                    {
                        FSerrno = CE_WRITE_ERROR;
                        return EOF;
                    }
                    break;
                case 'd':
                case 'i':
                    flags |= _FLAG_SIGNED;
                /* fall through */
                case 'o':
                case 'u':
                case 'x':
                case 'X':
                case 'b':
                case 'B':
                    /* This is a bit of a trick. The 'l' and 'hh' size
                        specifiers are valid only for the integer conversions,
                        not the 'p' or 'P' conversions, and are ignored for the
                        latter. By jumping over the additional size specifier
                        checks here we get the best code size since we can
                        limit the size checks in the remaining code. */
                    if (size == _FMT_LONG)
                    {
                        if (flags & _FLAG_SIGNED)
                            larg = va_arg (ap, long int);
                        else
                            larg = va_arg (ap, unsigned long int);
                        goto _do_integer_conversion;
                    }
                    else if (size == _FMT_BYTE)
                    {
                        if (flags & _FLAG_SIGNED)
                            larg = (signed char) va_arg (ap, int);
                        else
                            larg = (unsigned char) va_arg (ap, unsigned int);
                        goto _do_integer_conversion;
                    }
#ifndef __18CXX
                    else if (size == _FMT_LONGLONG)
                    {
                        if (flags & _FLAG_SIGNED)
                            larg = (signed long long)va_arg (ap, long long);
                        else
                            larg = (unsigned long long) va_arg (ap, unsigned long long);
                        goto _do_integer_conversion;
                    }
#endif
                    /* fall trough */
                case 'p':
                case 'P':
#ifdef __18CXX
                    if (size == _FMT_SHRTLONG)
                    {
                        if (flags & _FLAG_SIGNED)
                            larg = va_arg (ap, short long int);
                        else
                            larg = va_arg (ap, unsigned short long int);
                    }
                    else
#endif
                        if (flags & _FLAG_SIGNED)
                            larg = va_arg (ap, int);
                        else
                            larg = va_arg (ap, unsigned int);
                    _do_integer_conversion:
                        /* default precision is 1 */
                        if (!have_precision)
                            precision = 1;
                        {
                            unsigned char digit_cnt = 0;
                            unsigned char prefix_cnt = 0;
                            unsigned char sign_char;
                            /* A 32 bit number will require at most 32 digits in the
                                string representation (binary format). */
#ifdef __18CXX
                            char buf[33];
                            /* Start storing digits least-significant first */
                            char *q = &buf[31];
                            /* null terminate the string */
                            buf[32] = '\0';
#else
                            char buf[65];
                            char *q = &buf[63];
                            buf[64] = '\0';
#endif
                            space_cnt = 0;
                            size = 10;

                            switch (c)
                            {
                                case 'b':
                                case 'B':
                                    size = 2;
#ifndef __18CXX
                                    size2 = 1;
#endif
                                    break;
                                case 'o':
                                    size = 8;
#ifndef __18CXX
                                    size2 = 3;
#endif
                                    break;
                                case 'p':
                                case 'P':
                                    /* from here on out, treat 'p' conversions just
                                        like 'x' conversions. */
                                    c += 'x' - 'p';
                                /* fall through */
                                case 'x':
                                case 'X':
                                    size = 16;
#ifndef __18CXX
                                    size2 = 4;
#endif
                                    break;
                            }// switch (c)

                            /* if it's an unsigned conversion, we should ignore the
                                ' ' and '+' flags */
                            if (!(flags & _FLAG_SIGNED))
                                flags &= ~(_FLAG_PLUS | _FLAG_SPACE);

                            /* if it's a negative value, we need to negate the
                                unsigned version before we convert to text. Using
                                unsigned for this allows us to (ab)use the 2's
                                complement system to avoid overflow and be able to
                                adequately handle LONG_MIN.

                                We'll figure out what sign character to print, if
                                any, here as well. */
#ifdef __18CXX
                            if (flags & _FLAG_SIGNED && ((long) larg < 0))
                            {
                                larg = -(long) larg;
#else
                            if (flags & _FLAG_SIGNED && ((long long) larg < 0))
                            {
                                larg = -(long long) larg;
#endif
                                sign_char = '-';
                                ++digit_cnt;
                            }
                            else if (flags & _FLAG_PLUS)
                            {
                        sign_char = '+';
                        ++digit_cnt;
                     }
                      else if (flags & _FLAG_SPACE)
                      {
                                sign_char = ' ';
                                ++digit_cnt;
                            }
                            else
                                sign_char = '\0';
                            /* get the digits for the actual number. If the
                                precision is zero and the value is zero, the result
                                is no characters. */
                            if (precision || larg)
                            {
                                do
                                {
#ifdef __18CXX
                                    cval = s_digits[larg % size];
                                    if (c == 'X' && cval >= 'a')
                                        cval -= 'a' - 'A';
                                    larg /= size;
#else
                                    // larg is congruent mod size2 to its lower 16 bits
                                    // for size2 = 2^n, 0 <= n <= 4
                                    if (size2 != 0)
                                        cval = s_digits[(unsigned int) larg % size];
                                    else
                                        cval = s_digits[larg % size];
                                    if (c == 'X' && cval >= 'a')
                                        cval -= 'a' - 'A';
                                    if (size2 != 0)
                                        larg = larg >> size2;
                                    else
                                        larg /= size;
#endif
                                    *q-- = cval;
                                    ++digit_cnt;
                                } while (larg);
                                /* if the '#' flag was specified and we're dealing
                                    with an 'o', 'b', 'B', 'x', or 'X' conversion,
                                    we need a bit more. */
                                if (flags & _FLAG_OCTO)
                                {
                                    if (c == 'o')
                                    {
                                        /* per the standard, for octal, the '#' flag
                                            makes the precision be at least one more
                                            than the number of digits in the number */
                                        if (precision <= digit_cnt)
                                            precision = digit_cnt + 1;
                                    }
                                    else if (c == 'x' || c == 'X' || c == 'b' || c == 'B')
                                        prefix_cnt = 2;
                                }
                            }
                            else
                                digit_cnt = 0;

                            /* The leading zero count depends on whether the '0'
                                flag was specified or not. If it was not, then the
                                count is the difference between the specified
                                precision and the number of digits (including the
                                sign character, if any) to be printed; otherwise,
                                it's as if the precision were equal to the max of
                                the specified precision and the field width. If a
                                precision was specified, the '0' flag is ignored,
                                however. */
                            if ((flags & _FLAG_ZERO) && (width > precision)
                                && !have_precision)
                                precision = width;
                            /* for the rest of the processing, precision contains
                                the leading zero count for the conversion. */
                            if (precision > digit_cnt)
                                precision -= digit_cnt;
                            else
                                precision = 0;
                            /* the space count is the difference between the field
                                width and the digit count plus the leading zero
                                count. If the width is less than the digit count
                                plus the leading zero count, the space count is
                                zero. */
                            if (width > precision + digit_cnt + prefix_cnt)
                                space_cnt =   width - precision - digit_cnt - prefix_cnt;

                            /* for output, we check the justification, if it's
                                right justified and the space count is positive, we
                                emit the space characters first. */
                            if (!(flags & _FLAG_MINUS) && space_cnt)
                            {
                                if (str_put_n_chars (handle, space_cnt, ' '))
                                {
                                    FSerrno = CE_WRITE_ERROR;
                                    return EOF;
                                }
                                count += space_cnt;
                                space_cnt = 0;
                            }
                            /* if we have a sign character to print, that comes
                                next */
                            if (sign_char)
                                if (FSputc (sign_char, handle) == EOF)
                                {
                                    FSerrno = CE_WRITE_ERROR;
                                    return EOF;
                                }
                            /* if we have a prefix (0b, 0B, 0x or 0X), that's next */
                            if (prefix_cnt)
                            {
                                if (FSputc ('0', handle) == EOF)
                                {
                                    FSerrno = CE_WRITE_ERROR;
                                    return EOF;
                                }
                                if (FSputc (c, handle) == EOF)
                                {
                                    FSerrno = CE_WRITE_ERROR;
                                    return EOF;
                                }
                            }
                            /* if we have leading zeros, they follow. the prefix, if any
                                is included in the number of digits when determining how
                                many leading zeroes are needed. */
//                            if (precision > prefix_cnt)
  //                              precision -= prefix_cnt;
                            if (str_put_n_chars (handle, precision, '0'))
                            {
                                FSerrno = CE_WRITE_ERROR;
                                return EOF;
                            }
                            /* print the actual number */
                            for (cval = *++q; cval; cval = *++q)
                                if (FSputc (cval, handle) == EOF)
                                {
                                    FSerrno = CE_WRITE_ERROR;
                                    return EOF;
                                }
                            /* if there are any spaces left, they go to right-pad
                                the field */
                            if (str_put_n_chars (handle, space_cnt, ' '))
                            {
                                FSerrno = CE_WRITE_ERROR;
                                return EOF;
                            }

                            count += precision + digit_cnt + space_cnt + prefix_cnt;
                        }
                        break;
                case 'n':
                    switch (size)
                    {
                        case _FMT_LONG:
                            *(long *) va_arg (ap, long *) = count;
                            break;
#ifdef __18CXX
                        case _FMT_SHRTLONG:
                            *(short long *) va_arg (ap, short long *) = count;
                            break;
#else
                        case _FMT_LONGLONG:
                            *(long long *) va_arg (ap, long long *) = count;
                            break;
#endif
                        case _FMT_BYTE:
                            *(signed char *) va_arg (ap, signed char *) = count;
                            break;
                        default:
                            *(int *) va_arg (ap, int *) = count;
                            break;
                    }
                    break;
                default:
                    /* undefined behaviour. we do nothing */
                    break;
            }
        }
        else
        {
            if (FSputc (c, handle) == EOF)
            {
                FSerrno = CE_WRITE_ERROR;
                return EOF;
            }
            ++count;
        }
    }
    return count;
}



#endif




