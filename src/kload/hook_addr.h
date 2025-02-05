// ADDRESSES for hook.cpp
#define CODELEN 19
enum 
{
    C_D3DCREATE_CS,
    C_LOADTEXTUREFORPLAYER_CS, C_LOADTEXTUREFORPLAYER,
    TBL_BEGINRENDER1,	TBL_BEGINRENDER2,
    C_EDITCOPYPLAYERNAME_CS, C_COPYSTRING_CS,
    C_SUB_MENUMODE, C_ADD_MENUMODE,
    C_READ_FILE, C_WRITE_FILE,
    C_COPY_DATA, C_COPY_DATA2,
    C_ENTER_UNIFORM_SELECT, C_EXIT_UNIFORM_SELECT,
    C_WRITE_DATA,
    C_READ_NAMES, C_READ_NAMES2,
    C_READ_DATA,
};

#define NOCODEADDR {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
DWORD codeArray[][CODELEN] = { 
    // PES2013 demo
    {
        0x10D2A45,
        0, 0, 
        0, 0, 
        0, 0, 
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0,
        0, 0,
        0,
    },
    // PES2013 v1.00
    NOCODEADDR
    // PES2013 v1.01
    NOCODEADDR
    // PES2013 v1.02
    NOCODEADDR
    // PES2013 v1.03
    {
        0,
        0, 0, 
        0, 0, 
        0, 0, 
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0x12AE078,
        0x510C9F, 0x12AEDB7,
        0xCE3828,
    },
    // PES2013 v1.04
    {
		0,
		0, 0, 
		0, 0, 
		0, 0, 
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0x12B1788,
        0x511540, 0x12B24C7,
        0xCE66D8,
    }
};

#define DATALEN 13
enum {
	PLAYERDATA, ISREFEREEADDR,
	GENERALINFO, REPLAYGENERALINFO,
    MENU_MODE_IDX, MAIN_SCREEN_INDICATOR, INGAME_INDICATOR,
    NEXT_MATCH_DATA_PTR, CUP_MODE_PTR, EDIT_DATA_PTR,
    EDIT_DATA_SIZE, REPLAY_DATA_SIZE, BAL_DATA_SIZE,
};

#define NODATAADDR {0,0,0,0,0,0,0,0,0,0,0,0,0},
DWORD dataArray[][DATALEN] = {
    // PES2013 demo
    {
        0, 0,
        0, 0,
        0, 0, 0,
        0, 0, 0,
        123, 456, 789,
    },
    // PES2013 v1.00
    NODATAADDR
    // PES2013 v1.01
    NODATAADDR
    // PES2013 v1.02
    NODATAADDR
    // PES2013 v1.03
    {
        0, 0,
        0, 0,
        0, 0, 0,
        123, 456, 789,
    },
    // PES2013 v1.04
    {
        0, 0,
        0, 0,
        0, 0, 0,
        0, 0, 0,
        123, 456, 789,
    }
};

#define LTFPLEN 15
#define NOLTFPADDR {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
BYTE ltfpPatchArray[][LTFPLEN] = {
    // PES2013 demo
	NOLTFPADDR
    // PES2013 v1.00
    NOLTFPADDR
    // PES2013 v1.01
    NOLTFPADDR
    // PES2013 v1.02
    NOLTFPADDR
    // PES2013 v1.03
    NOLTFPADDR
    // PES2013 v1.04
    NOLTFPADDR
};

DWORD code[CODELEN];
DWORD dta[DATALEN];
BYTE ltfpPatch[LTFPLEN];
