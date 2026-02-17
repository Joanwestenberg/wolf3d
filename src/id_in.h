//
// ID Engine
// ID_IN.h - Header file for Input Manager
// macOS/SDL2 port
//

#ifndef __ID_IN__
#define __ID_IN__

#define MaxPlayers    1
#define MaxKbds       2
#define MaxJoys       2
#define NumCodes      128

typedef byte ScanCode;
#define sc_None         0
#define sc_Bad          0xff
#define sc_Return       0x1c
#define sc_Enter        sc_Return
#define sc_Escape       0x01
#define sc_Space        0x39
#define sc_BackSpace    0x0e
#define sc_Tab          0x0f
#define sc_Alt          0x38
#define sc_Control      0x1d
#define sc_CapsLock     0x3a
#define sc_LShift       0x2a
#define sc_RShift       0x36
#define sc_UpArrow      0x48
#define sc_DownArrow    0x50
#define sc_LeftArrow    0x4b
#define sc_RightArrow   0x4d
#define sc_Insert       0x52
#define sc_Delete       0x53
#define sc_Home         0x47
#define sc_End          0x4f
#define sc_PgUp         0x49
#define sc_PgDn         0x51
#define sc_F1           0x3b
#define sc_F2           0x3c
#define sc_F3           0x3d
#define sc_F4           0x3e
#define sc_F5           0x3f
#define sc_F6           0x40
#define sc_F7           0x41
#define sc_F8           0x42
#define sc_F9           0x43
#define sc_F10          0x44
#define sc_F11          0x57
#define sc_F12          0x59

#define sc_1            0x02
#define sc_2            0x03
#define sc_3            0x04
#define sc_4            0x05
#define sc_5            0x06
#define sc_6            0x07
#define sc_7            0x08
#define sc_8            0x09
#define sc_9            0x0a
#define sc_0            0x0b

#define sc_A            0x1e
#define sc_B            0x30
#define sc_C            0x2e
#define sc_D            0x20
#define sc_E            0x12
#define sc_F            0x21
#define sc_G            0x22
#define sc_H            0x23
#define sc_I            0x17
#define sc_J            0x24
#define sc_K            0x25
#define sc_L            0x26
#define sc_M            0x32
#define sc_N            0x31
#define sc_O            0x18
#define sc_P            0x19
#define sc_Q            0x10
#define sc_R            0x13
#define sc_S            0x1f
#define sc_T            0x14
#define sc_U            0x16
#define sc_V            0x2f
#define sc_W            0x11
#define sc_X            0x2d
#define sc_Y            0x15
#define sc_Z            0x2c

#define key_None        0

typedef enum {
	demo_Off, demo_Record, demo_Playback, demo_PlayDone
} Demo;

typedef enum {
	ctrl_Keyboard,
	ctrl_Keyboard1 = ctrl_Keyboard, ctrl_Keyboard2,
	ctrl_Joystick, ctrl_Joystick1 = ctrl_Joystick, ctrl_Joystick2,
	ctrl_Mouse
} ControlType;

typedef enum {
	motion_Left = -1, motion_Up = -1,
	motion_None = 0,
	motion_Right = 1, motion_Down = 1
} Motion;

typedef enum {
	dir_North, dir_NorthEast,
	dir_East, dir_SouthEast,
	dir_South, dir_SouthWest,
	dir_West, dir_NorthWest,
	dir_None
} Direction;

typedef struct {
	boolean button0, button1, button2, button3;
	int x, y;
	Motion xaxis, yaxis;
	Direction dir;
} ControlInfo;

typedef ControlInfo CursorInfo;

typedef struct {
	ScanCode button0, button1,
	         upleft, up, upright,
	         left, right,
	         downleft, down, downright;
} KeyboardDef;

typedef struct {
	word joyMinX, joyMinY,
	     threshMinX, threshMinY,
	     threshMaxX, threshMaxY,
	     joyMaxX, joyMaxY;
	word joyMultXL, joyMultYL,
	     joyMultXH, joyMultYH;
} JoystickDef;

// Global variables
extern boolean    Keyboard[NumCodes];
extern boolean    Paused;
extern char       LastASCII;
extern ScanCode   LastScan;

extern KeyboardDef KbdDefs;
extern JoystickDef JoyDefs[MaxJoys];
extern ControlType Controls[MaxPlayers];

extern Demo       DemoMode;
extern byte      *DemoBuffer;
extern word       DemoOffset, DemoSize;

extern boolean    MousePresent;
extern boolean    JoysPresent[MaxJoys];
extern boolean    CapsLock;

// Function prototypes
void IN_Startup(void);
void IN_Shutdown(void);

void IN_Default(boolean gotit, ControlType in);
void IN_SetKeyHook(void (*hook)());

void IN_ClearKeysDown(void);
void IN_ReadControl(int player, ControlInfo *info);
void IN_ReadCursor(CursorInfo *info);

ScanCode IN_WaitForKey(void);
char     IN_WaitForASCII(void);

void IN_StartAck(void);
boolean IN_CheckAck(void);
void IN_Ack(void);
void IN_AckBack(void);
boolean IN_UserInput(longword delay);

byte IN_MouseButtons(void);
byte IN_JoyButtons(void);

void IN_SetControlType(int player, ControlType type);

byte *IN_GetScanName(ScanCode scan);

void IN_GetJoyAbs(word joy, word *xp, word *yp);
void IN_SetupJoy(word joy, word minx, word maxx, word miny, word maxy);
word IN_GetJoyButtonsDB(word joy);

void IN_StopDemo(void);
void IN_FreeDemoBuffer(void);

// SDL2-specific
void IN_PumpEvents(void);

// Internal functions used by other modules
void INL_GetJoyDelta(word joy, int *dx, int *dy);
word INL_GetJoyButtons(word joy);
void INL_GetMouseDelta(int *x, int *y);

#endif
