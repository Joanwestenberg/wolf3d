// WL_DRAW.C - macOS ARM64/SDL2 port

#include "wl_def.h"
#include <stdint.h>

//#define DEBUGWALLS
//#define DEBUGTICS

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

// the door is the last picture before the sprites
#define DOORWALL	(PMSpriteStart-8)

#define ACTORSIZE	0x4000

/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/


#ifdef DEBUGWALLS
unsigned screenloc[3]= {0,0,0};
#else
unsigned screenloc[3]= {PAGE1START,PAGE2START,PAGE3START};
#endif
unsigned freelatch = FREESTART;

long 	lasttimecount;
long 	frameon;

unsigned	wallheight[MAXVIEWWIDTH];

fixed	tileglobal	= TILEGLOBAL;
fixed	mindist		= MINDIST;


//
// math tables
//
int			pixelangle[MAXVIEWWIDTH];
long		finetangent[FINEANGLES/4];
fixed 		sintable[ANGLES+ANGLES/4], *costable = sintable+(ANGLES/4);

//
// refresh variables
//
fixed	viewx,viewy;			// the focal point
int		viewangle;
fixed	viewsin,viewcos;



fixed	FixedByFrac (fixed a, fixed b);
void	TransformActor (objtype *ob);
void	BuildTables (void);
void	ClearScreen (void);
int		CalcRotate (objtype *ob);
void	DrawScaleds (void);
void	CalcTics (void);
void	FixOfs (void);
void	ThreeDRefresh (void);



//
// wall optimization variables
//
int		lastside;		// true for vertical
long	lastintercept;
int		lasttilehit;


//
// ray tracing variables
//
int			focaltx,focalty,viewtx,viewty;

int			midangle,angle;
unsigned	xpartial,ypartial;
unsigned	xpartialup,xpartialdown,ypartialup,ypartialdown;
unsigned	xinttile,yinttile;

unsigned	tilehit;
unsigned	pixx;

int		xtile,ytile;
int		xtilestep,ytilestep;
long	xintercept,yintercept;
long	xstep,ystep;

int		horizwall[MAXWALLTILES],vertwall[MAXWALLTILES];


/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/


/*
============================================================================

			   3 - D  DEFINITIONS

============================================================================
*/


//==========================================================================


/*
========================
=
= FixedByFrac
=
= multiply a 16/16 bit, 2's complement fixed point number by a 16 bit
= fraction, passed as a signed magnitude 32 bit number
=
========================
*/

fixed FixedByFrac (fixed a, fixed b)
{
	// b is signed-magnitude: bit 31 is sign, low 16 bits are fraction magnitude
	int sign = (b >> 31) & 1;  // extract sign bit
	unsigned frac = (unsigned)(b & 0xFFFF);  // magnitude (low 16 bits)

	// make a positive, track sign
	int64_t aa = (int64_t)a;
	if (aa < 0)
	{
		aa = -aa;
		sign = !sign;
	}

	// multiply unsigned magnitude by fraction
	int64_t result = (aa * (int64_t)frac) >> 16;

	// apply sign
	if (sign)
		result = -result;

	return (fixed)result;
}

//==========================================================================

/*
========================
=
= TransformActor
=
= Takes paramaters:
=   gx,gy		: globalx/globaly of point
=
= globals:
=   viewx,viewy		: point of view
=   viewcos,viewsin	: sin/cos of viewangle
=   scale		: conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
========================
*/


//
// transform actor
//
void TransformActor (objtype *ob)
{
	fixed gx,gy,gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
	gx = ob->x-viewx;
	gy = ob->y-viewy;

//
// calculate newx
//
	gxt = FixedByFrac(gx,viewcos);
	gyt = FixedByFrac(gy,viewsin);
	nx = gxt-gyt-ACTORSIZE;		// fudge the shape forward a bit, because
								// the midpoint could put parts of the shape
								// into an adjacent wall

//
// calculate newy
//
	gxt = FixedByFrac(gx,viewsin);
	gyt = FixedByFrac(gy,viewcos);
	ny = gyt+gxt;

//
// calculate perspective ratio
//
	ob->transx = nx;
	ob->transy = ny;

	if (nx<mindist)			// too close, don't overflow the divide
	{
	  ob->viewheight = 0;
	  return;
	}

	ob->viewx = centerx + ny*scale/nx;

//
// calculate height (heightnumerator/(nx>>8))
//
	ob->viewheight = (int)(heightnumerator / (nx >> 8));
}

