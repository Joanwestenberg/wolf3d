//
// ID Engine
// ID_SD.h - Sound Manager Header
// macOS/SDL2 port
//

#ifndef __ID_SD__
#define __ID_SD__

void alOut(byte n, byte b);

#define TickBase 70

typedef enum {
	sdm_Off,
	sdm_PC, sdm_AdLib,
} SDMode;
typedef enum {
	smm_Off, smm_AdLib
} SMMode;
typedef enum {
	sds_Off, sds_PC, sds_SoundSource, sds_SoundBlaster
} SDSMode;

typedef struct {
	longword length;
	word     priority;
} SoundCommon;

typedef struct {
	SoundCommon common;
	byte        data[1];
} PCSound;

typedef struct {
	SoundCommon common;
	word        hertz;
	byte        bits,
	            reference,
	            data[1];
} SampledSound;

// AdLib register addresses (kept for music sequencer)
#define alChar      0x20
#define alScale     0x40
#define alAttack    0x60
#define alSus       0x80
#define alWave      0xe0
#define alFreqL     0xa0
#define alFreqH     0xb0
#define alFeedCon   0xc0
#define alEffects   0xbd

typedef struct {
	byte mChar, cChar,
	     mScale, cScale,
	     mAttack, cAttack,
	     mSus, cSus,
	     mWave, cWave,
	     nConn,
	     voice,
	     mode,
	     unused[3];
} Instrument;

typedef struct {
	SoundCommon common;
	Instrument  inst;
	byte        block,
	            data[1];
} AdLibSound;

#define sqMaxTracks 10
#define sqMaxMoods  1

typedef struct {
	word length,
	     values[1];
} MusicGroup;

// Global variables
extern boolean  AdLibPresent,
                SoundSourcePresent,
                SoundBlasterPresent,
                NeedsMusic,
                SoundPositioned;
extern SDMode   SoundMode;
extern SDSMode  DigiMode;
extern SMMode   MusicMode;
extern boolean  DigiPlaying;
extern int      DigiMap[];
extern longword TimeCount;

// Function prototypes
extern void    SD_Startup(void),
               SD_Shutdown(void),
               SD_Default(boolean gotit, SDMode sd, SMMode sm),
               SD_PositionSound(int leftvol, int rightvol);
extern boolean SD_PlaySound(soundnames sound);
extern void    SD_SetPosition(int leftvol, int rightvol),
               SD_StopSound(void),
               SD_WaitSoundDone(void),
               SD_StartMusic(MusicGroup *music),
               SD_MusicOn(void),
               SD_MusicOff(void),
               SD_FadeOutMusic(void),
               SD_SetUserHook(void (*hook)(void));
extern boolean SD_MusicPlaying(void),
               SD_SetSoundMode(SDMode mode),
               SD_SetMusicMode(SMMode mode);
extern word    SD_SoundPlaying(void);

extern void    SD_SetDigiDevice(SDSMode),
               SD_PlayDigitized(word which, int leftpos, int rightpos),
               SD_StopDigitized(void),
               SD_Poll(void);

#endif
