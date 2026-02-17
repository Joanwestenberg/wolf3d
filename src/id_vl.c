// ID_VL.C - Video Low-level (macOS/SDL2 port)
//
// Replaces all VGA Mode X hardware access with an SDL2 window,
// renderer, and streaming texture. The game draws into a flat
// 320x200 indexed-color framebuffer which is palette-converted
// and uploaded to the GPU each frame via VL_Present().

#include "id_heads.h"

//==========================================================================

unsigned bufferofs;
unsigned displayofs, pelpan;
unsigned screenseg;
unsigned linewidth;
unsigned ylookup[MAXSCANLINES];
boolean  screenfaded;
unsigned bordercolor;

//==========================================================================
// SDL2 state
//==========================================================================

SDL_Window   *sdl_window;
SDL_Renderer *sdl_renderer;
SDL_Texture  *sdl_texture;
byte         *sdl_framebuffer;   // 320x200 indexed

static SDL_Color sdl_palette[256];
static uint32_t  sdl_rgbapal[256]; // pre-computed ARGB

// Latch memory -- a block of RAM that replaces VGA off-screen memory
// used by LoadLatchMem / VL_LatchToScreen
byte *vl_latchmem;

//==========================================================================

void VL_Startup(void)
{
	int i;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0)
		Quit("VL_Startup: SDL_Init failed");

	sdl_window = SDL_CreateWindow(
		"Wolfenstein 3D",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		960, 600, 0);
	if (!sdl_window)
		Quit("VL_Startup: SDL_CreateWindow failed");

	sdl_renderer = SDL_CreateRenderer(sdl_window, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!sdl_renderer)
		sdl_renderer = SDL_CreateRenderer(sdl_window, -1, 0);
	if (!sdl_renderer)
		Quit("VL_Startup: SDL_CreateRenderer failed");

	SDL_RenderSetLogicalSize(sdl_renderer, 320, 200);

	sdl_texture = SDL_CreateTexture(sdl_renderer,
		SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STREAMING,
		320, 200);
	if (!sdl_texture)
		Quit("VL_Startup: SDL_CreateTexture failed");

	sdl_framebuffer = (byte *)calloc(320 * 200, 1);
	if (!sdl_framebuffer)
		Quit("VL_Startup: Failed to allocate framebuffer");

	vl_latchmem = (byte *)calloc(VL_LATCHMEM_SIZE, 1);
	if (!vl_latchmem)
		Quit("VL_Startup: Failed to allocate latch memory");

	// Initialize ylookup
	linewidth = 320;
	for (i = 0; i < MAXSCANLINES; i++)
		ylookup[i] = i * linewidth;

	// Default palette to greyscale
	for (i = 0; i < 256; i++)
	{
		sdl_palette[i].r = sdl_palette[i].g = sdl_palette[i].b = (byte)i;
		sdl_palette[i].a = 255;
		sdl_rgbapal[i] = 0xFF000000 | (i << 16) | (i << 8) | i;
	}

	screenfaded = false;
	bufferofs = 0;
	displayofs = 0;
	pelpan = 0;
	screenseg = 0;
}

void VL_Shutdown(void)
{
	if (vl_latchmem)  { free(vl_latchmem);  vl_latchmem = NULL; }
	if (sdl_framebuffer) { free(sdl_framebuffer); sdl_framebuffer = NULL; }
	if (sdl_texture)  { SDL_DestroyTexture(sdl_texture);   sdl_texture = NULL; }
	if (sdl_renderer) { SDL_DestroyRenderer(sdl_renderer); sdl_renderer = NULL; }
	if (sdl_window)   { SDL_DestroyWindow(sdl_window);     sdl_window = NULL; }
	SDL_Quit();
}

//==========================================================================

void VL_Present(void)
{
	uint32_t *pixels;
	int pitch, i;

	if (SDL_LockTexture(sdl_texture, NULL, (void **)&pixels, &pitch) < 0)
		return;

	for (i = 0; i < 320 * 200; i++)
		pixels[i] = sdl_rgbapal[sdl_framebuffer[i]];

	SDL_UnlockTexture(sdl_texture);
	SDL_RenderClear(sdl_renderer);
	SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
	SDL_RenderPresent(sdl_renderer);
}

//==========================================================================

void VL_SetVGAPlaneMode(void)
{
	// Already set up in VL_Startup
	VL_ClearVideo(0);
}

void VL_ClearVideo(byte color)
{
	memset(sdl_framebuffer, color, 320 * 200);
}

void VL_SetLineWidth(unsigned width)
{
	int i;
	linewidth = width * 4; // original was in "words" (4 bytes per word in Mode X)
	if (linewidth < 320)
		linewidth = 320;
	for (i = 0; i < MAXSCANLINES; i++)
		ylookup[i] = i * linewidth;
}