//==========================================================================

/*
========================
=
= TransformTile
=
= Takes paramaters:
=   tx,ty		: tile the object is centered in
=
= globals:
=   viewx,viewy		: point of view
=   viewcos,viewsin	: sin/cos of viewangle
=   scale		: conversion from global value to screen value
=
= sets:
=   screenx,transx,transy,screenheight: projected edge location and size
=
= Returns true if the tile is withing getting distance
=
========================
*/

boolean TransformTile (int tx, int ty, int *dispx, int *dispheight)
{
	fixed gx,gy,gxt,gyt,nx,ny;

//
// translate point to view centered coordinates
//
	gx = ((long)tx<<TILESHIFT)+0x8000-viewx;
	gy = ((long)ty<<TILESHIFT)+0x8000-viewy;

//
// calculate newx
//
	gxt = FixedByFrac(gx,viewcos);
	gyt = FixedByFrac(gy,viewsin);
	nx = gxt-gyt-0x2000;		// 0x2000 is size of object

//
// calculate newy
//
	gxt = FixedByFrac(gx,viewsin);
	gyt = FixedByFrac(gy,viewcos);
	ny = gyt+gxt;


//
// calculate perspective ratio
//
	if (nx<mindist)			// too close, don't overflow the divide
	{
		*dispheight = 0;
		return false;
	}

	*dispx = centerx + ny*scale/nx;

//
// calculate height (heightnumerator/(nx>>8))
//
	*dispheight = (int)(heightnumerator / (nx >> 8));

//
// see if it should be grabbed
//
	if (nx<TILEGLOBAL && ny>-TILEGLOBAL/2 && ny<TILEGLOBAL/2)
		return true;
	else
		return false;
}

//==========================================================================

/*
====================
=
= CalcHeight
=
= Calculates the height of xintercept,yintercept from viewx,viewy
=
====================
*/

int	CalcHeight (void)
{
	fixed gxt,gyt,nx;
	long	gx,gy;

	gx = xintercept-viewx;
	gxt = FixedByFrac(gx,viewcos);

	gy = yintercept-viewy;
	gyt = FixedByFrac(gy,viewsin);

	nx = gxt-gyt;

  //
  // calculate perspective ratio (heightnumerator/(nx>>8))
  //
	if (nx<mindist)
		nx=mindist;			// don't let divide overflow

	return (int)(heightnumerator / (nx >> 8));
}


//==========================================================================

/*
===================
=
= ScalePost
=
===================
*/

byte		*postsource;
unsigned	postx;
unsigned	postwidth;

void ScalePost (void)
{
	int		height;
	int		toprow, bottomrow;
	int		srcfrac, srcstep;
	int		x, y;
	byte	*src;
	byte	col;

	height = wallheight[postx] >> 2;  // height in pixels (wallheight has 2 fractional bits)
	if (height <= 0)
		return;

	// clamp to max scale height
	if (height > maxscaleshl2 >> 2)
		height = maxscaleshl2 >> 2;

	// calculate top and bottom rows
	toprow = (viewheight - height) / 2;
	bottomrow = toprow + height;

	// source step: we need to map height pixels to 64 source texels
	srcstep = (64 << 16) / height;  // 16.16 fixed point step
	srcfrac = 0;

	// adjust if top is clipped
	if (toprow < 0)
	{
		srcfrac += (-toprow) * srcstep;
		toprow = 0;
	}
	if (bottomrow > viewheight)
		bottomrow = viewheight;

	src = postsource;
	if (!src)
		return;

	for (y = toprow; y < bottomrow; y++)
	{
		int texel = srcfrac >> 16;
		if (texel > 63)
			texel = 63;
		col = src[texel];

		// draw postwidth pixels horizontally
		for (x = postx; x < (int)(postx + postwidth); x++)
		{
			if (x >= 0 && x < viewwidth)
				sdl_framebuffer[(y + screenofs / 320) * 320 + (screenofs % 320) + x] = col;
		}

		srcfrac += srcstep;
	}
}

