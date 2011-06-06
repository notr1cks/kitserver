/* <fserv>
 *
 */
#define UNICODE
#define THISMOD &k_fserv

#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#include "kload_exp.h"
#include "afsio.h"
#include "fserv.h"
#include "fserv_addr.h"
#include "dllinit.h"
#include "configs.h"
#include "configs.hpp"
#include "utf8.h"
#include "player.h"
#include "replay.h"
#include "bal.h"

#define lang(s) getTransl("fserv",s)

#include <map>
#include <list>
#include <hash_map>
#include <wchar.h>

//#define CREATE_FLAGS FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_NO_BUFFERING
#define CREATE_FLAGS 0

#define FIRST_FACE_SLOT 13000
#define NUM_SLOTS 65536
#define FIRST_XFACE_ID 3000
#define FIRST_XHAIR_ID 3001

#define NETWORK_MODE 4

#define MAX_FACE_ID 1500
#define MAX_HAIR_ID 1500
#define MANEKEN_ID 948

#define REPLAY_SIG "ks11"

// VARIABLES
HINSTANCE hInst = NULL;
KMOD k_fserv = {MODID, NAMELONG, NAMESHORT, DEFAULT_DEBUG};

class fserv_config_t
{
public:
    bool _enable_online;
    fserv_config_t() : _enable_online(false) {}
};

fserv_config_t _fserv_config;

class player_facehair_t
{
public:
    BYTE specialFace;
    BYTE specialHair;
    DWORD faceHairBits;

    player_facehair_t(PLAYER_INFO* p) :
        specialFace(p->specialFace),
        specialHair(p->specialHair),
        faceHairBits(p->faceHairBits)
    {}
};

// GLOBALS
hash_map<DWORD,wstring> _player_face;
hash_map<DWORD,wstring> _player_hair;
hash_map<DWORD,WORD> _player_face_slot;
hash_map<DWORD,WORD> _player_hair_slot;

hash_map<DWORD,player_facehair_t> _saved_facehair;

wstring* _fast_bin_table[NUM_SLOTS-FIRST_FACE_SLOT];

bool _struct_replaced = false;
int _num_slots = 8094;

DWORD _gotFaceBin = 0;
DWORD _gotHairBin = 0;

// FUNCTIONS
HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface);

bool IsSpecialHair(PLAYER_INFO* p);
bool IsSpecialFace(PLAYER_INFO* p);
bool IsSpecialHairByte(BYTE b);
bool IsSpecialFaceByte(BYTE b);

void fservAtFaceHairCallPoint();
void fservAtCopyEditDataCallPoint();
KEXPORT void fservAtFaceHair(DWORD dest, PLAYER_DETAILS* src);

void fservAtUseFaceHairCallPoint();
KEXPORT void fservAtUseFaceHair(DWORD src, PLAYER_DETAILS* dest);

void fservAtGetFaceBinCallPoint();
void fservAtGetHairBinCallPoint();
KEXPORT DWORD fservGetFaceBin(DWORD faceId);
KEXPORT DWORD fservGetHairBin(DWORD hairId);

void fservAtResetHairCallPoint();
void fservAtResetHair(PLAYER_DETAILS* src, PLAYER_DETAILS* dest, 
                      DWORD numDwords);
void fservAtSquadListCallPoint();
void fservAtSquadList(PLAYER_DETAILS* src, PLAYER_DETAILS* dest, 
                      DWORD numDwords);
void fservAtSetDefaultCallPoint();
void fservAtSetDefault(DWORD src, DWORD dest, DWORD numDwords);

void fservConfig(char* pName, const void* pValue, DWORD a);
bool fservGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize);
bool OpenFileIfExists(const wchar_t* filename, HANDLE& handle, DWORD& size);
void InitMaps();
void fservCopyPlayerData(PLAYER_INFO* players, int place, bool writeList);

void fservWriteEditData(LPCVOID data, DWORD size);
void fservReadReplayData(LPCVOID data, DWORD size);
void fservWriteReplayData(LPCVOID data, DWORD size);
void fservReadBalData(LPCVOID data, DWORD size);

static void string_strip(wstring& s)
{
    static const wchar_t* empties = L" \t\n\r";
    int e = s.find_last_not_of(empties);
    s.erase(e + 1);
    int b = s.find_first_not_of(empties);
    s.erase(0,b);
}

