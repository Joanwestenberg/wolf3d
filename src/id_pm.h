// ID_PM.H - Page manager header (macOS/SDL2 port)

#ifndef __ID_PM_H__
#define __ID_PM_H__

#define PMPageSize 4096

typedef enum
{
	pml_Unlocked,
	pml_Locked
} PMLockType;

extern word ChunksInFile,
            PMSpriteStart, PMSoundStart;

#define PM_GetSoundPage(v)  PM_GetPage(PMSoundStart + (v))
#define PM_GetSpritePage(v) PM_GetPage(PMSpriteStart + (v))

#define PM_LockMainMem()    PM_SetMainMemPurge(0)
#define PM_UnlockMainMem()  PM_SetMainMemPurge(3)

extern char PageFileName[13];

extern void   PM_Startup(void),
              PM_Shutdown(void),
              PM_Reset(void),
              PM_Preload(boolean (*update)(word current, word total)),
              PM_NextFrame(void),
              PM_SetPageLock(int pagenum, PMLockType lock),
              PM_SetMainPurge(int level),
              PM_CheckMainMem(void);
extern memptr PM_GetPageAddress(int pagenum),
              PM_GetPage(int pagenum);

void PM_SetMainMemPurge(int level);

#endif
