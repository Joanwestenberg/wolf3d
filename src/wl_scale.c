// WL_SCALE.C - Software sprite scaling for macOS ARM64/SDL2 port
//
// The original DOS version used "compiled scalers" - x86 machine code
// generated at runtime to draw scaled sprite columns directly to VGA memory.
// This rewrite replaces all of that with straightforward software scaling
// using nearest-neighbor interpolation, writing to the SDL2 framebuffer.

#include "wl_def.h"
#pragma hdrstop

/*
=============================================================================

                          GLOBALS

=============================================================================
*/

t_compscale *scaledirectory[MAXSCALEHEIGHT+1];
long        fullscalefarcall[MAXSCALEHEIGHT+1];

int         maxscale, maxscaleshl2;

boolean     insetupscaling;

/*
=============================================================================

                          LOCALS

=============================================================================
*/

int         stepbytwo;

int         slinex, slinewidth;
uint16_t    *linecmds;
long        linescale;
unsigned    maskword;

//
// bit mask tables - kept for compatibility even though VGA masks are not used
//
byte mapmasks1[4][8] = {
{1 ,3 ,7 ,15,15,15,15,15},
{2 ,6 ,14,14,14,14,14,14},
{4 ,12,12,12,12,12,12,12},
{8 ,8 ,8 ,8 ,8 ,8 ,8 ,8} };

byte mapmasks2[4][8] = {
{0 ,0 ,0 ,0 ,1 ,3 ,7 ,15},
{0 ,0 ,0 ,1 ,3 ,7 ,15,15},
{0 ,0 ,1 ,3 ,7 ,15,15,15},
{0 ,1 ,3 ,7 ,15,15,15,15} };

byte mapmasks3[4][8] = {
{0 ,0 ,0 ,0 ,0 ,0 ,0 ,0},
{0 ,0 ,0 ,0 ,0 ,0 ,0 ,1},
{0 ,0 ,0 ,0 ,0 ,0 ,1 ,3},
{0 ,0 ,0 ,0 ,0 ,1 ,3 ,7} };

unsigned wordmasks[8][8] = {
{0x0080,0x00c0,0x00e0,0x00f0,0x00f8,0x00fc,0x00fe,0x00ff},
{0x0040,0x0060,0x0070,0x0078,0x007c,0x007e,0x007f,0x807f},
{0x0020,0x0030,0x0038,0x003c,0x003e,0x003f,0x803f,0xc03f},
{0x0010,0x0018,0x001c,0x001e,0x001f,0x801f,0xc01f,0xe01f},
{0x0008,0x000c,0x000e,0x000f,0x800f,0xc00f,0xe00f,0xf00f},
{0x0004,0x0006,0x0007,0x8007,0xc007,0xe007,0xf007,0xf807},
{0x0002,0x0003,0x8003,0xc003,0xe003,0xf003,0xf803,0xfc03},
{0x0001,0x8001,0xc001,0xe001,0xf001,0xf801,0xfc01,0xfe01} };


//===========================================================================

/*
==========================
=
= SetupScaling
=
= In the original, this built compiled x86 scaler functions for each
= possible scale height. We just store the max scale parameters;
= actual scaling is done at draw time with software math.
=
==========================
*/

void SetupScaling (int maxscaleheight)
{
    insetupscaling = true;

    maxscaleheight /= 2;            // one scaler every two pixels

    maxscale = maxscaleheight - 1;
    maxscaleshl2 = maxscale << 2;

    stepbytwo = viewheight / 2;

    //
    // Clear out old scaler directory - we no longer allocate compiled code,
    // but other parts of the code may check these pointers
    //
    memset(scaledirectory, 0, sizeof(scaledirectory));
    memset(fullscalefarcall, 0, sizeof(fullscalefarcall));

    insetupscaling = false;
}


//===========================================================================

/*
=======================
=
= ScaleLine
=
= Draws a single scaled sprite column to sdl_framebuffer.
=
= Uses the globals: slinex, slinewidth, linecmds, linescale
=
= linecmds points to the column segment data within the sprite shape.
= linescale holds the display height for this sprite.
=
= The segment command format (from the t_compshape data) is:
=   word: end pixel * 2   (0 = end of column)
=   word: source pixel offset into the 64-byte texture column
=   word: start pixel * 2
=   <repeat>
=
=======================
*/