static void string_strip_quotes(wstring& s)
{
    static const wchar_t* empties = L" \t\n\r";
    if (s[s.length()-1]=='"')
        s.erase(s.length()-1);
    if (s[0]=='"')
        s.erase(0,1);
}


/*******************/
/* DLL Entry Point */
/*******************/
EXTERN_C BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		hInst = hInstance;

		RegisterKModule(THISMOD);
		
		if (!checkGameVersion()) {
			LOG(L"Sorry, your game version isn't supported!");
			return false;
		}

        //CHECK_KLOAD(MAKELONG(0,11));

		copyAdresses();
		hookFunction(hk_D3D_CreateDevice, initModule);
	}
	
	else if (dwReason == DLL_PROCESS_DETACH)
	{
	}
	
	return true;
}

void fservConfig(char* pName, const void* pValue, DWORD a)
{
	switch (a) {
		case 1: // debug
			k_fserv.debug = *(DWORD*)pValue;
			break;
        case 2: // disable-online
            _fserv_config._enable_online = *(DWORD*)pValue == 1;
            break;
	}
	return;
}

void PatchCode(DWORD addr, char* patch)
{
    DWORD newProtection = PAGE_EXECUTE_READWRITE;
    DWORD savedProtection;
    if (VirtualProtect(
            (BYTE*)addr, strlen(patch)*2, newProtection, &savedProtection)) {
        memcpy((BYTE*)addr, patch, strlen(patch));
    }
}

HRESULT STDMETHODCALLTYPE initModule(IDirect3D9* self, UINT Adapter,
    D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS *pPresentationParameters, 
    IDirect3DDevice9** ppReturnedDeviceInterface) {

	unhookFunction(hk_D3D_CreateDevice, initModule);

    LOG(L"Initializing Faceserver");

    getConfig("fserv", "debug", DT_DWORD, 1, fservConfig);
    getConfig("fserv", "online.enabled", DT_DWORD, 2, fservConfig);

    _gotFaceBin = code[C_GOT_FACE_BIN];
    _gotHairBin = code[C_GOT_HAIR_BIN];

    HookCallPoint(code[C_CHECK_FACE_AND_HAIR_ID], 
            fservAtFaceHairCallPoint, 6, 1);

    //PatchCode(code[C_CHECK_HAIR_ID], "\x8b\xfe\x90\x90\x90");
    //PatchCode(code[C_CHECK_FACE_ID], "\x8b\xc8\x90\x90\x90");
    HookCallPoint(code[C_GET_FACE_BIN],
            fservAtGetFaceBinCallPoint, 6, 4);
    HookCallPoint(code[C_GET_HAIR_BIN],
            fservAtGetHairBinCallPoint, 6, 0);
    HookCallPoint(code[C_RESET_HAIR],
            fservAtResetHairCallPoint, 6, 2);
    //HookCallPoint(code[C_SQUAD_LIST],
    //        fservAtSquadListCallPoint, 6, 2);
    //HookCallPoint(code[C_SET_DEFAULT_PLAYER],
    //        fservAtSetDefaultCallPoint, 6, 2);
    HookCallPoint(code[C_FACEHAIR_READ],
            fservAtUseFaceHairCallPoint, 6, 2);

    // register callbacks
    afsioAddCallback(fservGetFileInfo);
    addCopyPlayerDataCallback(fservCopyPlayerData);

    addReadReplayDataCallback(fservReadReplayData);
    addReadBalDataCallback(fservReadBalData);
    addWriteReplayDataCallback(fservWriteReplayData);
    addWriteEditDataCallback(fservWriteEditData);

    // initialize face/hair map
    InitMaps();

	TRACE(L"Hooking done.");
    return D3D_OK;
}

