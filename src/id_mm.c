// ID_MM.C - Memory manager (macOS/SDL2 port)
//
// Replaces the original DOS segment-based memory manager (EMS/XMS/near/far
// heap) with simple malloc/free wrappers.

#include "id_heads.h"

//==========================================================================

mminfotype mminfo;
memptr     bufferseg;
boolean    mmerror;

void (*beforesort)(void);
void (*aftersort)(void);

static boolean mmstarted;
static boolean bombonerror;

//==========================================================================

void MM_Startup(void)
{
	if (mmstarted)
		MM_Shutdown();

	mmstarted   = true;
	bombonerror = true;
	mmerror     = false;

	mminfo.nearheap = 64L * 1024L;
	mminfo.farheap  = 64L * 1024L * 1024L;
	mminfo.EMSmem   = 0;
	mminfo.XMSmem   = 0;
	mminfo.mainmem  = mminfo.nearheap + mminfo.farheap;

	bufferseg = malloc(BUFFERSIZE);
	if (!bufferseg)
		Quit("MM_Startup: Failed to allocate misc buffer");
}

void MM_Shutdown(void)
{
	if (!mmstarted)
		return;

	if (bufferseg)
	{
		free(bufferseg);
		bufferseg = NULL;
	}

	mmstarted = false;
}

void MM_GetPtr(memptr *baseptr, unsigned long size)
{
	mmerror = false;

	*baseptr = malloc(size);
	if (!*baseptr)
	{
		if (bombonerror)
			Quit("MM_GetPtr: Out of memory!");
		else
			mmerror = true;
	}
}

void MM_FreePtr(memptr *baseptr)
{
	if (*baseptr)
	{
		free(*baseptr);
		*baseptr = NULL;
	}
}

void MM_SetPurge(memptr *baseptr, int purge)
{
	(void)baseptr;
	(void)purge;
}

void MM_SetLock(memptr *baseptr, boolean locked)
{
	(void)baseptr;
	(void)locked;
}

void MM_SortMem(void) { }
void MM_ShowMemory(void) { }

long MM_UnusedMemory(void) { return 64L * 1024L * 1024L; }
long MM_TotalFree(void) { return 64L * 1024L * 1024L; }

void MM_BombOnError(boolean bomb) { bombonerror = bomb; }

void MML_UseSpace(unsigned segstart, unsigned seglength)
{
	(void)segstart;
	(void)seglength;
}

void MM_MapEMS(void) { }