void ScaleLine (void)
{
    uint16_t *cmdptr;
    int      displayheight;
    int      toppix;
    int      x, y;
    int      endsrc, startsrc;
    int      startpix, endpix;
    byte     *shape_bytes;
    byte     pixel;

    displayheight = (int)linescale;
    if (displayheight <= 0)
        return;

    toppix = (viewheight - displayheight) / 2;

    //
    // linecmds points into the shape's column segment data
    //
    cmdptr = linecmds;
    shape_bytes = (byte *)linecmds;

    while (1)
    {
        unsigned endval = cmdptr[0];     // end pixel * 2 (0 = done)
        if (endval == 0)
            break;

        unsigned srcoff = cmdptr[1];     // source pixel offset
        unsigned startval = cmdptr[2];   // start pixel * 2
        cmdptr += 3;

        //
        // Convert from shape's 64-pixel coordinate space to screen space
        //
        startsrc = startval / 2;
        endsrc = endval / 2;

        //
        // Scale source range [startsrc..endsrc) to screen
        //
        for (int src = startsrc; src < endsrc; src++)
        {
            //
            // Calculate screen Y range for this source pixel
            //
            int screeny_start = toppix + (src * displayheight) / 64;
            int screeny_end   = toppix + ((src + 1) * displayheight) / 64;

            //
            // Get the source pixel color from the shape data.
            // srcoff is an offset from the start of the shape data, and
            // the source pixels are at sequential bytes from that offset.
            // The shape bytes pointer gives us access to the whole shape.
            //
            pixel = shape_bytes[srcoff + src - startsrc];

            if (pixel == 0)
                continue;   // transparent

            for (y = screeny_start; y < screeny_end; y++)
            {
                if (y < 0)
                    continue;
                if (y >= viewheight)
                    break;

                //
                // Draw across the width of this scaled column
                //
                for (x = slinex; x < slinex + slinewidth; x++)
                {
                    if (x >= 0 && x < viewwidth)
                    {
                        sdl_framebuffer[(y + screenofs / 320) * 320 + (screenofs % 320) + x] = pixel;
                    }
                }
            }
        }
    }
}


//===========================================================================

/*
=======================
=
= ScaleShape
=
= Draws a compiled shape at [scale] pixels high
=
= each vertical line of the shape has a pointer to segment data:
=   end of segment pixel*2 (0 terminates line)
=   source pixel offset
=   start of segment pixel*2
=   <repeat>
=
=======================
*/