void InitMaps()
{
    ZeroMemory(_fast_bin_table, sizeof(_fast_bin_table));

    // process face/hair map file
    hash_map<DWORD,wstring> mapFile;
    wstring mpath(getPesInfo()->gdbDir);
    mpath += L"GDB\\faces\\map.txt";
    if (!readMap(mpath.c_str(), mapFile))
    {
        LOG(L"Couldn't open face/hair-map for reading: {%s}",mpath.c_str());
    }
    else
    {
        for (hash_map<DWORD,wstring>::iterator it = mapFile.begin(); it != mapFile.end(); it++)
        {
            wstring& line = it->second;
            int comma = line.find(',');

            wstring face(line.substr(0,comma));
            string_strip(face);
            if (!face.empty())
                string_strip_quotes(face);

            wstring hair;
            if (comma != string::npos) // hair can be omitted
            {
                hair = line.substr(comma+1);
                string_strip(hair);
                if (!hair.empty())
                    string_strip_quotes(hair);
            }

            //LOG(L"{%d}:",it->first);
            //LOG(L"{%s}/{%s}",face.c_str(),hair.c_str());

            if (!face.empty())
            {
                // check that the file exists, so that we don't crash
                // later, when it's attempted to replace a face.
                wstring filename(getPesInfo()->gdbDir);
                filename += L"GDB\\faces\\" + face;
                HANDLE handle;
                DWORD size;
                if (OpenFileIfExists(filename.c_str(), handle, size))
                {
                    CloseHandle(handle);
                    _player_face.insert(pair<DWORD,wstring>(it->first,face));
                }
                else
                    LOG(L"ERROR in faceserver map for ID = %d: FAILED to open face BIN \"%s\". Skipping", it->first, face.c_str());
            }
            if (!hair.empty())
            {
                // check that the file exists, so that we don't crash
                // later, when it's attempted to replace a hair.
                wstring filename(getPesInfo()->gdbDir);
                filename += L"GDB\\faces\\" + hair;
                HANDLE handle;
                DWORD size;
                if (OpenFileIfExists(filename.c_str(), handle, size))
                {
                    CloseHandle(handle);
                    _player_hair.insert(pair<DWORD,wstring>(it->first,hair));
                }
                else
                    LOG(L"ERROR in faceserver map for ID = %d: FAILED to open hair BIN \"%s\". Skipping", it->first, hair.c_str());
            }
        }
    }
}

WORD GetHairId(DWORD faceHairBits)
{
    return faceHairBits & 0x7ff;
}

WORD GetFaceId(DWORD faceHairBits)
{
    return (faceHairBits >> 15) & 0x7ff;
}

int GetHairBin(PLAYER_INFO* p)
{
    if (!IsSpecialHair(p)) {
        return -1;
    }
    return GetHairId(p->faceHairBits);
}

int GetFaceBin(PLAYER_INFO* p)
{
    if (!IsSpecialFace(p)) {
        return -1;
    }
    return GetFaceId(p->faceHairBits);
}

bool IsSpecialHair(PLAYER_INFO* p)
{
    return (p->specialHair & SPECIAL_HAIR) != 0;
}

bool IsSpecialFace(PLAYER_INFO* p)
{
    return (p->specialFace & SPECIAL_FACE) != 0;
}

bool IsSpecialHairByte(BYTE b)
{
    return (b & SPECIAL_HAIR) != 0;
}

bool IsSpecialFaceByte(BYTE b)
{
    return (b & SPECIAL_FACE) != 0;
}

void SetSpecialHair(PLAYER_INFO* p)
{
    p->specialHair |= SPECIAL_HAIR;
}

void SetSpecialFace(PLAYER_INFO* p)
{
    p->specialFace |= SPECIAL_FACE;
}

