// ID_IN.C - Input Manager (macOS/SDL2 port)

#include "id_heads.h"

//==========================================================================
// Globals
//==========================================================================

boolean    Keyboard[NumCodes];
boolean    Paused;
char       LastASCII;
ScanCode   LastScan;

KeyboardDef KbdDefs = {
	sc_Control,     // button0
	sc_Alt,         // button1
	sc_Home,        // upleft
	sc_UpArrow,     // up
	sc_PgUp,        // upright
	sc_LeftArrow,   // left
	sc_RightArrow,  // right
	sc_End,         // downleft
	sc_DownArrow,   // down
	sc_PgDn         // downright
};

JoystickDef JoyDefs[MaxJoys];
ControlType Controls[MaxPlayers];

Demo       DemoMode;
byte      *DemoBuffer;
word       DemoOffset, DemoSize;

boolean    MousePresent;
boolean    JoysPresent[MaxJoys];
boolean    CapsLock;

static boolean INL_Started;
static void    (*INL_KeyHook)(void);
static boolean btnstate[8];

//==========================================================================
// Direction table for control info
//==========================================================================

static Direction DirTable[] = {
	dir_NorthWest, dir_North,  dir_NorthEast,
	dir_West,      dir_None,   dir_East,
	dir_SouthWest, dir_South,  dir_SouthEast
};

//==========================================================================
// ASCII name tables for scan codes
//==========================================================================

static byte *ScanNames[] = {
	"?","?","1","2","3","4","5","6","7","8","9","0","-","+","?","?",
	"Q","W","E","R","T","Y","U","I","O","P","[","]","|","?","A","S",
	"D","F","G","H","J","K","L",";","\"","?","?","?","Z","X","C","V",
	"B","N","M",",",".","/","?","?","?","?","?","?","?","?","?","?",
	"?","?","?","?","?","?","?","?","\x0f","?","-","\x15","5","\x11","+","?",
	"\x13","?","?","?","?","?","?","?","?","?","?","?","?","?","?","?",
	"?","?","?","?","?","?","?","?","?","?","?","?","?","?","?","?",
	"?","?","?","?","?","?","?","?","?","?","?","?","?","?","?","?"
};

static byte *ExtScanNames[] = {NULL};
static ScanCode ExtScanCodes[] = {0};