void VL_SetSplitScreen(int linenum)
{
	(void)linenum;
}

void VL_WaitVBL(int vbls)
{
	SDL_Delay(vbls * 1000 / 70);
}

void VL_CrtcStart(int crtc)
{
	displayofs = crtc;
}

void VL_SetScreen(int crtc, int pel)
{
	displayofs = crtc;
	pelpan = pel;
}

//==========================================================================
// Palette
//==========================================================================

static void UpdateRGBAPalette(void)
{
	int i;
	for (i = 0; i < 256; i++)
		sdl_rgbapal[i] = 0xFF000000 |
			((uint32_t)sdl_palette[i].r << 16) |
			((uint32_t)sdl_palette[i].g << 8) |
			(uint32_t)sdl_palette[i].b;
}

void VL_FillPalette(int red, int green, int blue)
{
	int i;
	for (i = 0; i < 256; i++)
	{
		sdl_palette[i].r = red;
		sdl_palette[i].g = green;
		sdl_palette[i].b = blue;
	}
	UpdateRGBAPalette();
}

void VL_SetColor(int color, int red, int green, int blue)
{
	sdl_palette[color].r = red;
	sdl_palette[color].g = green;
	sdl_palette[color].b = blue;
	sdl_rgbapal[color] = 0xFF000000 |
		((uint32_t)red << 16) | ((uint32_t)green << 8) | (uint32_t)blue;
}

void VL_GetColor(int color, int *red, int *green, int *blue)
{
	*red   = sdl_palette[color].r;
	*green = sdl_palette[color].g;
	*blue  = sdl_palette[color].b;
}

void VL_SetPalette(byte *palette)
{
	int i;
	for (i = 0; i < 256; i++)
	{
		// Original VGA palette is 6-bit (0-63), scale to 8-bit
		sdl_palette[i].r = palette[i * 3 + 0] * 255 / 63;
		sdl_palette[i].g = palette[i * 3 + 1] * 255 / 63;
		sdl_palette[i].b = palette[i * 3 + 2] * 255 / 63;
	}
	UpdateRGBAPalette();
	screenfaded = false;
}

void VL_GetPalette(byte *palette)
{
	int i;
	for (i = 0; i < 256; i++)
	{
		palette[i * 3 + 0] = sdl_palette[i].r * 63 / 255;
		palette[i * 3 + 1] = sdl_palette[i].g * 63 / 255;
		palette[i * 3 + 2] = sdl_palette[i].b * 63 / 255;
	}
}

void VL_FadeOut(int start, int end, int red, int green, int blue, int steps)
{
	int i, j;
	byte origpal[256 * 3];

	VL_GetPalette(origpal);

	red   = red * 255 / 63;
	green = green * 255 / 63;
	blue  = blue * 255 / 63;

	for (i = 0; i < steps; i++)
	{
		for (j = start; j <= end; j++)
		{
			int orig_r = origpal[j * 3 + 0] * 255 / 63;
			int orig_g = origpal[j * 3 + 1] * 255 / 63;
			int orig_b = origpal[j * 3 + 2] * 255 / 63;

			sdl_palette[j].r = orig_r + (red - orig_r) * (i + 1) / steps;
			sdl_palette[j].g = orig_g + (green - orig_g) * (i + 1) / steps;
			sdl_palette[j].b = orig_b + (blue - orig_b) * (i + 1) / steps;
		}
		UpdateRGBAPalette();
		VL_Present();
		VL_WaitVBL(1);
	}

	screenfaded = true;
}

void VL_FadeIn(int start, int end, byte *palette, int steps)
{
	int i, j;
	SDL_Color origpal[256];

	// Save starting palette so we interpolate from fixed values
	memcpy(origpal, sdl_palette, sizeof(origpal));

	for (i = 0; i < steps; i++)
	{
		for (j = start; j <= end; j++)
		{
			int target_r = palette[j * 3 + 0] * 255 / 63;
			int target_g = palette[j * 3 + 1] * 255 / 63;
			int target_b = palette[j * 3 + 2] * 255 / 63;

			sdl_palette[j].r = origpal[j].r + (target_r - origpal[j].r) * (i + 1) / steps;
			sdl_palette[j].g = origpal[j].g + (target_g - origpal[j].g) * (i + 1) / steps;
			sdl_palette[j].b = origpal[j].b + (target_b - origpal[j].b) * (i + 1) / steps;
		}
		UpdateRGBAPalette();
		VL_Present();
		VL_WaitVBL(1);
	}

	VL_SetPalette(palette);
	screenfaded = false;
}