void fservCopyPlayerData(PLAYER_INFO* players, int place, bool writeList)
{
    afsioExtendSlots(0x0c, 45000);

    if (place==2)
    {
        //DWORD menuMode = *(DWORD*)data[MENU_MODE_IDX];
        //if (menuMode==NETWORK_MODE && !_fserv_config._enable_online)
        //    return;
    }

    int minFaceId = 2048;
    int maxFaceId = 0;
    int minHairId = 2048;
    int maxHairId = 0;
    hash_map<int,bool> hairsUsed;
    hash_map<int,bool> facesUsed;

    _saved_facehair.clear();
    
    _player_face_slot.clear();
    _player_hair_slot.clear();
    ZeroMemory(_fast_bin_table, sizeof(_fast_bin_table));

    bool indexTaken(false);

    multimap<string,DWORD> mm;
    for (WORD i=0; i<MAX_PLAYERS; i++)
    {
        players[i].index = i;

        if (players[i].id == 0) {
            continue;  // no player at this player slot
        }
        if (players[i].name[0] == '\0') {
            continue;  // no player at this player slot
        }

        // assign slots
        hash_map<DWORD,wstring>::iterator sit;
        sit = _player_face.find(players[i].id);
        if (sit != _player_face.end()) {
            DWORD slot = FIRST_FACE_SLOT + i*2;
            _fast_bin_table[slot - FIRST_FACE_SLOT] = &sit->second;
            _player_face_slot.insert(pair<DWORD,WORD>(sit->first,slot));
        }
        sit = _player_hair.find(players[i].id);
        if (sit != _player_hair.end()) {
            DWORD slot = FIRST_FACE_SLOT + i*2 + 1;
            _fast_bin_table[slot - FIRST_FACE_SLOT] = &sit->second;
            _player_hair_slot.insert(pair<DWORD,WORD>(sit->first,slot));
        }

        // save original info
        player_facehair_t facehair(&players[i]);
        _saved_facehair.insert(
            pair<DWORD,player_facehair_t>(i, facehair));

        hash_map<DWORD,WORD>::iterator it = 
            _player_face_slot.find(players[i].id);
        if (it != _player_face_slot.end())
        {
            LOG(L"player #%d assigned slot (face) #%d",
                    players[i].id,it->second);
            // if not unique face, remember that for later restoring
            //if (!(IsSpecialFace(&players[i]))) {
            //    _non_unique_face.push_back(i);
            //}
            // set new face
            SetSpecialFace(&players[i]);
        }
        it = _player_hair_slot.find(players[i].id);
        if (it != _player_hair_slot.end())
        {
            LOG(L"player #%d assigned slot (hair) #%d",
                    players[i].id,it->second);
            // if not unique hair, remember that for later restoring
            //if (!(IsSpecialHair(&players[i]))) {
            //    _non_unique_hair.push_back(i);
            //}
            // set new hair
            SetSpecialHair(&players[i]);
        }

        if (writeList && players[i].name[0]!='\0') {
            string name(players[i].name);
            mm.insert(pair<string,DWORD>(name,players[i].id));
        }

        WORD faceId = GetFaceId(players[i].faceHairBits);
        WORD hairId = GetHairId(players[i].faceHairBits);
        if (IsSpecialFace(&players[i])) {
            if (faceId < minFaceId) minFaceId = faceId;
            if (faceId > maxFaceId) maxFaceId = faceId;
            facesUsed.insert(pair<WORD,bool>(faceId,true));
        }
        if (IsSpecialHair(&players[i])) {
            if (hairId < minHairId) minHairId = hairId;
            if (hairId > maxHairId) maxHairId = hairId;
            hairsUsed.insert(pair<WORD,bool>(hairId,true));
        }
        
        LOG(L"Player (%d): faceHairBits: %08x", i, players[i].faceHairBits);

        int faceBin = GetFaceBin(&players[i]);
        int hairBin = GetHairBin(&players[i]);
        wchar_t* nameBuf = Utf8::utf8ToUnicode((BYTE*)players[i].name);
        LOG(L"Player (%d): id=%d (id_again=%d): %s (f:%d, h:%d), index=%04x", 
                i, players[i].id, players[i].id_again, nameBuf,
                faceBin, hairBin, players[i].index);
        Utf8::free(nameBuf);

        //if (players[i].index != 0) {
        //    indexTaken = true;
        //}
    }

    //LOG(L"indexTaken=%d", indexTaken);
    LOG(L"face id range: [%d,%d]", minFaceId, maxFaceId);
    LOG(L"hair id range: [%d,%d]", minHairId, maxHairId);
    LOG(L"number of faces used: %d", facesUsed.size());
    LOG(L"number of hairs used: %d", hairsUsed.size());

    if (writeList)
    {
        // write out playerlist.txt
        wstring plist(getPesInfo()->myDir);
        plist += L"\\playerlist.txt";
        FILE* f = _wfopen(plist.c_str(),L"wt");
        if (f)
        {
            for (multimap<string,DWORD>::iterator it = mm.begin();
                    it != mm.end();
                    it++)
                fprintf(f,"%7d : %s\n",it->second,it->first.c_str());
            fclose(f);
        }
    }

    LOG(L"fservCopyPlayerData() done: players updated.");
}

void fservAtUseFaceHairCallPoint()
{
    __asm {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push eax  // param: dest
        push ecx  // param: src
        call fservAtUseFaceHair
        add esp,0x08 // pop params
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        xor word ptr ds:[eax+0x3c],dx  // modified code
        movzx edx,byte ptr ds:[ecx+8]   // ...
        retn
    }
}