void FarScalePost (void)
{
	ScalePost ();
}


/*
====================
=
= HitVertWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

void HitVertWall (void)
{
	int			wallpic;
	unsigned	texture;

	texture = (yintercept>>4)&0xfc0;
	if (xtilestep == -1)
	{
		texture = 0xfc0-texture;
		xintercept += TILEGLOBAL;
	}
	wallheight[pixx] = CalcHeight();

	if (lastside==1 && lastintercept == xtile && lasttilehit == tilehit)
	{
		// in the same wall type as last time, so check for optimized draw
		if (texture == (unsigned)((uintptr_t)postsource & 0xFFF))
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		else
		{
			ScalePost ();
			postsource = (byte *)PM_GetPage(wallpic) + texture;
			// wallpic not set here -- reuse the page base from postsource
			postsource = (byte *)((uintptr_t)postsource & ~0xFFFUL) + texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
	// new wall
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();

		lastside = 1;
		lastintercept = xtile;

		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		if (tilehit & 0x40)
		{								// check for adjacent doors
			ytile = yintercept>>TILESHIFT;
			if ( tilemap[xtile-xtilestep][ytile]&0x80 )
				wallpic = DOORWALL+3;
			else
				wallpic = vertwall[tilehit & ~0x40];
		}
		else
			wallpic = vertwall[tilehit];

		postsource = (byte *)PM_GetPage(wallpic) + texture;
	}
}


/*
====================
=
= HitHorizWall
=
= tilehit bit 7 is 0, because it's not a door tile
= if bit 6 is 1 and the adjacent tile is a door tile, use door side pic
=
====================
*/

