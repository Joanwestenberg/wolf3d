// ID_VH.C - Video High-level (macOS/SDL2 port)

#include "wl_def.h"

//==========================================================================

pictabletype    *pictable;
pictabletype    *picmtable;
spritetabletype *spritetable;

byte fontcolor;
int  fontnumber;
int  px, py;

unsigned latchpics[NUMLATCHPICS];
unsigned freelatch;

extern byte gamepal[768]; // defined in signon.c

//==========================================================================

void VW_InitDoubleBuffer(void)
{
	// No-op in SDL2 port -- we always present the whole framebuffer
}

int VW_MarkUpdateBlock(int x1, int y1, int x2, int y2)
{
	(void)x1; (void)y1; (void)x2; (void)y2;
	return 1;
}

void VW_UpdateScreen(void)
{
	VL_Present();
}

//==========================================================================
// Drawing routines
//==========================================================================

void VWB_DrawTile8(int x, int y, int tile)
{
	byte *src;

	if (!grsegs[STARTTILE8])
		return;

	src = ((byte *)grsegs[STARTTILE8]) + tile * 64;
	VL_PlanarToScreen(src, 8, 8, x, y);
}

void VWB_DrawTile8M(int x, int y, int tile)
{
	VWB_DrawTile8(x, y, tile);
}

void VWB_DrawTile16(int x, int y, int tile)
{
	(void)x; (void)y; (void)tile;
}

void VWB_DrawTile16M(int x, int y, int tile)
{
	(void)x; (void)y; (void)tile;
}

void VWB_DrawPic(int x, int y, int chunknum)
{
	int picwidth, picheight;

	if (chunknum < 0 || !pictable || !grsegs[chunknum])
		return;

	picwidth = pictable[chunknum - STARTPICS].width;
	picheight = pictable[chunknum - STARTPICS].height;

	VL_PlanarToScreen((byte *)grsegs[chunknum], picwidth, picheight, x & ~7, y);
}

void VWB_DrawMPic(int x, int y, int chunknum)
{
	VWB_DrawPic(x, y, chunknum);
}

void VWB_Bar(int x, int y, int width, int height, int color)
{
	VL_Bar(x, y, width, height, color);
}

void VWB_Plot(int x, int y, int color)
{
	VL_Plot(x, y, color);
}

void VWB_Hlin(int x1, int x2, int y, int color)
{
	VL_Hlin(x1, y, x2 - x1 + 1, color);
}

void VWB_Vlin(int y1, int y2, int x, int color)
{
	VL_Vlin(x, y1, y2 - y1 + 1, color);
}

//==========================================================================
// Proportional font drawing
//==========================================================================

void VWB_DrawPropString(char *string)
{
	fontstruct *font;
	int width, step, height;
	byte *source;
	byte ch;

	font = (fontstruct *)grsegs[STARTFONT + fontnumber];
	if (!font)
		return;

	height = font->height;

	while ((ch = (byte)*string++) != 0)
	{
		width = step = font->width[ch];
		source = ((byte *)font) + font->location[ch];

		while (width--)
		{
			int i;
			for (i = 0; i < height; i++)
			{
				byte pixel = source[i * step];
				if (pixel)
				{
					if (px < 320 && py + i < 200)
						sdl_framebuffer[(py + i) * 320 + px] = fontcolor;
				}
				else
				{
					// transparent
				}
			}
			px++;
			source++;
		}
	}
}

void VWB_DrawMPropString(char *string)
{
	VWB_DrawPropString(string);
}

void VWB_DrawSprite(int x, int y, int chunknum)
{
	(void)x; (void)y; (void)chunknum;
	// Sprites are drawn via the compiled scalers in the raycaster
}

void VW_MeasurePropString(char *string, word *width, word *height)
{
	fontstruct *font;
	word w = 0;

	font = (fontstruct *)grsegs[STARTFONT + fontnumber];
	if (!font)
	{
		*width = 0;
		*height = 0;
		return;
	}

	*height = font->height;

	while (*string)
		w += font->width[(byte)*string++];

	*width = w;
}

//==========================================================================
// Palette
//==========================================================================

void VH_SetDefaultColors(void)
{
	VL_SetPalette(gamepal);
}

//==========================================================================
// Latch functions
//==========================================================================

void LatchDrawPic(unsigned x, unsigned y, unsigned picnum)
{
	unsigned source;

	source = latchpics[2 + picnum - LATCHPICS_LUMP_START];
	VL_LatchToScreen(source,
		pictable[picnum - STARTPICS].width,
		pictable[picnum - STARTPICS].height,
		x * 8, y);
}