void fservAtResetHairCallPoint()
{
    __asm {
        pushfd
        push ebp
        push eax
        push ebx
        push edx
        push esi
        push edi
        mov ecx,0x10
        push ecx  // param: dword count
        push edi  // param: dest
        push esi  // param: src
        call fservAtResetHair
        add esp,0x0c // pop params
        pop edi
        add edi,0x40
        pop esi
        add esi,0x40
        mov ecx,0
        pop edx
        pop ebx
        pop eax
        pop ebp
        popfd
        retn
    }
}

void fservAtSquadListCallPoint()
{
    __asm {
        pushfd
        push ebp
        push eax
        push ebx
        push edx
        push esi
        push edi
        mov ecx,0x10
        push ecx  // param: dword count
        push edi  // param: dest
        push esi  // param: src
        call fservAtSquadList
        add esp,0x0c // pop params
        pop edi
        add edi,0x40
        pop esi
        add esi,0x40
        mov ecx,0
        pop edx
        pop ebx
        pop eax
        pop ebp
        popfd
        retn
    }
}

void fservAtSetDefaultCallPoint()
{
    __asm {
        pushfd
        push ebp
        push eax
        push ebx
        push edx
        push esi
        push edi
        mov ecx,0x1b
        push ecx  // param: dword count
        push edi  // param: dest
        push esi  // param: src
        call fservAtSetDefault
        add esp,0x0c // pop params
        pop edi
        add edi,0x6c
        pop esi
        add esi,0x6c
        mov ecx,0
        pop edx
        pop ebx
        pop eax
        pop ebp
        popfd
        retn
    }
}

void fservAtUseFaceHair(DWORD src, PLAYER_DETAILS* dest)
{
    WORD faceId = *(WORD*)(src+0x0c);
    WORD hairId = *(WORD*)(src+4);

    if (faceId >= FIRST_XFACE_ID) {
        dest->index = (faceId - FIRST_XFACE_ID)/2;
    }
    else if (hairId >= FIRST_XHAIR_ID) {
        dest->index = (hairId - FIRST_XHAIR_ID)/2;
    }
}

void fservAtResetHair(PLAYER_DETAILS* src, PLAYER_DETAILS* dest, 
                      DWORD numDwords)
{
    //LOG(L"src/dest: (%02x,%02x,%08x)/(%02x,%02x,%08x)", 
    //    src->specialHair, src->specialFace,
    //    src->faceHairBits, dest->specialHair,
    //    dest->specialFace, dest->faceHairBits);

    // enforce special hair bit, if needed
    if (src->index != 0) {
        if (_fast_bin_table[src->index*2 + 1]) {
            //LOG(L"setting special hair bit");
            src->specialHair |= SPECIAL_HAIR;
        }
        if (_fast_bin_table[src->index*2]) {
            //LOG(L"setting special face bit");
            src->specialFace |= SPECIAL_FACE;
        }
    }

    // execute game logic
    memcpy(dest, src, numDwords*sizeof(DWORD));


    //LOG(L"dest now: (%02x,%02x,%08x)",
    //    dest->specialHair, dest->specialFace, dest->faceHairBits);
}

void fservAtSquadList(PLAYER_DETAILS* src, PLAYER_DETAILS* dest, 
                      DWORD numDwords)
{
    LOG(L"SQUAD src/dest: (%02x,%02x,%08x)/(%02x,%02x,%08x)", 
        src->specialHair, src->specialFace,
        src->faceHairBits, dest->specialHair,
        dest->specialFace, dest->faceHairBits);

    // execute game logic
    memcpy(dest, src, numDwords*sizeof(DWORD));
}

void fservAtSetDefault(DWORD srcAddr, DWORD destAddr, DWORD numDwords)
{
    PLAYER_DETAILS* src = (PLAYER_DETAILS*)(srcAddr + 0x2c);
    PLAYER_DETAILS* dest = (PLAYER_DETAILS*)(destAddr + 0x2c);

    LOG(L"SDEF: src/dest: (%02x,%02x,%08x)/(%02x,%02x,%08x)", 
        src->specialHair, src->specialFace,
        src->faceHairBits, dest->specialHair,
        dest->specialFace, dest->faceHairBits);

    // execute game logic
    memcpy(dest, src, numDwords*sizeof(DWORD));

    LOG(L"SDEF: dest now: (%02x,%02x,%08x)",
        dest->specialHair, dest->specialFace, dest->faceHairBits);
}