static char ASCIINames[] = {
	0,0,'1','2','3','4','5','6','7','8','9','0','-','=',8,9,
	'q','w','e','r','t','y','u','i','o','p','[',']',13,0,'a','s',
	'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
	'b','n','m',',','.','/',0,0,0,' ',0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,'-',0,'5',0,'+',0,
	0,0,0,127,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static char ShiftNames[] = {
	0,0,'!','@','#','$','%','^','&','*','(',')','_','+',8,9,
	'Q','W','E','R','T','Y','U','I','O','P','{','}',13,0,'A','S',
	'D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V',
	'B','N','M','<','>','?',0,0,0,' ',0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,'-',0,'5',0,'+',0,
	0,0,0,127,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

//==========================================================================
// SDL scancode -> DOS scancode mapping
//==========================================================================

static byte sdl_to_dos_scancode(SDL_Scancode sc)
{
	switch (sc)
	{
	case SDL_SCANCODE_ESCAPE:       return sc_Escape;
	case SDL_SCANCODE_1:            return 0x02;
	case SDL_SCANCODE_2:            return 0x03;
	case SDL_SCANCODE_3:            return 0x04;
	case SDL_SCANCODE_4:            return 0x05;
	case SDL_SCANCODE_5:            return 0x06;
	case SDL_SCANCODE_6:            return 0x07;
	case SDL_SCANCODE_7:            return 0x08;
	case SDL_SCANCODE_8:            return 0x09;
	case SDL_SCANCODE_9:            return 0x0a;
	case SDL_SCANCODE_0:            return 0x0b;
	case SDL_SCANCODE_MINUS:        return 0x0c;
	case SDL_SCANCODE_EQUALS:       return 0x0d;
	case SDL_SCANCODE_BACKSPACE:    return sc_BackSpace;
	case SDL_SCANCODE_TAB:          return sc_Tab;
	case SDL_SCANCODE_Q:            return 0x10;
	case SDL_SCANCODE_W:            return 0x11;
	case SDL_SCANCODE_E:            return 0x12;
	case SDL_SCANCODE_R:            return 0x13;
	case SDL_SCANCODE_T:            return 0x14;
	case SDL_SCANCODE_Y:            return 0x15;
	case SDL_SCANCODE_U:            return 0x16;
	case SDL_SCANCODE_I:            return 0x17;
	case SDL_SCANCODE_O:            return 0x18;
	case SDL_SCANCODE_P:            return 0x19;
	case SDL_SCANCODE_LEFTBRACKET:  return 0x1a;
	case SDL_SCANCODE_RIGHTBRACKET: return 0x1b;
	case SDL_SCANCODE_RETURN:       return sc_Return;
	case SDL_SCANCODE_LCTRL:        return sc_Control;
	case SDL_SCANCODE_RCTRL:        return sc_Control;
	case SDL_SCANCODE_A:            return 0x1e;
	case SDL_SCANCODE_S:            return 0x1f;
	case SDL_SCANCODE_D:            return 0x20;
	case SDL_SCANCODE_F:            return 0x21;
	case SDL_SCANCODE_G:            return 0x22;
	case SDL_SCANCODE_H:            return 0x23;
	case SDL_SCANCODE_J:            return 0x24;
	case SDL_SCANCODE_K:            return 0x25;
	case SDL_SCANCODE_L:            return 0x26;
	case SDL_SCANCODE_SEMICOLON:    return 0x27;
	case SDL_SCANCODE_APOSTROPHE:   return 0x28;
	case SDL_SCANCODE_GRAVE:        return 0x29;
	case SDL_SCANCODE_LSHIFT:       return sc_LShift;
	case SDL_SCANCODE_BACKSLASH:    return 0x2b;
	case SDL_SCANCODE_Z:            return 0x2c;
	case SDL_SCANCODE_X:            return 0x2d;
	case SDL_SCANCODE_C:            return 0x2e;
	case SDL_SCANCODE_V:            return 0x2f;
	case SDL_SCANCODE_B:            return 0x30;
	case SDL_SCANCODE_N:            return 0x31;
	case SDL_SCANCODE_M:            return 0x32;
	case SDL_SCANCODE_COMMA:        return 0x33;
	case SDL_SCANCODE_PERIOD:       return 0x34;
	case SDL_SCANCODE_SLASH:        return 0x35;
	case SDL_SCANCODE_RSHIFT:       return sc_RShift;
	case SDL_SCANCODE_LALT:         return sc_Alt;
	case SDL_SCANCODE_RALT:         return sc_Alt;
	case SDL_SCANCODE_SPACE:        return sc_Space;
	case SDL_SCANCODE_CAPSLOCK:     return sc_CapsLock;
	case SDL_SCANCODE_F1:           return sc_F1;
	case SDL_SCANCODE_F2:           return sc_F2;
	case SDL_SCANCODE_F3:           return sc_F3;
	case SDL_SCANCODE_F4:           return sc_F4;
	case SDL_SCANCODE_F5:           return sc_F5;
	case SDL_SCANCODE_F6:           return sc_F6;
	case SDL_SCANCODE_F7:           return sc_F7;
	case SDL_SCANCODE_F8:           return sc_F8;
	case SDL_SCANCODE_F9:           return sc_F9;
	case SDL_SCANCODE_F10:          return sc_F10;
	case SDL_SCANCODE_F11:          return sc_F11;
	case SDL_SCANCODE_F12:          return sc_F12;
	case SDL_SCANCODE_KP_7:
	case SDL_SCANCODE_HOME:         return sc_Home;
	case SDL_SCANCODE_KP_8:
	case SDL_SCANCODE_UP:           return sc_UpArrow;
	case SDL_SCANCODE_KP_9:
	case SDL_SCANCODE_PAGEUP:       return sc_PgUp;
	case SDL_SCANCODE_KP_4:
	case SDL_SCANCODE_LEFT:         return sc_LeftArrow;
	case SDL_SCANCODE_KP_6:
	case SDL_SCANCODE_RIGHT:        return sc_RightArrow;
	case SDL_SCANCODE_KP_1:
	case SDL_SCANCODE_END:          return sc_End;
	case SDL_SCANCODE_KP_2:
	case SDL_SCANCODE_DOWN:         return sc_DownArrow;
	case SDL_SCANCODE_KP_3:
	case SDL_SCANCODE_PAGEDOWN:     return sc_PgDn;
	case SDL_SCANCODE_KP_0:
	case SDL_SCANCODE_INSERT:       return sc_Insert;
	case SDL_SCANCODE_KP_PERIOD:
	case SDL_SCANCODE_DELETE:       return sc_Delete;
	case SDL_SCANCODE_KP_ENTER:     return sc_Return;
	default:                        return sc_None;
	}
}

//==========================================================================
// IN_PumpEvents - Process SDL events
//==========================================================================

void IN_PumpEvents(void)
{
	SDL_Event e;

	while (SDL_PollEvent(&e))
	{
		switch (e.type)
		{
		case SDL_KEYDOWN:
			if (e.key.repeat)
				break;
			{
				byte sc = sdl_to_dos_scancode(e.key.keysym.scancode);

				if (e.key.keysym.scancode == SDL_SCANCODE_PAUSE)
				{
					Paused = true;
					break;
				}

				if (sc != sc_None)
				{
					Keyboard[sc] = true;
					LastScan = sc;

					if (sc == sc_CapsLock)
						CapsLock ^= true;

					{
						char c;
						if (Keyboard[sc_LShift] || Keyboard[sc_RShift])
						{
							c = ShiftNames[sc];
							if ((c >= 'A') && (c <= 'Z') && CapsLock)
								c += 'a' - 'A';
						}
						else
						{
							c = ASCIINames[sc];
							if ((c >= 'a') && (c <= 'z') && CapsLock)
								c -= 'a' - 'A';
						}
						if (c)
							LastASCII = c;
					}
				}

				if (INL_KeyHook)
					INL_KeyHook();
			}
			break;

		case SDL_KEYUP:
			{
				byte sc = sdl_to_dos_scancode(e.key.keysym.scancode);
				if (sc != sc_None)
					Keyboard[sc] = false;
			}
			break;

		case SDL_QUIT:
			Quit(NULL);
			break;
		}
	}
}

//==========================================================================
// Mouse
//==========================================================================

void INL_GetMouseDelta(int *x, int *y)
{
	SDL_GetRelativeMouseState(x, y);
}

static word INL_GetMouseButtons(void)
{
	Uint32 buttons = SDL_GetMouseState(NULL, NULL);
	word result = 0;

	if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT))   result |= 1;
	if (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT))  result |= 2;
	if (buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) result |= 4;

	return result;
}