void HitHorizWall (void)
{
	int			wallpic;
	unsigned	texture;

	texture = (xintercept>>4)&0xfc0;
	if (ytilestep == -1)
		yintercept += TILEGLOBAL;
	else
		texture = 0xfc0-texture;
	wallheight[pixx] = CalcHeight();

	if (lastside==0 && lastintercept == ytile && lasttilehit == tilehit)
	{
		// in the same wall type as last time, so check for optimized draw
		if (texture == (unsigned)((uintptr_t)postsource & 0xFFF))
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		else
		{
			ScalePost ();
			postsource = (byte *)((uintptr_t)postsource & ~0xFFFUL) + texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
	// new wall
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();

		lastside = 0;
		lastintercept = ytile;

		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		if (tilehit & 0x40)
		{								// check for adjacent doors
			xtile = xintercept>>TILESHIFT;
			if ( tilemap[xtile][ytile-ytilestep]&0x80 )
				wallpic = DOORWALL+2;
			else
				wallpic = horizwall[tilehit & ~0x40];
		}
		else
			wallpic = horizwall[tilehit];

		postsource = (byte *)PM_GetPage(wallpic) + texture;
	}

}

//==========================================================================

/*
====================
=
= HitHorizDoor
=
====================
*/

void HitHorizDoor (void)
{
	unsigned	texture,doorpage,doornum;

	doornum = tilehit&0x7f;
	texture = ( (xintercept-doorposition[doornum]) >> 4) &0xfc0;

	wallheight[pixx] = CalcHeight();

	if (lasttilehit == tilehit)
	{
	// in the same door as last time, so check for optimized draw
		if (texture == (unsigned)((uintptr_t)postsource & 0xFFF))
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		else
		{
			ScalePost ();
			postsource = (byte *)((uintptr_t)postsource & ~0xFFFUL) + texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();			// draw last post
	// first pixel in this door
		lastside = 2;
		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		switch (doorobjlist[doornum].lock)
		{
		case dr_normal:
			doorpage = DOORWALL;
			break;
		case dr_lock1:
		case dr_lock2:
		case dr_lock3:
		case dr_lock4:
			doorpage = DOORWALL+6;
			break;
		case dr_elevator:
			doorpage = DOORWALL+4;
			break;
		}

		postsource = (byte *)PM_GetPage(doorpage) + texture;
	}
}

//==========================================================================

/*
====================
=
= HitVertDoor
=
====================
*/

void HitVertDoor (void)
{
	unsigned	texture,doorpage,doornum;

	doornum = tilehit&0x7f;
	texture = ( (yintercept-doorposition[doornum]) >> 4) &0xfc0;

	wallheight[pixx] = CalcHeight();

	if (lasttilehit == tilehit)
	{
	// in the same door as last time, so check for optimized draw
		if (texture == (unsigned)((uintptr_t)postsource & 0xFFF))
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		else
		{
			ScalePost ();
			postsource = (byte *)((uintptr_t)postsource & ~0xFFFUL) + texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();			// draw last post
	// first pixel in this door
		lastside = 2;
		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		switch (doorobjlist[doornum].lock)
		{
		case dr_normal:
			doorpage = DOORWALL;
			break;
		case dr_lock1:
		case dr_lock2:
		case dr_lock3:
		case dr_lock4:
			doorpage = DOORWALL+6;
			break;
		case dr_elevator:
			doorpage = DOORWALL+4;
			break;
		}

		postsource = (byte *)PM_GetPage(doorpage+1) + texture;
	}
}

//==========================================================================


/*
====================
=
= HitHorizPWall
=
= A pushable wall in action has been hit
=
====================
*/

void HitHorizPWall (void)
{
	int			wallpic;
	unsigned	texture,offset;

	texture = (xintercept>>4)&0xfc0;
	offset = pwallpos<<10;
	if (ytilestep == -1)
		yintercept += TILEGLOBAL-offset;
	else
	{
		texture = 0xfc0-texture;
		yintercept += offset;
	}

	wallheight[pixx] = CalcHeight();

	if (lasttilehit == tilehit)
	{
		// in the same wall type as last time, so check for optimized draw
		if (texture == (unsigned)((uintptr_t)postsource & 0xFFF))
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		else
		{
			ScalePost ();
			postsource = (byte *)((uintptr_t)postsource & ~0xFFFUL) + texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
	// new wall
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();

		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		wallpic = horizwall[tilehit&63];

		postsource = (byte *)PM_GetPage(wallpic) + texture;
	}

}


/*
====================
=
= HitVertPWall
=
= A pushable wall in action has been hit
=
====================
*/

void HitVertPWall (void)
{
	int			wallpic;
	unsigned	texture,offset;

	texture = (yintercept>>4)&0xfc0;
	offset = pwallpos<<10;
	if (xtilestep == -1)
	{
		xintercept += TILEGLOBAL-offset;
		texture = 0xfc0-texture;
	}
	else
		xintercept += offset;

	wallheight[pixx] = CalcHeight();

	if (lasttilehit == tilehit)
	{
		// in the same wall type as last time, so check for optimized draw
		if (texture == (unsigned)((uintptr_t)postsource & 0xFFF))
		{
		// wide scale
			postwidth++;
			wallheight[pixx] = wallheight[pixx-1];
			return;
		}
		else
		{
			ScalePost ();
			postsource = (byte *)((uintptr_t)postsource & ~0xFFFUL) + texture;
			postwidth = 1;
			postx = pixx;
		}
	}
	else
	{
	// new wall
		if (lastside != -1)				// if not the first scaled post
			ScalePost ();

		lasttilehit = tilehit;
		postx = pixx;
		postwidth = 1;

		wallpic = vertwall[tilehit&63];

		postsource = (byte *)PM_GetPage(wallpic) + texture;
	}

}

//==========================================================================

//==========================================================================

unsigned vgaCeiling[]=
{
#ifndef SPEAR
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0xbfbf,
 0x4e4e,0x4e4e,0x4e4e,0x1d1d,0x8d8d,0x4e4e,0x1d1d,0x2d2d,0x1d1d,0x8d8d,
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x1d1d,0x2d2d,0xdddd,0x1d1d,0x1d1d,0x9898,

 0x1d1d,0x9d9d,0x2d2d,0xdddd,0xdddd,0x9d9d,0x2d2d,0x4d4d,0x1d1d,0xdddd,
 0x7d7d,0x1d1d,0x2d2d,0x2d2d,0xdddd,0xd7d7,0x1d1d,0x1d1d,0x1d1d,0x2d2d,
 0x1d1d,0x1d1d,0x1d1d,0x1d1d,0xdddd,0xdddd,0x7d7d,0xdddd,0xdddd,0xdddd
#else
 0x6f6f,0x4f4f,0x1d1d,0xdede,0xdfdf,0x2e2e,0x7f7f,0x9e9e,0xaeae,0x7f7f,
 0x1d1d,0xdede,0xdfdf,0xdede,0xdfdf,0xdede,0xe1e1,0xdcdc,0x2e2e,0x1d1d,0xdcdc
#endif
};

/*
=====================
=
= VGAClearScreen
=
=====================
*/

void VGAClearScreen (void)
{
	unsigned ceilingword = vgaCeiling[gamestate.episode*10+mapon];
	byte ceilingcolor = (byte)(ceilingword & 0xFF);
	byte floorcolor = 0x19;
	int x, y;
	int halfheight = viewheight / 2;
	int ybase = screenofs / 320;  // vertical offset in the framebuffer
	int xbase = screenofs % 320;  // horizontal offset in the framebuffer

	// draw ceiling (top half)
	for (y = 0; y < halfheight; y++)
	{
		for (x = 0; x < viewwidth; x++)
		{
			sdl_framebuffer[(ybase + y) * 320 + xbase + x] = ceilingcolor;
		}
	}

	// draw floor (bottom half)
	for (y = halfheight; y < viewheight; y++)
	{
		for (x = 0; x < viewwidth; x++)
		{
			sdl_framebuffer[(ybase + y) * 320 + xbase + x] = floorcolor;
		}
	}
}

//==========================================================================

/*
=====================
=
= CalcRotate
=
=====================
*/

int	CalcRotate (objtype *ob)
{
	int	angle,viewangle;

	// this isn't exactly correct, as it should vary by a trig value,
	// but it is close enough with only eight rotations

	viewangle = player->angle + (centerx - ob->viewx)/8;

	if (ob->obclass == rocketobj || ob->obclass == hrocketobj)
		angle =  (viewangle-180)- ob->angle;
	else
		angle =  (viewangle-180)- dirangle[ob->dir];

	angle+=ANGLES/16;
	while (angle>=ANGLES)
		angle-=ANGLES;
	while (angle<0)
		angle+=ANGLES;

	if (ob->state->rotate == 2)             // 2 rotation pain frame
		return 4*(angle/(ANGLES/2));        // seperated by 3 (art layout...)

	return angle/(ANGLES/8);
}


/*
=====================
=
= DrawScaleds
=
= Draws all objects that are visable
=
=====================
*/

#define MAXVISABLE	50

typedef struct
{
	int	viewx,
		viewheight,
		shapenum;
} visobj_t;

visobj_t	vislist[MAXVISABLE],*visptr,*visstep,*farthest;

void DrawScaleds (void)
{
	int 		i,j,least,numvisable,height;
	memptr		shape;
	byte		*tilespot,*visspot;
	int			shapenum;
	unsigned	spotloc;

	statobj_t	*statptr;
	objtype		*obj;

	visptr = &vislist[0];

//
// place static objects
//
	for (statptr = &statobjlist[0] ; statptr !=laststatobj ; statptr++)
	{
		if ((visptr->shapenum = statptr->shapenum) == -1)
			continue;						// object has been deleted

		if (!*statptr->visspot)
			continue;						// not visable

		if (TransformTile (statptr->tilex,statptr->tiley
			,&visptr->viewx,&visptr->viewheight) && statptr->flags & FL_BONUS)
		{
			GetBonus (statptr);
			continue;
		}

		if (!visptr->viewheight)
			continue;						// to close to the object

		if (visptr < &vislist[MAXVISABLE-1])	// don't let it overflow
			visptr++;
	}

//
// place active objects
//
	for (obj = player->next;obj;obj=obj->next)
	{
		if (!(visptr->shapenum = obj->state->shapenum))
			continue;						// no shape

		spotloc = (obj->tilex<<6)+obj->tiley;	// optimize: keep in struct?
		visspot = &spotvis[0][0]+spotloc;
		tilespot = &tilemap[0][0]+spotloc;

		//
		// could be in any of the nine surrounding tiles
		//
		if (*visspot
		|| ( *(visspot-1) && !*(tilespot-1) )
		|| ( *(visspot+1) && !*(tilespot+1) )
		|| ( *(visspot-65) && !*(tilespot-65) )
		|| ( *(visspot-64) && !*(tilespot-64) )
		|| ( *(visspot-63) && !*(tilespot-63) )
		|| ( *(visspot+65) && !*(tilespot+65) )
		|| ( *(visspot+64) && !*(tilespot+64) )
		|| ( *(visspot+63) && !*(tilespot+63) ) )
		{
			obj->active = true;
			TransformActor (obj);
			if (!obj->viewheight)
				continue;						// too close or far away

			visptr->viewx = obj->viewx;
			visptr->viewheight = obj->viewheight;
			if (visptr->shapenum == -1)
				visptr->shapenum = obj->temp1;	// special shape

			if (obj->state->rotate)
				visptr->shapenum += CalcRotate (obj);

			if (visptr < &vislist[MAXVISABLE-1])	// don't let it overflow
				visptr++;
			obj->flags |= FL_VISABLE;
		}
		else
			obj->flags &= ~FL_VISABLE;
	}

//
// draw from back to front
//
	numvisable = visptr-&vislist[0];

	if (!numvisable)
		return;									// no visable objects

	for (i = 0; i<numvisable; i++)
	{
		least = 32000;
		for (visstep=&vislist[0] ; visstep<visptr ; visstep++)
		{
			height = visstep->viewheight;
			if (height < least)
			{
				least = height;
				farthest = visstep;
			}
		}
		//
		// draw farthest
		//
		ScaleShape(farthest->viewx,farthest->shapenum,farthest->viewheight);

		farthest->viewheight = 32000;
	}

}

//==========================================================================

/*
==============
=
= DrawPlayerWeapon
=
= Draw the player's hands
=
==============
*/

int	weaponscale[NUMWEAPONS] = {SPR_KNIFEREADY,SPR_PISTOLREADY
	,SPR_MACHINEGUNREADY,SPR_CHAINREADY};

void DrawPlayerWeapon (void)
{
	int	shapenum;

#ifndef SPEAR
	if (gamestate.victoryflag)
	{
		if (player->state == &s_deathcam && (TimeCount&32) )
			SimpleScaleShape(viewwidth/2,SPR_DEATHCAM,viewheight+1);
		return;
	}
#endif

	if (gamestate.weapon != -1)
	{
		shapenum = weaponscale[gamestate.weapon]+gamestate.weaponframe;
		SimpleScaleShape(viewwidth/2,shapenum,viewheight+1);
	}

	if (demorecord || demoplayback)
		SimpleScaleShape(viewwidth/2,SPR_DEMO,viewheight+1);
}


//==========================================================================


/*
=====================
=
= CalcTics
=
=====================
*/

void CalcTics (void)
{
	long	newtime,oldtimecount;

//
// calculate tics since last refresh for adaptive timing
//
	if (lasttimecount > TimeCount)
		TimeCount = lasttimecount;		// if the game was paused a LONG time

	do
	{
		newtime = TimeCount;
		tics = newtime-lasttimecount;
		if (!tics)
			SDL_Delay(1);		// yield CPU while waiting for next tick
	} while (!tics);			// make sure at least one tic passes

	lasttimecount = newtime;

#ifdef FILEPROFILE
		strcpy (scratch,"\tTics:");
		itoa (tics,str,10);
		strcat (scratch,str);
		strcat (scratch,"\n");
		write (profilehandle,scratch,strlen(scratch));
#endif

	if (tics>MAXTICS)
	{
		TimeCount -= (tics-MAXTICS);
		tics = MAXTICS;
	}
}


//==========================================================================


/*
========================
=
= FixOfs
=
= No-op in SDL2 port (no VGA page management)
=
========================
*/

void	FixOfs (void)
{
	// No-op: no VGA page flipping in SDL2 port
}


//==========================================================================

/*
====================
=
= AsmRefresh
=
= Raycaster core - casts one ray per screen column using DDA
=
====================
*/

void AsmRefresh (void)
{
	int     angle;
	long    xstep_local, ystep_local;
	long    xintercept_local, yintercept_local;
	unsigned xtile_local, ytile_local;
	int     xtilestep_local, ytilestep_local;
	int     xspot, yspot;
	unsigned tile;

	for (pixx = 0; pixx < (unsigned)viewwidth; pixx++)
	{
		angle = midangle + pixelangle[pixx];

		if (angle < 0)
			angle += FINEANGLES;
		if (angle >= FINEANGLES)
			angle -= FINEANGLES;

		//
		// setup xstep/ystep based on angle quadrant
		//
		if (angle < ANG90)
		{
			// first quadrant: right and up
			xtilestep = 1;
			ytilestep = -1;

			xstep = finetangent[ANG90 - 1 - angle];
			ystep = -finetangent[angle];

			xintercept = ((long)viewtx << TILESHIFT) + TILEGLOBAL;
			xintercept += FixedByFrac(xpartialup, xstep);
			xtile = viewtx + 1;

			yintercept = ((long)viewty << TILESHIFT);
			yintercept += FixedByFrac(ypartialup, ystep);
			ytile = viewty - 1;
		}
		else if (angle < ANG180)
		{
			// second quadrant: left and up
			xtilestep = -1;
			ytilestep = -1;

			xstep = -finetangent[angle - ANG90];
			ystep = -finetangent[ANG180 - 1 - angle];

			xintercept = ((long)viewtx << TILESHIFT);
			xintercept += FixedByFrac(xpartialdown, xstep);
			xtile = viewtx - 1;

			yintercept = ((long)viewty << TILESHIFT);
			yintercept += FixedByFrac(ypartialup, ystep);
			ytile = viewty - 1;
		}
		else if (angle < ANG270)
		{
			// third quadrant: left and down
			xtilestep = -1;
			ytilestep = 1;

			xstep = -finetangent[ANG270 - 1 - angle];
			ystep = finetangent[angle - ANG180];

			xintercept = ((long)viewtx << TILESHIFT);
			xintercept += FixedByFrac(xpartialdown, xstep);
			xtile = viewtx - 1;

			yintercept = ((long)viewty << TILESHIFT) + TILEGLOBAL;
			yintercept += FixedByFrac(ypartialdown, ystep);
			ytile = viewty + 1;
		}
		else
		{
			// fourth quadrant: right and down
			xtilestep = 1;
			ytilestep = 1;

			xstep = finetangent[angle - ANG270];
			ystep = finetangent[ANG360 - 1 - angle];

			xintercept = ((long)viewtx << TILESHIFT) + TILEGLOBAL;
			xintercept += FixedByFrac(xpartialup, xstep);
			xtile = viewtx + 1;

			yintercept = ((long)viewty << TILESHIFT) + TILEGLOBAL;
			yintercept += FixedByFrac(ypartialdown, ystep);
			ytile = viewty + 1;
		}

		//
		// trace the ray through the map via DDA
		//
		// We alternate checking vertical (x) and horizontal (y) grid intersections
		//
		for (;;)
		{
			// check vertical (x) grid line intersection
			xspot = (xtile << 6) + (yintercept >> TILESHIFT);
			if (xspot >= 0 && xspot < MAPSIZE * MAPSIZE)
			{
				tile = ((byte *)tilemap)[xspot];
				if (tile)
				{
					tilehit = tile;
					if (tilehit & 0x80)
					{
						// door tile
						long intercept;
						int doornum;

						// check door at half-tile offset
						intercept = yintercept + (ystep >> 1);
						doornum = tilehit & 0x7f;

						if ((intercept >> TILESHIFT) != ytile)
							goto passvert;  // stepped into next tile

						// check if door is open enough
						if ( (unsigned)(intercept >> 4) & 0xfc0
							 && ((unsigned)((intercept - doorposition[doornum]) >> 4) & 0xfc0) <= 0xfc0 )
						{
							yintercept = intercept;
							xintercept = ((long)xtile << TILESHIFT) + (TILEGLOBAL / 2);
							HitVertDoor();
							break;
						}
						goto passvert;
					}
					else if (tilehit & 0x40)
					{
						// pushwall check
						// check if we hit the pushwall offset
						long intercept = yintercept + ((long)pwallpos * ystep) / 64;

						xintercept = ((long)xtile << TILESHIFT) + ((long)pwallpos << 10);
						if (xtilestep == -1)
							xintercept = ((long)xtile << TILESHIFT) + TILEGLOBAL - ((long)pwallpos << 10);

						yintercept = intercept;
						HitVertWall();
						break;
					}
					else
					{
						// solid wall
						xintercept = (long)xtile << TILESHIFT;
						HitVertWall();
						break;
					}
				}
			}

passvert:
			// advance to next vertical grid line
			xtile += xtilestep;
			yintercept += ystep;

			// check horizontal (y) grid line intersection
			yspot = (ytile << 6) + (xintercept >> TILESHIFT);
			if (yspot >= 0 && yspot < MAPSIZE * MAPSIZE)
			{
				tile = ((byte *)tilemap)[yspot];
				if (tile)
				{
					tilehit = tile;
					if (tilehit & 0x80)
					{
						// door tile
						long intercept;
						int doornum;

						intercept = xintercept + (xstep >> 1);
						doornum = tilehit & 0x7f;

						if ((intercept >> TILESHIFT) != xtile)
							goto passhoriz;

						if ( (unsigned)(intercept >> 4) & 0xfc0
							 && ((unsigned)((intercept - doorposition[doornum]) >> 4) & 0xfc0) <= 0xfc0 )
						{
							xintercept = intercept;
							yintercept = ((long)ytile << TILESHIFT) + (TILEGLOBAL / 2);
							HitHorizDoor();
							break;
						}
						goto passhoriz;
					}
					else if (tilehit & 0x40)
					{
						// pushwall
						long intercept = xintercept + ((long)pwallpos * xstep) / 64;

						yintercept = ((long)ytile << TILESHIFT) + ((long)pwallpos << 10);
						if (ytilestep == -1)
							yintercept = ((long)ytile << TILESHIFT) + TILEGLOBAL - ((long)pwallpos << 10);

						xintercept = intercept;
						HitHorizWall();
						break;
					}
					else
					{
						// solid wall
						yintercept = (long)ytile << TILESHIFT;
						HitHorizWall();
						break;
					}
				}
			}

passhoriz:
			// advance to next horizontal grid line
			ytile += ytilestep;
			xintercept += xstep;
		}
	}
}


//==========================================================================

/*
====================
=
= WallRefresh
=
====================
*/

void WallRefresh (void)
{
//
// set up variables for this view
//
	viewangle = player->angle;
	midangle = viewangle*(FINEANGLES/ANGLES);
	viewsin = sintable[viewangle];
	viewcos = costable[viewangle];
	viewx = player->x - FixedByFrac(focallength,viewcos);
	viewy = player->y + FixedByFrac(focallength,viewsin);

	focaltx = viewx>>TILESHIFT;
	focalty = viewy>>TILESHIFT;

	viewtx = player->x >> TILESHIFT;
	viewty = player->y >> TILESHIFT;

	xpartialdown = viewx&(TILEGLOBAL-1);
	xpartialup = TILEGLOBAL-xpartialdown;
	ypartialdown = viewy&(TILEGLOBAL-1);
	ypartialup = TILEGLOBAL-ypartialdown;

	lastside = -1;			// the first pixel is on a new wall
	AsmRefresh ();
	ScalePost ();			// no more optimization on last post
}

//==========================================================================

/*
========================
=
= ThreeDRefresh
=
========================
*/

void	ThreeDRefresh (void)
{
//
// clear out the traced array
//
	memset(spotvis, 0, sizeof(spotvis));

//
// follow the walls from there to the right, drawing as we go
//
	VGAClearScreen ();

	WallRefresh ();

//
// draw all the scaled images
//
	DrawScaleds();			// draw scaled stuff
	DrawPlayerWeapon ();	// draw player's hands

//
// show screen and time last cycle
//
	if (fizzlein)
	{
		FizzleFade(bufferofs,displayofs+screenofs,viewwidth,viewheight,20,false);
		fizzlein = false;

		lasttimecount = TimeCount = 0;		// don't make a big tic count
	}

	VW_UpdateScreen();

	frameon++;
	PM_NextFrame();
}


//===========================================================================