void VL_ColorBorder(int color)
{
	bordercolor = color;
}

//==========================================================================
// Drawing primitives
//==========================================================================

void VL_Plot(int x, int y, int color)
{
	if (x >= 0 && x < 320 && y >= 0 && y < 200)
		sdl_framebuffer[y * 320 + x] = color;
}

void VL_Hlin(unsigned x, unsigned y, unsigned width, unsigned color)
{
	if (y < 200)
		memset(&sdl_framebuffer[y * 320 + x], color, width);
}

void VL_Vlin(int x, int y, int height, int color)
{
	int i;
	byte *dest = &sdl_framebuffer[y * 320 + x];
	for (i = 0; i < height; i++, dest += 320)
		*dest = color;
}

void VL_Bar(int x, int y, int width, int height, int color)
{
	int i;
	byte *dest = &sdl_framebuffer[y * 320 + x];
	for (i = 0; i < height; i++, dest += 320)
		memset(dest, color, width);
}

//==========================================================================
// Blitting
//==========================================================================

void VL_MungePic(byte *source, unsigned width, unsigned height)
{
	// De-interleave VGA planar data to linear
	unsigned size = width * height;
	byte *temp = (byte *)malloc(size);
	unsigned plane, x, y;
	byte *src, *dest;

	if (!temp) return;
	memcpy(temp, source, size);

	src = temp;
	for (plane = 0; plane < 4; plane++)
	{
		dest = &source[plane];
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < width / 4; x++)
			{
				*dest = *src++;
				dest += 4;
			}
		}
	}

	free(temp);
}

void VL_MemToLatch(byte *source, int width, int height, unsigned dest)
{
	// Copy planar data into latch memory, de-interleaving
	int plane, x, y;
	int planewidth = width / 4;

	for (plane = 0; plane < 4; plane++)
	{
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < planewidth; x++)
			{
				int srcidx = plane * planewidth * height + y * planewidth + x;
				int dstidx = y * width + x * 4 + plane;
				if (dest + dstidx < VL_LATCHMEM_SIZE)
					vl_latchmem[dest + dstidx] = source[srcidx];
			}
		}
	}
}

void VL_ScreenToScreen(unsigned source, unsigned dest, int width, int height)
{
	// Copy within framebuffer (source/dest are byte offsets)
	int y;
	int w = width * 4; // width in "Mode X words" -> pixels
	for (y = 0; y < height; y++)
	{
		int srcoff = source + y * 320;
		int dstoff = dest + y * 320;
		if (srcoff + w <= 320 * 200 && dstoff + w <= 320 * 200)
			memcpy(&sdl_framebuffer[dstoff], &sdl_framebuffer[srcoff], w);
	}
}

void VL_MemToScreen(byte *source, int width, int height, int x, int y)
{
	// After VL_MungePic, data is already in linear pixel order
	int py;

	for (py = 0; py < height; py++)
	{
		if (y + py < 200)
			memcpy(&sdl_framebuffer[(y + py) * 320 + x],
				   &source[py * width],
				   (x + width <= 320) ? width : 320 - x);
	}
}

void VL_PlanarToScreen(byte *source, int width, int height, int x, int y)
{
	// De-interleave VGA planar data (plane-sequential) to linear framebuffer
	// Used for cached graphics that haven't been through VL_MungePic
	int plane, px, py;
	int planewidth = width / 4;

	for (plane = 0; plane < 4; plane++)
	{
		for (py = 0; py < height; py++)
		{
			for (px = 0; px < planewidth; px++)
			{
				int destx = x + px * 4 + plane;
				int desty = y + py;
				if (destx < 320 && desty < 200)
					sdl_framebuffer[desty * 320 + destx] =
						source[plane * planewidth * height + py * planewidth + px];
			}
		}
	}
}

void VL_LatchToScreen(unsigned source, int width, int height, int x, int y)
{
	// Copy from latch memory to screen
	int w = width * 4; // width in Mode X words -> pixels
	int py;
	for (py = 0; py < height; py++)
	{
		int srcoff = source + py * w;
		int dstoff = (y + py) * 320 + x;
		if (srcoff + w <= VL_LATCHMEM_SIZE && dstoff + w <= 320 * 200)
			memcpy(&sdl_framebuffer[dstoff], &vl_latchmem[srcoff], w);
	}
}

void VL_ScreenToMem(byte *dest, int width, int height, int x, int y)
{
	int py;
	for (py = 0; py < height; py++)
		memcpy(&dest[py * width], &sdl_framebuffer[(y + py) * 320 + x], width);
}

void VL_TestPaletteSet(void)
{
}