void fservAtGetFaceBinCallPoint()
{
    __asm {
        pushfd
        push ebp
        push ebx
        push ecx
        push edx
        push esi
        push edi
        movzx eax, word ptr ds:[ebx+0x0c]
        push eax // param: face id
        call fservGetFaceBin
        add esp,0x04     // pop parameters
        cmp eax,FIRST_FACE_SLOT
        jae fsface
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop ebp
        popfd
        mov ecx,0x5dc
        retn
fsface: pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop ebp
        popfd
        add eax,0x0c000000
        push eax
        mov eax,_gotFaceBin
        mov [esp+4],eax
        pop eax
        retn
    }
}

void fservAtGetHairBinCallPoint()
{
    __asm {
        pushfd
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push edi
        push esi // param: hair id
        call fservGetHairBin
        add esp,0x04     // pop parameters
        mov esi,eax
        cmp esi,FIRST_FACE_SLOT
        jae fshair
        pop edi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        mov edi,0x5dc
        retn
fshair: pop edi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        add esi,0x0c000000
        mov edi,[esp+4] 
        add esp,4
        push eax
        mov eax,_gotHairBin
        mov [esp+4],eax
        pop eax
        retn
    }
}

void fservAtFaceHairCallPoint()
{
    __asm {
        pushfd 
        push ebp
        push eax
        push ebx
        push ecx
        push edx
        push esi
        push edi
        push esi // param: src struct
        push ecx // param: dest struct
        call fservAtFaceHair
        add esp,0x08     // pop parameters
        pop edi
        pop esi
        pop edx
        pop ecx
        pop ebx
        pop eax
        pop ebp
        popfd
        movzx eax,byte ptr ds:[esi+8] // execute replaced code
        and al,0x7f                   // ...
        retn
    }
}

KEXPORT void fservAtFaceHair(DWORD dest, PLAYER_DETAILS* src)
{
    //static WORD lastSrcIndex = 0;
    WORD index = src->index;
    //if (index == 0) {
    //    index = lastSrcIndex;
    //} 
    //else {
    //    lastSrcIndex = index;
    //}

    WORD* pFace = (WORD*)(dest+0x0c);
    WORD* pHair = (WORD*)(dest+4);

    if (index == 0) {
        //LOG(L"(index=0) src->faceHairBits == %08x", src->faceHairBits);
        //LOG(L"(index=0) src->specialFace == %02x", src->specialFace);
        //LOG(L"(index=0) src->specialHair == %02x", src->specialHair);

        //if (src->faceHairBits == 0xc3920726) {
        //    __asm int 3;
        //}
    }

    if (index != 0) {
        // check corresponding slots
        WORD faceBin = FIRST_FACE_SLOT + index*2;
        WORD hairBin = faceBin + 1;

        if (IsSpecialFaceByte(src->specialFace)) {
            if (_fast_bin_table[faceBin - FIRST_FACE_SLOT]) {
                // face slot is assigned 
                *pFace = faceBin - FIRST_FACE_SLOT + FIRST_XFACE_ID;
                //LOG(L"index = %04x", index);
                //LOG(L"*pFace is now: %04x (bin=%d)", *pFace, faceBin);

                //DWORD id = faceBin - FIRST_FACE_SLOT + FIRST_XFACE_ID;
                //src->faceHairBits &= 0xc0007fff;
                //src->faceHairBits |= (id << 15) & 0x3fff8000;
            }
        }
        if (IsSpecialHairByte(src->specialHair)) {
            if (_fast_bin_table[hairBin - FIRST_FACE_SLOT]) {
                // hair slot is assigned
                *pHair = hairBin - FIRST_FACE_SLOT + FIRST_XHAIR_ID;
                //LOG(L"index = %04x (src=%08x, dest=%08x)", 
                //    index, (DWORD)src, (DWORD)dest);
                //LOG(L"*pHair is now: %04x (bin=%d)", *pHair, hairBin);

                //DWORD id = hairBin - FIRST_FACE_SLOT + FIRST_XHAIR_ID;
                //src->faceHairBits &= 0xffff8000;
                //src->faceHairBits |= id & 0x00007fff;

                //if (hairBin == 20021) {
                //    __asm int 3;
                //}
            }
        }
    }
}

/**
 */
