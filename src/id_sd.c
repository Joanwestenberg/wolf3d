// ID_SD.C - Sound Manager (macOS/SDL2 port)
//
// Provides the 70Hz timer via SDL_AddTimer (critical for game timing),
// and stubs all audio functions. A proper OPL2 emulator can be added later.

#include "id_heads.h"

//==========================================================================
// Globals
//==========================================================================

boolean  AdLibPresent;
boolean  SoundSourcePresent;
boolean  SoundBlasterPresent;
boolean  NeedsMusic;
boolean  SoundPositioned;
SDMode   SoundMode;
SDSMode  DigiMode;
SMMode   MusicMode;
boolean  DigiPlaying;
int      DigiMap[LASTSOUND];
longword TimeCount;

static void (*SoundUserHook)(void);
static boolean SD_Started;
static SDL_TimerID sdl_timer_id;
static word SoundNumber;
static word DigiNumber;

//==========================================================================
// 70Hz Timer Callback
//==========================================================================

static Uint32 SD_TimerCallback(Uint32 interval, void *param)
{
	(void)param;
	TimeCount++;
	if (SoundUserHook)
		SoundUserHook();
	return interval;
}

//==========================================================================

void alOut(byte n, byte b)
{
	// No-op without OPL emulator
	(void)n;
	(void)b;
}

//==========================================================================

void SD_Startup(void)
{
	int i;

	if (SD_Started)
		return;

	TimeCount = 0;
	SoundMode = sdm_Off;
	MusicMode = smm_Off;
	DigiMode = sds_Off;
	DigiPlaying = false;
	SoundNumber = 0;
	DigiNumber = 0;
	SoundUserHook = NULL;
	SoundPositioned = false;
	NeedsMusic = false;

	// Report AdLib as present so caching code loads music
	AdLibPresent = true;
	SoundBlasterPresent = true;
	SoundSourcePresent = false;

	for (i = 0; i < LASTSOUND; i++)
		DigiMap[i] = -1;

	// Start the 70Hz timer
	sdl_timer_id = SDL_AddTimer(1000 / 70, SD_TimerCallback, NULL);
	if (!sdl_timer_id)
		Quit("SD_Startup: SDL_AddTimer failed");

	SD_Started = true;
}

void SD_Shutdown(void)
{
	if (!SD_Started)
		return;

	if (sdl_timer_id)
	{
		SDL_RemoveTimer(sdl_timer_id);
		sdl_timer_id = 0;
	}

	SD_Started = false;
}

void SD_Default(boolean gotit, SDMode sd, SMMode sm)
{
	(void)gotit;
	SoundMode = sd;
	MusicMode = sm;
}

boolean SD_PlaySound(soundnames sound)
{
	SoundNumber = sound;
	SoundPositioned = false;
	return false;
}

void SD_PositionSound(int leftvol, int rightvol)
{
	(void)leftvol;
	(void)rightvol;
	SoundPositioned = true;
}

void SD_SetPosition(int leftvol, int rightvol)
{
	(void)leftvol;
	(void)rightvol;
}

void SD_StopSound(void)
{
	SoundNumber = 0;
}

void SD_WaitSoundDone(void)
{
}

word SD_SoundPlaying(void)
{
	return 0;
}

boolean SD_SetSoundMode(SDMode mode)
{
	SoundMode = mode;
	return true;
}

boolean SD_SetMusicMode(SMMode mode)
{
	MusicMode = mode;
	return true;
}

void SD_StartMusic(MusicGroup *music)
{
	(void)music;
}

void SD_MusicOn(void)
{
}

void SD_MusicOff(void)
{
}

void SD_FadeOutMusic(void)
{
	MusicMode = smm_Off;
}

boolean SD_MusicPlaying(void)
{
	return false;
}

void SD_SetUserHook(void (*hook)(void))
{
	SoundUserHook = hook;
}

void SD_SetDigiDevice(SDSMode mode)
{
	DigiMode = mode;
}

void SD_PlayDigitized(word which, int leftpos, int rightpos)
{
	(void)which;
	(void)leftpos;
	(void)rightpos;
	DigiPlaying = true;
	DigiNumber = which;
}

void SD_StopDigitized(void)
{
	DigiPlaying = false;
	DigiNumber = 0;
}

void SD_Poll(void)
{
	// No-op - audio mixing would happen here with a real implementation
}