byte IN_MouseButtons(void)
{
	if (MousePresent)
		return (byte)INL_GetMouseButtons();
	return 0;
}

//==========================================================================
// Joystick (stubs)
//==========================================================================

byte IN_JoyButtons(void) { return 0; }

void INL_GetJoyDelta(word joy, int *dx, int *dy)
{
	(void)joy;
	*dx = 0;
	*dy = 0;
}

void IN_GetJoyAbs(word joy, word *xp, word *yp)
{
	(void)joy;
	*xp = 0;
	*yp = 0;
}

void IN_SetupJoy(word joy, word minx, word maxx, word miny, word maxy)
{
	word d, r;
	JoystickDef *def = &JoyDefs[joy];

	def->joyMinX = minx; def->joyMaxX = maxx;
	r = maxx - minx; d = r / 3;
	def->threshMinX = ((r / 2) - d) + minx;
	def->threshMaxX = ((r / 2) + d) + minx;

	def->joyMinY = miny; def->joyMaxY = maxy;
	r = maxy - miny; d = r / 3;
	def->threshMinY = ((r / 2) - d) + miny;
	def->threshMaxY = ((r / 2) + d) + miny;
}

word IN_GetJoyButtonsDB(word joy) { (void)joy; return 0; }

//==========================================================================
// Startup / Shutdown
//==========================================================================

