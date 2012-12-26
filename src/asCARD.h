
//--------------------------------------------------------------------------------------
// DEFINE SD-CARD PRESENCE AND BEHAVIOUR
// Undefine to exclude Lua SD-card bindings from PIC32Lua or
// to skip booting the file "BOOT32.LUA"
#define USE_SDCARD_CODE
#define USE_SDCARD_BOOTFILE






//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// no changes below this line

#ifdef USE_SDCARD_CODE

	#define CARD_DETECTED      1
	#define CARD_MOUNTED       2
	
	#define CARD_BOOT_ERR_NOCODE	 1
	#define CARD_BOOT_ERR_NOBOOT	 2
	#define CARD_BOOT_ERR_NOMOUNT	 4
	#define CARD_BOOT_ERR_NOFILE	 8
	#define CARD_BOOT_ERR_NORUN		16
	#define CARD_BOOT_ERR_NOFCT		32


	extern unsigned gCardState;
	extern SearchRec CardRec;
	extern int asCARD_Detect(void);
	extern int asCARD_Mount(void);
	extern int asCARD_FindFirst(char *fname, unsigned int);
	extern int asCARD_FindNext(void);

	extern int asCARD_Boot(lua_State *L);


#endif