void LoadLatchMem(void)
{
	int i, start, end;
	unsigned destoff;
	int picwidth, picheight;

	// Reset latch memory
	freelatch = FREESTART;
	memset(vl_latchmem, 0, VL_LATCHMEM_SIZE);

	// Load tile 8s
	CA_CacheGrChunk(STARTTILE8);
	latchpics[0] = freelatch;
	if (grsegs[STARTTILE8])
	{
		VL_MemToLatch((byte *)grsegs[STARTTILE8],
			8, 8 * NUMTILE8, freelatch);
		freelatch += 8 * 8 * NUMTILE8;
	}
	UNCACHEGRCHUNK(STARTTILE8);

	// Load tile 16s
	latchpics[1] = freelatch;

	// Load latch pics
	start = LATCHPICS_LUMP_START;
	end = LATCHPICS_LUMP_END;
	destoff = freelatch;

	for (i = start; i <= end; i++)
	{
		latchpics[2 + i - start] = destoff;
		CA_CacheGrChunk(i);
		if (grsegs[i])
		{
			picwidth = pictable[i - STARTPICS].width;
			picheight = pictable[i - STARTPICS].height;
			VL_MemToLatch((byte *)grsegs[i], picwidth, picheight, destoff);
			destoff += picwidth * picheight;
		}
		UNCACHEGRCHUNK(i);
	}

	freelatch = destoff;
}

//==========================================================================
// FizzleFade - LFSR-based screen transition effect
//==========================================================================

boolean FizzleFade(unsigned source, unsigned dest,
	unsigned width, unsigned height, unsigned frames, boolean abortable)
{
	unsigned pixperframe;
	unsigned rndval = 1;
	longword frame;
	int p;
	unsigned x, y;
	longword lastframe;
	byte *newscreen;
	int destx, desty;

	(void)source;

	// Compute the top-left corner of the dest region in the framebuffer
	desty = dest / 320;
	destx = dest % 320;

	// Save the new content that's already in the framebuffer
	newscreen = (byte *)malloc(width * height);
	if (!newscreen)
		return false;

	for (y = 0; y < height; y++)
	{
		int sy = desty + y;
		if (sy < 200)
			memcpy(&newscreen[y * width],
				   &sdl_framebuffer[sy * 320 + destx],
				   (destx + width <= 320) ? width : 320 - destx);
	}

	// Fill the dest region with black to start the dissolve
	for (y = 0; y < height; y++)
	{
		int sy = desty + y;
		if (sy < 200)
			memset(&sdl_framebuffer[sy * 320 + destx], 0,
				   (destx + width <= 320) ? width : 320 - destx);
	}
	VL_Present();

	pixperframe = (width * height) / frames;
	lastframe = TimeCount;

	for (frame = 0; frame < frames; frame++)
	{
		if (abortable)
		{
			IN_PumpEvents();
			if (LastScan)
			{
				// Reveal remaining pixels immediately
				for (y = 0; y < height; y++)
				{
					int sy = desty + y;
					if (sy < 200)
						memcpy(&sdl_framebuffer[sy * 320 + destx],
							   &newscreen[y * width],
							   (destx + width <= 320) ? width : 320 - destx);
				}
				free(newscreen);
				VL_Present();
				return true;
			}
		}

		for (p = 0; p < (int)pixperframe; p++)
		{
			// Galois LFSR for pseudo-random pixel ordering
			do
			{
				// 17-bit LFSR
				unsigned lsb = rndval & 1;
				rndval >>= 1;
				if (lsb)
					rndval ^= 0x00012000;

				x = (rndval - 1) % 320;
				y = (rndval - 1) / 320;
			} while (rndval - 1 >= width * height);

			if (x < width && y < height)
			{
				int sx = destx + x;
				int sy = desty + y;
				if (sx < 320 && sy < 200)
					sdl_framebuffer[sy * 320 + sx] = newscreen[y * width + x];
			}

			if (rndval == 1)
				break; // full cycle completed
		}

		// Wait for frame timing
		while (TimeCount - lastframe < 1)
			SDL_Delay(1);
		lastframe = TimeCount;

		VL_Present();
	}

	// Ensure all pixels are revealed
	for (y = 0; y < height; y++)
	{
		int sy = desty + y;
		if (sy < 200)
			memcpy(&sdl_framebuffer[sy * 320 + destx],
				   &newscreen[y * width],
				   (destx + width <= 320) ? width : 320 - destx);
	}
	free(newscreen);
	VL_Present();

	return false;
}