void IN_Startup(void)
{
	if (INL_Started)
		return;

	INL_KeyHook = NULL;
	IN_ClearKeysDown();

	MousePresent = true;
	JoysPresent[0] = false;
	JoysPresent[1] = false;

	SDL_SetRelativeMouseMode(SDL_TRUE);

	INL_Started = true;
}

void IN_Shutdown(void)
{
	if (!INL_Started)
		return;

	SDL_SetRelativeMouseMode(SDL_FALSE);
	INL_Started = false;
}

void IN_Default(boolean gotit, ControlType in)
{
	if (!gotit
		|| ((in == ctrl_Joystick1) && !JoysPresent[0])
		|| ((in == ctrl_Joystick2) && !JoysPresent[1])
		|| ((in == ctrl_Mouse) && !MousePresent))
		in = ctrl_Keyboard1;
	IN_SetControlType(0, in);
}

void IN_SetKeyHook(void (*hook)())
{
	INL_KeyHook = hook;
}

void IN_ClearKeysDown(void)
{
	LastScan = sc_None;
	LastASCII = key_None;
	memset(Keyboard, 0, sizeof(Keyboard));
}

//==========================================================================
// IN_ReadControl
//==========================================================================

void IN_ReadControl(int player, ControlInfo *info)
{
	boolean       realdelta;
	byte          dbyte;
	word          buttons;
	int           dx, dy;
	Motion        mx, my;
	ControlType   type;
	KeyboardDef   *def;

	dx = dy = 0;
	mx = my = motion_None;
	buttons = 0;

	IN_PumpEvents();

	if (DemoMode == demo_Playback)
	{
		dbyte = DemoBuffer[DemoOffset + 1];
		my = (dbyte & 3) - 1;
		mx = ((dbyte >> 2) & 3) - 1;
		buttons = (dbyte >> 4) & 3;

		if (!(--DemoBuffer[DemoOffset]))
		{
			DemoOffset += 2;
			if (DemoOffset >= DemoSize)
				DemoMode = demo_PlayDone;
		}

		realdelta = false;
	}
	else if (DemoMode == demo_PlayDone)
		Quit("Demo playback exceeded");
	else
	{
		switch (type = Controls[player])
		{
		case ctrl_Keyboard:
		case ctrl_Keyboard2:
			def = &KbdDefs;

			if (Keyboard[def->upleft])     mx = motion_Left,  my = motion_Up;
			else if (Keyboard[def->upright])    mx = motion_Right, my = motion_Up;
			else if (Keyboard[def->downleft])   mx = motion_Left,  my = motion_Down;
			else if (Keyboard[def->downright])  mx = motion_Right, my = motion_Down;

			if (Keyboard[def->up])         my = motion_Up;
			else if (Keyboard[def->down])       my = motion_Down;

			if (Keyboard[def->left])       mx = motion_Left;
			else if (Keyboard[def->right])      mx = motion_Right;

			if (Keyboard[def->button0])    buttons += 1 << 0;
			if (Keyboard[def->button1])    buttons += 1 << 1;
			realdelta = false;
			break;
		case ctrl_Joystick1:
		case ctrl_Joystick2:
			INL_GetJoyDelta(type - ctrl_Joystick, &dx, &dy);
			buttons = IN_JoyButtons();
			realdelta = true;
			break;
		case ctrl_Mouse:
			INL_GetMouseDelta(&dx, &dy);
			buttons = INL_GetMouseButtons();
			realdelta = true;
			break;
		default:
			realdelta = false;
			break;
		}
	}

	if (realdelta)
	{
		mx = (dx < 0) ? motion_Left : ((dx > 0) ? motion_Right : motion_None);
		my = (dy < 0) ? motion_Up : ((dy > 0) ? motion_Down : motion_None);
	}
	else
	{
		dx = mx * 127;
		dy = my * 127;
	}

	info->x = dx;
	info->xaxis = mx;
	info->y = dy;
	info->yaxis = my;
	info->button0 = buttons & (1 << 0);
	info->button1 = buttons & (1 << 1);
	info->button2 = buttons & (1 << 2);
	info->button3 = buttons & (1 << 3);
	info->dir = DirTable[((my + 1) * 3) + (mx + 1)];

	if (DemoMode == demo_Record)
	{
		dbyte = (buttons << 4) | ((mx + 1) << 2) | (my + 1);

		if ((DemoBuffer[DemoOffset + 1] == dbyte) && (DemoBuffer[DemoOffset] < 255))
			(DemoBuffer[DemoOffset])++;
		else
		{
			if (DemoOffset || DemoBuffer[DemoOffset])
				DemoOffset += 2;

			if (DemoOffset >= DemoSize)
				Quit("Demo buffer overflow");

			DemoBuffer[DemoOffset] = 1;
			DemoBuffer[DemoOffset + 1] = dbyte;
		}
	}
}