bool OpenFileIfExists(const wchar_t* filename, HANDLE& handle, DWORD& size)
{
    TRACE(L"OpenFileIfExists:: %s", filename);
    handle = CreateFile(filename,           // file to open
                       GENERIC_READ,          // open for reading
                       FILE_SHARE_READ,       // share for reading
                       NULL,                  // default security
                       OPEN_EXISTING,         // existing file only
                       FILE_ATTRIBUTE_NORMAL | CREATE_FLAGS, // normal file
                       NULL);                 // no attr. template

    if (handle != INVALID_HANDLE_VALUE)
    {
        size = GetFileSize(handle,NULL);
        return true;
    }
    return false;
}

/**
 * AFSIO callback
 */
bool fservGetFileInfo(DWORD afsId, DWORD binId, HANDLE& hfile, DWORD& fsize)
{
    //if (afsId == 0x0c)
    //    LOG(L"Handling BIN: (%02x,%d)", afsId, binId);
    if (afsId != 0x0c || binId < FIRST_FACE_SLOT || binId >= NUM_SLOTS)
        return false;

    wchar_t filename[1024] = {0};
    wstring* pws = _fast_bin_table[binId - FIRST_FACE_SLOT];
    if (pws) 
    {
        LOG(L"Loading face/hair BIN: %d", binId);
        swprintf(filename,L"%sGDB\\faces\\%s", getPesInfo()->gdbDir,
                pws->c_str());
        return OpenFileIfExists(filename, hfile, fsize);
    }
    return false;
}

/**
 * write data callback
 */
void fservWriteEditData(LPCVOID data, DWORD size)
{
    LOG(L"Restoring face/hair settings");

    // restore face/hair settings
    PLAYER_INFO* players = (PLAYER_INFO*)((BYTE*)data + 0x1a0);
    hash_map<DWORD,player_facehair_t>::iterator it;
    for (it = _saved_facehair.begin(); it != _saved_facehair.end(); it++) {
        players[it->first].specialHair = it->second.specialHair;
        players[it->first].specialFace = it->second.specialFace;
        players[it->first].index = 0;
        /*
        if (!IsSpecialHair(&players[it->first])) {
            // if edited hair --> allow that to be saved
            DWORD justHair = (players[it->first].faceHairBits) & 0x7ff;
            players[it->first].faceHairBits = it->second.faceHairBits;
            players[it->first].faceHairBits &= 0xfffff800;
            players[it->first].faceHairBits |= justHair;
            players[it->first].specialHair &= ~SPECIAL_HAIR;
        }
        else {
            players[it->first].faceHairBits = it->second.faceHairBits;
        }
        */
    }
}

void fservReadReplayData(LPCVOID data, DWORD size)
{
    LOG(L"Reading replay data:");
    REPLAY_DATA* replay = (REPLAY_DATA*)data;
    if (memcmp(replay->ksSignature, REPLAY_SIG, 
                sizeof(replay->ksSignature))==0) {
        // check faceId/hairId for players
        // if we have the extended slots mapped, then ok
        // --> otherwise, clear the corresponding bits
        for (int i=0; i<22; i++) {
            // debug
            wchar_t* wname = Utf8::utf8ToUnicode(
                (BYTE*)(replay->payload.players[i].name));
            LOG(L"Player #%02d: %s", i, wname);
            Utf8::free(wname);

            /*
            DWORD* pFaceHairBits = &replay->payload.players[i].faceHairBits;

            int faceId = GetFaceIdRaw(*pFaceHairBits);
            if (faceId >= FIRST_XFACE_ID) {
                if (!_fast_bin_table[faceId - FIRST_XFACE_ID]) {
                    // do not currently have any players mapped
                    // to that slot ==> clear extended bits, use maneken
                    *pFaceHairBits = *pFaceHairBits & CLEAR_EXTENDED_FACE_BITS;
                    SetSpecialFaceBits(pFaceHairBits, MANEKEN_ID);
                    LOG(L"nothing mapped to slot: %d. Skipping", 
                        faceId + FIRST_FACE_SLOT - FIRST_XFACE_ID);
                }
            }
            else {
                // normal bin
                replay->payload.players[i].faceHairBits 
                    &= CLEAR_EXTENDED_FACE_BITS;
            }
            int hairId = GetHairIdRaw(*pFaceHairBits);
            if (hairId >= FIRST_XHAIR_ID) {
                if (!_fast_bin_table[hairId - FIRST_XHAIR_ID]) {
                    // do not currently have any players mapped
                    // to that slot ==> clear extended bits, use maneken
                    *pFaceHairBits = *pFaceHairBits & CLEAR_EXTENDED_HAIR_BITS;
                    SetSpecialHairBits(pFaceHairBits, MANEKEN_ID);
                    LOG(L"nothing mapped to slot: %d. Skipping", 
                        hairId + FIRST_FACE_SLOT - FIRST_XHAIR_ID);
                }
            }
            else {
                // normal bin
                replay->payload.players[i].faceHairBits 
                    &= CLEAR_EXTENDED_HAIR_BITS;
            }
            */
        }
    }
    else {
        // non-fs replay
        // clear extended bits in all players
        /*
        for (int i=0; i<22; i++) {
            replay->payload.players[i].faceHairBits 
                &= CLEAR_EXTENDED_BITS;
        }
        */
    }
}

