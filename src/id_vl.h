// ID_VL.H - Video Low-level header (macOS/SDL2 port)

#ifndef __ID_VL_H__
#define __ID_VL_H__

#define MS_Quit Quit
void Quit(char *error);

//===========================================================================

extern unsigned bufferofs;
extern unsigned displayofs, pelpan;

extern unsigned screenseg;

extern unsigned linewidth;
extern unsigned ylookup[MAXSCANLINES];

extern boolean  screenfaded;
extern unsigned bordercolor;

//===========================================================================

void VL_Startup(void);
void VL_Shutdown(void);

void VL_SetVGAPlaneMode(void);
void VL_ClearVideo(byte color);

void VL_SetLineWidth(unsigned width);
void VL_SetSplitScreen(int linenum);

void VL_WaitVBL(int vbls);
void VL_CrtcStart(int crtc);
void VL_SetScreen(int crtc, int pelpan);

void VL_FillPalette(int red, int green, int blue);
void VL_SetColor(int color, int red, int green, int blue);
void VL_GetColor(int color, int *red, int *green, int *blue);
void VL_SetPalette(byte *palette);
void VL_GetPalette(byte *palette);
void VL_FadeOut(int start, int end, int red, int green, int blue, int steps);
void VL_FadeIn(int start, int end, byte *palette, int steps);
void VL_ColorBorder(int color);

void VL_Plot(int x, int y, int color);
void VL_Hlin(unsigned x, unsigned y, unsigned width, unsigned color);
void VL_Vlin(int x, int y, int height, int color);
void VL_Bar(int x, int y, int width, int height, int color);

void VL_MungePic(byte *source, unsigned width, unsigned height);
void VL_MemToLatch(byte *source, int width, int height, unsigned dest);
void VL_ScreenToScreen(unsigned source, unsigned dest, int width, int height);
void VL_MemToScreen(byte *source, int width, int height, int x, int y);
void VL_PlanarToScreen(byte *source, int width, int height, int x, int y);
void VL_LatchToScreen(unsigned source, int width, int height, int x, int y);
void VL_ScreenToMem(byte *dest, int width, int height, int x, int y);

void VL_TestPaletteSet(void);

//===========================================================================

// SDL2-specific interface
extern SDL_Window   *sdl_window;
extern SDL_Renderer *sdl_renderer;
extern SDL_Texture  *sdl_texture;
extern byte         *sdl_framebuffer;   // 320x200 indexed color buffer

void VL_Present(void);

// Latch memory (for id_vh.c)
extern byte *vl_latchmem;
#define VL_LATCHMEM_SIZE (64 * 1024)

#endif