void IN_SetControlType(int player, ControlType type)
{
	Controls[player] = type;
}

void IN_ReadCursor(CursorInfo *info)
{
	IN_ReadControl(0, info);
}

//==========================================================================
// Wait/Ack functions
//==========================================================================

ScanCode IN_WaitForKey(void)
{
	ScanCode result;
	while (!(result = LastScan))
	{
		IN_PumpEvents();
		SDL_Delay(1);
	}
	LastScan = 0;
	return result;
}

char IN_WaitForASCII(void)
{
	char result;
	while (!(result = LastASCII))
	{
		IN_PumpEvents();
		SDL_Delay(1);
	}
	LastASCII = '\0';
	return result;
}

void IN_StartAck(void)
{
	unsigned i, buttons;

	IN_PumpEvents();
	IN_ClearKeysDown();
	memset(btnstate, 0, sizeof(btnstate));

	buttons = IN_JoyButtons() << 4;
	if (MousePresent)
		buttons |= IN_MouseButtons();

	for (i = 0; i < 8; i++, buttons >>= 1)
		if (buttons & 1)
			btnstate[i] = true;
}

boolean IN_CheckAck(void)
{
	unsigned i, buttons;

	IN_PumpEvents();

	if (LastScan)
		return true;

	buttons = IN_JoyButtons() << 4;
	if (MousePresent)
		buttons |= IN_MouseButtons();

	for (i = 0; i < 8; i++, buttons >>= 1)
		if (buttons & 1)
		{
			if (!btnstate[i])
				return true;
		}
		else
			btnstate[i] = false;

	return false;
}

void IN_Ack(void)
{
	IN_StartAck();
	while (!IN_CheckAck())
		SDL_Delay(1);
}

void IN_AckBack(void)
{
	while (IN_MouseButtons() || IN_JoyButtons() || LastScan)
	{
		IN_PumpEvents();
		SDL_Delay(1);
	}
}

boolean IN_UserInput(longword delay)
{
	longword lasttime = TimeCount;
	IN_StartAck();
	do
	{
		IN_PumpEvents();
		if (IN_CheckAck())
			return true;
		SDL_Delay(1);
	} while (TimeCount - lasttime < delay);
	return false;
}

//==========================================================================

byte *IN_GetScanName(ScanCode scan)
{
	byte     **p;
	ScanCode *s;

	for (s = ExtScanCodes, p = ExtScanNames; *s; p++, s++)
		if (*s == scan)
			return *p;

	return ScanNames[scan];
}

void IN_StopDemo(void)
{
	DemoMode = demo_Off;
}

void IN_FreeDemoBuffer(void)
{
	if (DemoBuffer)
	{
		free(DemoBuffer);
		DemoBuffer = NULL;
	}
}