void ScaleShape (int xcenter, int shapenum, unsigned height)
{
    t_compshape *shape;
    unsigned     scale, srcx, stopx;
    uint16_t    *cmdptr;
    boolean      leftvis, rightvis;
    int          displayheight;
    long         frac, step;

    shape = PM_GetSpritePage(shapenum);

    scale = height >> 3;                    // low three bits are fractional
    if (!scale || scale > (unsigned)maxscale)
        return;                             // too close or far away

    //
    // Calculate the display height and column width from scale
    //
    displayheight = scale * 2;
    linescale = displayheight;

    //
    // Calculate the fixed-point step for mapping 64 source columns to screen pixels
    //
    step = ((long)displayheight << 16) / 64;

    //
    // scale to the left (from pixel 31 to shape->leftpix)
    //
    srcx = 32;
    slinex = xcenter;
    stopx = shape->leftpix;
    cmdptr = &shape->dataofs[31 - stopx];

    while (--srcx >= stopx && slinex > 0)
    {
        //
        // Calculate width of this source column in screen pixels
        //
        int col_start = (int)(((long)srcx * step) >> 16);
        int col_end   = (int)(((long)(srcx + 1) * step) >> 16);
        slinewidth = col_end - col_start;
        if (slinewidth <= 0)
        {
            cmdptr--;
            continue;
        }

        linecmds = (uint16_t *)((byte *)shape + *cmdptr);
        cmdptr--;

        if (slinewidth == 1)
        {
            slinex--;
            if (slinex < viewwidth)
            {
                if (wallheight[slinex] >= height)
                    continue;               // obscured by closer wall
                ScaleLine();
            }
            continue;
        }

        //
        // handle multi pixel lines
        //
        if (slinex > viewwidth)
        {
            slinex -= slinewidth;
            slinewidth = viewwidth - slinex;
            if (slinewidth < 1)
                continue;                   // still off the right side
        }
        else
        {
            if (slinewidth > slinex)
                slinewidth = slinex;
            slinex -= slinewidth;
        }

        leftvis = (wallheight[slinex] < height);
        rightvis = (wallheight[slinex + slinewidth - 1] < height);

        if (leftvis)
        {
            if (rightvis)
                ScaleLine();
            else
            {
                while (wallheight[slinex + slinewidth - 1] >= height)
                    slinewidth--;
                ScaleLine();
            }
        }
        else
        {
            if (!rightvis)
                continue;                   // totally obscured

            while (wallheight[slinex] >= height)
            {
                slinex++;
                slinewidth--;
            }
            ScaleLine();
            break;                          // the rest of the shape is gone
        }
    }


    //
    // scale to the right
    //
    slinex = xcenter;
    stopx = shape->rightpix;
    if (shape->leftpix < 31)
    {
        srcx = 31;
        cmdptr = &shape->dataofs[32 - shape->leftpix];
    }
    else
    {
        srcx = shape->leftpix - 1;
        cmdptr = &shape->dataofs[0];
    }
    slinewidth = 0;

    while (++srcx <= stopx && (slinex += slinewidth) < viewwidth)
    {
        //
        // Calculate width of this source column in screen pixels
        //
        int col_start = (int)(((long)srcx * step) >> 16);
        int col_end   = (int)(((long)(srcx + 1) * step) >> 16);
        slinewidth = col_end - col_start;
        if (slinewidth <= 0)
        {
            continue;
        }

        linecmds = (uint16_t *)((byte *)shape + *cmdptr);
        cmdptr++;

        if (slinewidth == 1)
        {
            if (slinex >= 0 && wallheight[slinex] < height)
            {
                ScaleLine();
            }
            continue;
        }

        //
        // handle multi pixel lines
        //
        if (slinex < 0)
        {
            if (slinewidth <= -slinex)
                continue;                   // still off the left edge

            slinewidth += slinex;
            slinex = 0;
        }
        else
        {
            if (slinex + slinewidth > viewwidth)
                slinewidth = viewwidth - slinex;
        }

        leftvis = (wallheight[slinex] < height);
        rightvis = (wallheight[slinex + slinewidth - 1] < height);

        if (leftvis)
        {
            if (rightvis)
            {
                ScaleLine();
            }
            else
            {
                while (wallheight[slinex + slinewidth - 1] >= height)
                    slinewidth--;
                ScaleLine();
                break;                      // the rest of the shape is gone
            }
        }
        else
        {
            if (rightvis)
            {
                while (wallheight[slinex] >= height)
                {
                    slinex++;
                    slinewidth--;
                }
                ScaleLine();
            }
            else
                continue;                   // totally obscured
        }
    }
}


/*
=======================
=
= SimpleScaleShape
=
= NO CLIPPING against wallheight[], height in pixels
= Used for weapon overlay drawing
=
=======================
*/

void SimpleScaleShape (int xcenter, int shapenum, unsigned height)
{
    t_compshape *shape;
    unsigned     scale, srcx, stopx;
    uint16_t    *cmdptr;
    int          displayheight;
    long         step;

    shape = PM_GetSpritePage(shapenum);

    scale = height >> 1;
    if (!scale || scale > (unsigned)maxscale)
        return;

    displayheight = scale * 2;
    linescale = displayheight;

    step = ((long)displayheight << 16) / 64;

    //
    // scale to the left (from pixel 31 to shape->leftpix)
    //
    srcx = 32;
    slinex = xcenter;
    stopx = shape->leftpix;
    cmdptr = &shape->dataofs[31 - stopx];

    while (--srcx >= stopx)
    {
        int col_start = (int)(((long)srcx * step) >> 16);
        int col_end   = (int)(((long)(srcx + 1) * step) >> 16);
        slinewidth = col_end - col_start;
        if (slinewidth <= 0)
        {
            cmdptr--;
            continue;
        }

        linecmds = (uint16_t *)((byte *)shape + *cmdptr);
        cmdptr--;

        slinex -= slinewidth;
        ScaleLine();
    }


    //
    // scale to the right
    //
    slinex = xcenter;
    stopx = shape->rightpix;
    if (shape->leftpix < 31)
    {
        srcx = 31;
        cmdptr = &shape->dataofs[32 - shape->leftpix];
    }
    else
    {
        srcx = shape->leftpix - 1;
        cmdptr = &shape->dataofs[0];
    }
    slinewidth = 0;

    while (++srcx <= stopx)
    {
        int col_start = (int)(((long)srcx * step) >> 16);
        int col_end   = (int)(((long)(srcx + 1) * step) >> 16);
        slinewidth = col_end - col_start;
        if (slinewidth <= 0)
        {
            cmdptr++;
            continue;
        }

        linecmds = (uint16_t *)((byte *)shape + *cmdptr);
        cmdptr++;

        ScaleLine();
        slinex += slinewidth;
    }
}