void fservWriteReplayData(LPCVOID data, DWORD size)
{
    LOG(L"Writing replay data:");
    REPLAY_DATA* replay = (REPLAY_DATA*)data;
    // set kitserver signature
    memcpy(replay->ksSignature, REPLAY_SIG, sizeof(replay->ksSignature));
}

KEXPORT DWORD fservGetFaceBin(DWORD faceId)
{
    if (faceId < FIRST_XFACE_ID) {
        //if (faceId == 0x724) {
        //    LOG(L"fservGetFaceBin: faceId=%d (hex:%04x) --> result=%d", 
        //        faceId, faceId, faceId);
        //}
        return faceId;
    }
    DWORD binId = faceId - FIRST_XFACE_ID + FIRST_FACE_SLOT;
    if (_fast_bin_table[binId - FIRST_FACE_SLOT]) {
        //LOG(L"fservGetFaceBin: faceId=%d (hex:%04x) --> result=%d", 
        //    faceId, faceId, binId);
        return binId;
    }
    LOG(L"_fast_bin_table[%d - %d] is NULL!", binId, FIRST_FACE_SLOT);
    LOG(L"faceId & 0x7ff = %d", (faceId & 0x7ff));
    return MANEKEN_ID; //faceId & 0x7ff;
}

KEXPORT DWORD fservGetHairBin(DWORD hairId)
{
    if (hairId < FIRST_XHAIR_ID) {
        //if (hairId == 0x726) {
        //    LOG(L"fservGetHairBin: hairId=%d (hex:%04x) --> result=%d", 
        //        hairId, hairId, hairId);
        //}
        return hairId;
    }
    DWORD binId = hairId - FIRST_XHAIR_ID + FIRST_FACE_SLOT;
    if (_fast_bin_table[binId - FIRST_FACE_SLOT]) {
        //LOG(L"fservGetHairBin: hairId=%d (hex:%04x) --> result=%d", 
        //    hairId, hairId, binId);
        return binId;
    }
    LOG(L"_fast_bin_table[%d - %d] is NULL!", binId, FIRST_FACE_SLOT);
    LOG(L"hairId & 0x7ff = %d", (hairId & 0x7ff));
    return MANEKEN_ID; //hairId & 0x7ff;
}

void fservReadBalData(LPCVOID data, DWORD size)
{
    BAL* bal = (BAL*)data;
    wchar_t* wideName = Utf8::utf8ToUnicode((BYTE*)bal->bal1.player.name);
    LOG(L"BAL player: id=%d, name={%s}, faceHairBits=%08x, "
        L"sHair=%02x, sFace=%02x",
            bal->bal1.player.id, wideName, 
            bal->bal2.playerDetails.faceHairBits,
            bal->bal2.playerDetails.specialHair, 
            bal->bal2.playerDetails.specialFace);

    // Load face/hair, if specified (TODO)
    // TEST:
    //if (bal->bal1.player.id == 0x010000a0) {
    //    bal->bal2.playerDetails.faceHairBits = 0x8af815f2;
    //    bal->bal2.playerDetails.specialFace |= SPECIAL_FACE;
    //    bal->bal2.playerDetails.specialHair |= SPECIAL_HAIR;
    //}

    LOG(L"BAL player: faceHairBits now: %08x", 
            bal->bal2.playerDetails.faceHairBits);
    Utf8::free(wideName);
}

