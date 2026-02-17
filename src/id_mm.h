// ID_MM.H - Memory manager header (macOS/SDL2 port)
// Replaces DOS segment-based EMS/XMS memory management with malloc/free

#ifndef __ID_MM_H__
#define __ID_MM_H__

#define BUFFERSIZE 0x1000 // miscellaneous, always available buffer

//==========================================================================

typedef struct
{
	long nearheap, farheap, EMSmem, XMSmem, mainmem;
} mminfotype;

//==========================================================================

extern mminfotype mminfo;
extern memptr     bufferseg;
extern boolean    mmerror;

extern void (*beforesort)(void);
extern void (*aftersort)(void);

//==========================================================================

void MM_Startup(void);
void MM_Shutdown(void);
void MM_MapEMS(void);

void MM_GetPtr(memptr *baseptr, unsigned long size);
void MM_FreePtr(memptr *baseptr);

void MM_SetPurge(memptr *baseptr, int purge);
void MM_SetLock(memptr *baseptr, boolean locked);
void MM_SortMem(void);

void MM_ShowMemory(void);

long MM_UnusedMemory(void);
long MM_TotalFree(void);

void MM_BombOnError(boolean bomb);

void MML_UseSpace(unsigned segstart, unsigned seglength);

#endif
