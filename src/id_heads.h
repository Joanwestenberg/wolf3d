// ID_GLOB.H - macOS/SDL2 port

#ifndef __ID_HEADS_H__
#define __ID_HEADS_H__

#include "wl_compat.h"

#define __ID_GLOB__
#include "version.h"

//--------------------------------------------------------------------------

extern	byte		signon[];

#define	introscn	signon

#ifdef JAPAN
#ifdef JAPDEMO
#include "foreign/japan/gfxv_wj1.h"
#else
#include "foreign/japan/gfxv_wj6.h"
#endif
#include "audiowl6.h"
#include "mapswl6.h"
#else

#ifndef SPEAR

#include "gfxv_wl6.h"
#include "audiowl6.h"
#include "mapswl6.h"

#else

#ifndef SPEARDEMO
#include "gfxv_sod.h"
#include "audiosod.h"
#include "mapssod.h"
#else
#include "gfxv_sdm.h"
#include "audiosdm.h"
#include "mapssdm.h"
#endif

#endif
#endif
//-----------------


#define GREXT	"VGA"

#define	nil	((void *)0)

#include "id_mm.h"
#include "id_pm.h"
#include "id_ca.h"
#include "id_vl.h"
#include "id_vh.h"
#include "id_in.h"
#include "id_sd.h"
#include "id_us.h"


void	Quit (char *error);

//
// replacing refresh manager with custom routines
//

#define	PORTTILESWIDE		20
#define	PORTTILESHIGH		13

#define UPDATEWIDE			PORTTILESWIDE
#define UPDATEHIGH			PORTTILESHIGH

#define	MAXTICS				10
#define DEMOTICS			4

#define	UPDATETERMINATE	0x0301

extern	unsigned	mapwidth,mapheight,tics;
extern	boolean		compatability;

extern	byte		*updateptr;
extern	unsigned	uwidthtable[UPDATEHIGH];
extern	unsigned	blockstarts[UPDATEWIDE*UPDATEHIGH];

extern	byte		fontcolor,backcolor;

#define SETFONTCOLOR(f,b) fontcolor=f;backcolor=b;

#endif
