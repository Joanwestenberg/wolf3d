// ID_CA.C

// this has been customized for WOLF
// macOS ARM64 / SDL2 port - all x86 asm removed, pure C

/*
=============================================================================

Id Software Caching Manager
---------------------------

Must be started BEFORE the memory manager, because it needs to get the headers
loaded into the data segment

=============================================================================
*/

#include "id_heads.h"

#include <stdint.h>
#include <ctype.h>

#define THREEBYTEGRSTARTS

/*
=====================
=
= CAL_OpenCaseInsensitive
=
= Try to open a file trying original case, then lowercase, then uppercase.
= macOS default APFS is case-insensitive, but case-sensitive volumes exist.
=
=====================
*/
static int CAL_OpenCaseInsensitive(const char *filename, int flags, int mode)
{
	int handle;
	char buf[64];
	int i;

	// Try as-is first
	handle = open(filename, flags, mode);
	if (handle != -1)
		return handle;

	// Try lowercase
	for (i = 0; filename[i] && i < 63; i++)
		buf[i] = tolower((unsigned char)filename[i]);
	buf[i] = '\0';
	handle = open(buf, flags, mode);
	if (handle != -1)
		return handle;

	// Try uppercase
	for (i = 0; filename[i] && i < 63; i++)
		buf[i] = toupper((unsigned char)filename[i]);
	buf[i] = '\0';
	handle = open(buf, flags, mode);
	return handle;
}

/*
=============================================================================

						 LOCAL CONSTANTS

=============================================================================
*/

typedef struct
{
  unsigned short bit0,bit1;	// 0-255 is a leaf byte, 256+ is a node index
} huffnode;


typedef struct
{
	uint16_t	RLEWtag;
	int32_t		headeroffsets[100];
	byte		tileinfo[];
} mapfiletype;


/*
=============================================================================

						 GLOBAL VARIABLES

=============================================================================
*/

byte 		*tinf;
int			mapon;

unsigned	*mapsegs[MAPPLANES];
maptype		*mapheaderseg[NUMMAPS];
byte		*audiosegs[NUMSNDCHUNKS];
void		*grsegs[NUMCHUNKS];

byte		grneeded[NUMCHUNKS];
byte		ca_levelbit,ca_levelnum;

int			profilehandle,debughandle;

char		audioname[13]="AUDIO.";

/*
=============================================================================

						 LOCAL VARIABLES

=============================================================================
*/

char extension[5],	// Need a string, not constant to change cache files
     gheadname[10]=GREXT"HEAD.",
     gfilename[10]=GREXT"GRAPH.",
     gdictname[10]=GREXT"DICT.",
     mheadname[10]="MAPHEAD.",
     mfilename[10]="MAPTEMP.",
     aheadname[10]="AUDIOHED.",
     afilename[10]="AUDIOT.";

void CA_CannotOpen(char *string);

int32_t		*grstarts;	// array of offsets in egagraph, -1 for sparse (32-bit in data files)
int32_t		*audiostarts;	// array of offsets in audio / audiot (32-bit in data files)

huffnode	grhuffman[255];
huffnode	audiohuffman[255];


int			grhandle;		// handle to EGAGRAPH
int			maphandle;		// handle to MAPTEMP / GAMEMAPS
int			audiohandle;	// handle to AUDIOT / AUDIO

long		chunkcomplen,chunkexplen;

SDMode		oldsoundmode;



void	CAL_CarmackExpand (unsigned *source, unsigned *dest,
		unsigned length);


#ifdef THREEBYTEGRSTARTS
#define FILEPOSSIZE	3
long GRFILEPOS(int c)
{
	long value;
	int	offset;

	offset = c*3;

	/*
	 * Read 3 bytes from the packed grstarts array.
	 * On the original DOS version this did an unaligned 32-bit read
	 * and masked to 24 bits.  We reconstruct byte-by-byte for
	 * portability and to avoid unaligned access on ARM.
	 */
	byte *p = ((byte *)grstarts) + offset;
	value = (long)p[0] | ((long)p[1] << 8) | ((long)p[2] << 16);

	if (value == 0xffffffl)
		value = -1;

	return value;
}
#else
#define FILEPOSSIZE	4
#define	GRFILEPOS(c) (grstarts[c])
#endif

/*
=============================================================================

					   LOW LEVEL ROUTINES

=============================================================================
*/

/*
============================
=
= CA_OpenDebug / CA_CloseDebug
=
= Opens a binary file with the handle "debughandle"
=
============================
*/

void CA_OpenDebug (void)
{
	unlink ("DEBUG.TXT");
	debughandle = open("DEBUG.TXT", O_CREAT | O_WRONLY, 0666);
}

void CA_CloseDebug (void)
{
	close (debughandle);
}



/*
============================
=
= CAL_GetGrChunkLength
=
= Gets the length of an explicit length chunk (not tiles)
= The file pointer is positioned so the compressed data can be read in next.
=
============================
*/

void CAL_GetGrChunkLength (int chunk)
{
	lseek(grhandle,GRFILEPOS(chunk),SEEK_SET);
	read(grhandle,&chunkexplen,sizeof(chunkexplen));
	chunkcomplen = GRFILEPOS(chunk+1)-GRFILEPOS(chunk)-4;
}


/*
==========================
=
= CA_FarRead
=
= Read from a file to a pointer
=
==========================
*/

boolean CA_FarRead (int handle, byte *dest, long length)
{
	ssize_t nread;

	nread = read(handle, dest, (size_t)length);
	if (nread == -1)
		return false;
	if (nread != (ssize_t)length)
	{
		errno = EINVAL;
		return false;
	}
	return true;
}


/*
==========================
=
= CA_FarWrite
=
= Write from a pointer to a file
=
==========================
*/

boolean CA_FarWrite (int handle, byte *source, long length)
{
	ssize_t nwritten;

	nwritten = write(handle, source, (size_t)length);
	if (nwritten == -1)
		return false;
	if (nwritten != (ssize_t)length)
	{
		errno = ENOMEM;
		return false;
	}
	return true;
}


/*
==========================
=
= CA_ReadFile
=
= Reads a file into an allready allocated buffer
=
==========================
*/

boolean CA_ReadFile (char *filename, memptr *ptr)
{
	int handle;
	long size;

	if ((handle = open(filename,O_RDONLY | O_BINARY, S_IREAD)) == -1)
		return false;

	size = lseek(handle, 0, SEEK_END);
	lseek(handle, 0, SEEK_SET);

	if (!CA_FarRead (handle,(byte *)*ptr,size))
	{
		close (handle);
		return false;
	}
	close (handle);
	return true;
}


/*
==========================
=
= CA_WriteFile
=
= Writes a file from a memory buffer
=
==========================
*/

boolean CA_WriteFile (char *filename, void *ptr, long length)
{
	int handle;

	handle = open(filename,O_CREAT | O_BINARY | O_WRONLY,
				S_IREAD | S_IWRITE);

	if (handle == -1)
		return false;

	if (!CA_FarWrite (handle,(byte *)ptr,length))
	{
		close (handle);
		return false;
	}
	close (handle);
	return true;
}



/*
==========================
=
= CA_LoadFile
=
= Allocate space for and load a file
=
==========================
*/

boolean CA_LoadFile (char *filename, memptr *ptr)
{
	int handle;
	long size;

	if ((handle = open(filename,O_RDONLY | O_BINARY, S_IREAD)) == -1)
		return false;

	size = lseek(handle, 0, SEEK_END);
	lseek(handle, 0, SEEK_SET);

	MM_GetPtr (ptr,size);
	if (!CA_FarRead (handle,(byte *)*ptr,size))
	{
		close (handle);
		return false;
	}
	close (handle);
	return true;
}

/*
============================================================================

		COMPRESSION routines, see JHUFF.C for more

============================================================================
*/



/*
======================
=
= CAL_HuffExpand
=
= Length is the length of the EXPANDED data.
= Pure C Huffman decoder using node indices (no pointer-in-int tricks).
= Does not support screenhack (VGA planar mode) - always flat buffer.
=
======================
*/

void CAL_HuffExpand (byte *source, byte *dest,
  long length, huffnode *hufftable, boolean screenhack)
{
	huffnode *headptr;
	huffnode *nodeon;
	byte *end;
	byte val;
	byte mask;

	(void)screenhack;  // not supported in SDL2 port

	if (length == 0)
		return;

	headptr = hufftable + 254;	// head node is always node 254
	nodeon = headptr;
	end = dest + length;

	val = *source++;
	mask = 1;

	while (dest < end)
	{
		unsigned short code;

		if (val & mask)
			code = nodeon->bit1;
		else
			code = nodeon->bit0;

		// Advance to next bit
		mask <<= 1;
		if (mask == 0)
		{
			val = *source++;
			mask = 1;
		}

		if (code < 256)
		{
			// It's a leaf byte value
			*dest++ = (byte)code;
			nodeon = headptr;	// back to the head node
		}
		else
		{
			// It's a node index (256 + node_index)
			nodeon = hufftable + (code - 256);
		}
	}
}



/*
======================
=
= CAL_CarmackExpand
=
= Length is the length of the EXPANDED data
=
======================
*/

#define NEARTAG	0xa7
#define FARTAG	0xa8

void CAL_CarmackExpand (unsigned *source, unsigned *dest, unsigned length)
{
	unsigned	ch,chhigh,count,offset;
	unsigned	*copyptr, *inptr, *outptr;
	byte		*bptr;

	length/=2;

	inptr = source;
	outptr = dest;

	while (length)
	{
		ch = *inptr++;
		chhigh = ch>>8;
		if (chhigh == NEARTAG)
		{
			count = ch&0xff;
			if (!count)
			{				// have to insert a word containing the tag byte
				bptr = (byte *)inptr;
				ch |= *bptr++;
				inptr = (unsigned *)bptr;
				*outptr++ = ch;
				length--;
			}
			else
			{
				bptr = (byte *)inptr;
				offset = *bptr++;
				inptr = (unsigned *)bptr;
				copyptr = outptr - offset;
				length -= count;
				while (count--)
					*outptr++ = *copyptr++;
			}
		}
		else if (chhigh == FARTAG)
		{
			count = ch&0xff;
			if (!count)
			{				// have to insert a word containing the tag byte
				bptr = (byte *)inptr;
				ch |= *bptr++;
				inptr = (unsigned *)bptr;
				*outptr++ = ch;
				length --;
			}
			else
			{
				offset = *inptr++;
				copyptr = dest + offset;
				length -= count;
				while (count--)
					*outptr++ = *copyptr++;
			}
		}
		else
		{
			*outptr++ = ch;
			length --;
		}
	}
}



/*
======================
=
= CA_RLEWcompress
=
======================
*/

long CA_RLEWCompress (unsigned *source, long length, unsigned *dest,
  unsigned rlewtag)
{
  long complength;
  unsigned value,count,i;
  unsigned *start, *end;

  start = dest;

  end = source + (length+1)/2;

//
// compress it
//
  do
  {
	count = 1;
	value = *source++;
	while (*source == value && source<end)
	{
	  count++;
	  source++;
	}
	if (count>3 || value == rlewtag)
	{
    //
    // send a tag / count / value string
    //
      *dest++ = rlewtag;
      *dest++ = count;
      *dest++ = value;
    }
    else
    {
    //
    // send word without compressing
    //
      for (i=1;i<=count;i++)
	*dest++ = value;
	}

  } while (source<end);

  complength = 2*(dest-start);
  return complength;
}


/*
======================
=
= CA_RLEWexpand
= length is EXPANDED length
=
======================
*/

void CA_RLEWexpand (unsigned *source, unsigned *dest, long length,
  unsigned rlewtag)
{
  unsigned value,count,i;
  unsigned *end;

  end = dest + (length)/2;

//
// expand it
//
  do
  {
	value = *source++;
	if (value != rlewtag)
	//
	// uncompressed
	//
	  *dest++=value;
	else
	{
	//
	// compressed string
	//
	  count = *source++;
	  value = *source++;
	  for (i=1;i<=count;i++)
	*dest++ = value;
	}
  } while (dest<end);
}



/*
=============================================================================

					 CACHE MANAGER ROUTINES

=============================================================================
*/


/*
======================
=
= CAL_SetupGrFile
=
======================
*/

void CAL_SetupGrFile (void)
{
	char fname[13];
	int handle;
	memptr compseg;
	memptr temp;

//
// load ???dict.ext (huffman dictionary for graphics files)
//

	strcpy(fname,gdictname);
	strcat(fname,extension);

	if ((handle = CAL_OpenCaseInsensitive(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);

	read(handle, &grhuffman, sizeof(grhuffman));
	close(handle);

//
// load the data offsets from ???head.ext
//
	temp = NULL;
	MM_GetPtr (&temp,(NUMCHUNKS+1)*FILEPOSSIZE);
	grstarts = (int32_t *)temp;

	strcpy(fname,gheadname);
	strcat(fname,extension);

	if ((handle = CAL_OpenCaseInsensitive(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);

	CA_FarRead(handle, (byte *)grstarts, (NUMCHUNKS+1)*FILEPOSSIZE);

	close(handle);

//
// Open the graphics file, leaving it open until the game is finished
//
	strcpy(fname,gfilename);
	strcat(fname,extension);

	grhandle = CAL_OpenCaseInsensitive(fname, O_RDONLY | O_BINARY, S_IREAD);
	if (grhandle == -1)
		CA_CannotOpen(fname);


//
// load the pic and sprite headers into the arrays in the data segment
//
	temp = NULL;
	MM_GetPtr(&temp,NUMPICS*sizeof(pictabletype));
	pictable = (pictabletype *)temp;

	CAL_GetGrChunkLength(STRUCTPIC);		// position file pointer
	MM_GetPtr(&compseg,chunkcomplen);
	CA_FarRead (grhandle,(byte *)compseg,chunkcomplen);
	CAL_HuffExpand ((byte *)compseg, (byte *)pictable,NUMPICS*sizeof(pictabletype),grhuffman,false);
	MM_FreePtr(&compseg);
}

//==========================================================================


/*
======================
=
= CAL_SetupMapFile
=
======================
*/

void CAL_SetupMapFile (void)
{
	int	i;
	int handle;
	long length,pos;
	char fname[13];
	memptr temp;

//
// load maphead.ext (offsets and tileinfo for map file)
//
	strcpy(fname,mheadname);
	strcat(fname,extension);

	if ((handle = CAL_OpenCaseInsensitive(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);

	length = lseek(handle, 0, SEEK_END);
	lseek(handle, 0, SEEK_SET);

	temp = NULL;
	MM_GetPtr (&temp,length);
	tinf = (byte *)temp;
	CA_FarRead(handle, tinf, length);
	close(handle);

//
// open the data file
//
#ifdef CARMACIZED
	strcpy(fname,"GAMEMAPS.");
	strcat(fname,extension);

	if ((maphandle = CAL_OpenCaseInsensitive(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);
#else
	strcpy(fname,mfilename);
	strcat(fname,extension);

	if ((maphandle = CAL_OpenCaseInsensitive(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);
#endif

//
// load all map header
//
	for (i=0;i<NUMMAPS;i++)
	{
		pos = ((mapfiletype *)tinf)->headeroffsets[i];
		if (pos<0)						// $FFFFFFFF start is a sparse map
			continue;

		temp = NULL;
		MM_GetPtr(&temp,sizeof(maptype));
		mapheaderseg[i] = (maptype *)temp;
		MM_SetLock((memptr *)&mapheaderseg[i],true);
		lseek(maphandle,pos,SEEK_SET);
		CA_FarRead (maphandle,(byte *)mapheaderseg[i],sizeof(maptype));
	}

//
// allocate space for 3 64*64 planes
//
	for (i=0;i<MAPPLANES;i++)
	{
		temp = NULL;
		MM_GetPtr (&temp,64*64*2);
		mapsegs[i] = (unsigned *)temp;
		MM_SetLock ((memptr *)&mapsegs[i],true);
	}
}


//==========================================================================


/*
======================
=
= CAL_SetupAudioFile
=
======================
*/

void CAL_SetupAudioFile (void)
{
	int handle;
	long length;
	char fname[13];
	memptr temp;

//
// load audiohed.ext (offsets for audio file)
//
	strcpy(fname,aheadname);
	strcat(fname,extension);

	if ((handle = CAL_OpenCaseInsensitive(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);

	length = lseek(handle, 0, SEEK_END);
	lseek(handle, 0, SEEK_SET);

	temp = NULL;
	MM_GetPtr (&temp,length);
	audiostarts = (int32_t *)temp;
	CA_FarRead(handle, (byte *)audiostarts, length);
	close(handle);

//
// open the data file
//
	strcpy(fname,afilename);
	strcat(fname,extension);

	if ((audiohandle = CAL_OpenCaseInsensitive(fname,
		 O_RDONLY | O_BINARY, S_IREAD)) == -1)
		CA_CannotOpen(fname);
}

//==========================================================================


/*
======================
=
= CA_Startup
=
= Open all files and load in headers
=
======================
*/

void CA_Startup (void)
{
#ifdef PROFILE
	unlink ("PROFILE.TXT");
	profilehandle = open("PROFILE.TXT", O_CREAT | O_WRONLY, 0666);
#endif

	CAL_SetupMapFile ();
	CAL_SetupGrFile ();
	CAL_SetupAudioFile ();

	mapon = -1;
	ca_levelbit = 1;
	ca_levelnum = 0;

}

//==========================================================================


/*
======================
=
= CA_Shutdown
=
= Closes all files
=
======================
*/

void CA_Shutdown (void)
{
#ifdef PROFILE
	close (profilehandle);
#endif

	close (maphandle);
	close (grhandle);
	close (audiohandle);
}

//===========================================================================

/*
======================
=
= CA_CacheAudioChunk
=
======================
*/

void CA_CacheAudioChunk (int chunk)
{
	long	pos,compressed;
	memptr  temp;

	if (audiosegs[chunk])
	{
		MM_SetPurge ((memptr *)&audiosegs[chunk],0);
		return;							// allready in memory
	}

//
// load the chunk into a buffer, either the miscbuffer if it fits, or allocate
// a larger buffer
//
	pos = audiostarts[chunk];
	compressed = audiostarts[chunk+1]-pos;

	lseek(audiohandle,pos,SEEK_SET);

	temp = NULL;
	MM_GetPtr (&temp,compressed);
	audiosegs[chunk] = (byte *)temp;
	if (mmerror)
		return;

	CA_FarRead(audiohandle,audiosegs[chunk],compressed);
}

//===========================================================================

/*
======================
=
= CA_LoadAllSounds
=
= Purges all sounds, then loads all new ones (mode switch)
=
======================
*/

void CA_LoadAllSounds (void)
{
	unsigned	start,i;

	switch (oldsoundmode)
	{
	case sdm_Off:
		goto cachein;
	case sdm_PC:
		start = STARTPCSOUNDS;
		break;
	case sdm_AdLib:
		start = STARTADLIBSOUNDS;
		break;
	}

	for (i=0;i<NUMSOUNDS;i++,start++)
		if (audiosegs[start])
			MM_SetPurge ((memptr *)&audiosegs[start],3);		// make purgable

cachein:

	switch (SoundMode)
	{
	case sdm_Off:
		return;
	case sdm_PC:
		start = STARTPCSOUNDS;
		break;
	case sdm_AdLib:
		start = STARTADLIBSOUNDS;
		break;
	}

	for (i=0;i<NUMSOUNDS;i++,start++)
		CA_CacheAudioChunk (start);

	oldsoundmode = SoundMode;
}

//===========================================================================


/*
======================
=
= CAL_ExpandGrChunk
=
= Does whatever is needed with a pointer to a compressed chunk
=
======================
*/

void CAL_ExpandGrChunk (int chunk, byte *source)
{
	long	expanded;


	if (chunk >= STARTTILE8 && chunk < STARTEXTERNS)
	{
	//
	// expanded sizes of tile8/16/32 are implicit
	//

#define BLOCK		64
#define MASKBLOCK	128

		if (chunk<STARTTILE8M)			// tile 8s are all in one chunk!
			expanded = BLOCK*NUMTILE8;
		else if (chunk<STARTTILE16)
			expanded = MASKBLOCK*NUMTILE8M;
		else if (chunk<STARTTILE16M)	// all other tiles are one/chunk
			expanded = BLOCK*4;
		else if (chunk<STARTTILE32)
			expanded = MASKBLOCK*4;
		else if (chunk<STARTTILE32M)
			expanded = BLOCK*16;
		else
			expanded = MASKBLOCK*16;
	}
	else
	{
	//
	// everything else has an explicit size longword
	//
		expanded = *(int32_t *)source;  // data files store 32-bit sizes
		source += 4;			// skip over length
	}

//
// allocate final space, decompress it, and free bigbuffer
// Sprites need to have shifts made and various other junk
//
	MM_GetPtr (&grsegs[chunk],expanded);
	if (mmerror)
		return;
	CAL_HuffExpand (source,(byte *)grsegs[chunk],expanded,grhuffman,false);
}


/*
======================
=
= CA_CacheGrChunk
=
= Makes sure a given chunk is in memory, loading it if needed
=
======================
*/

void CA_CacheGrChunk (int chunk)
{
	long	pos,compressed;
	memptr	bigbufferseg;
	byte	*source;
	int		next;

	grneeded[chunk] |= ca_levelbit;		// make sure it doesn't get removed
	if (grsegs[chunk])
	{
		MM_SetPurge (&grsegs[chunk],0);
		return;							// allready in memory
	}

//
// load the chunk into a buffer, either the miscbuffer if it fits, or allocate
// a larger buffer
//
	pos = GRFILEPOS(chunk);
	if (pos<0)							// $FFFFFFFF start is a sparse tile
	  return;

	next = chunk +1;
	while (GRFILEPOS(next) == -1)		// skip past any sparse tiles
		next++;

	compressed = GRFILEPOS(next)-pos;

	lseek(grhandle,pos,SEEK_SET);

	if (compressed<=BUFFERSIZE)
	{
		CA_FarRead(grhandle,(byte *)bufferseg,compressed);
		source = (byte *)bufferseg;
	}
	else
	{
		MM_GetPtr(&bigbufferseg,compressed);
		MM_SetLock (&bigbufferseg,true);
		CA_FarRead(grhandle,(byte *)bigbufferseg,compressed);
		source = (byte *)bigbufferseg;
	}

	CAL_ExpandGrChunk (chunk,source);

	if (compressed>BUFFERSIZE)
		MM_FreePtr(&bigbufferseg);
}



//==========================================================================

/*
======================
=
= CA_CacheScreen
=
= Decompresses a chunk from disk into a temp buffer, then copies
= to the SDL framebuffer.
=
======================
*/

extern byte *sdl_framebuffer;

void CA_CacheScreen (int chunk)
{
	long	pos,compressed,expanded;
	memptr	bigbufferseg;
	memptr	tempbuf;
	byte	*source;
	int		next;

//
// load the chunk into a buffer
//
	pos = GRFILEPOS(chunk);
	next = chunk +1;
	while (GRFILEPOS(next) == -1)		// skip past any sparse tiles
		next++;
	compressed = GRFILEPOS(next)-pos;

	lseek(grhandle,pos,SEEK_SET);

	MM_GetPtr(&bigbufferseg,compressed);
	MM_SetLock (&bigbufferseg,true);
	CA_FarRead(grhandle,(byte *)bigbufferseg,compressed);
	source = (byte *)bigbufferseg;

	expanded = *(int32_t *)source;  // data files store 32-bit sizes
	source += 4;			// skip over length

//
// Decompress into a temp buffer, then copy to framebuffer.
// The original code wrote directly to VGA planar memory via screenhack;
// we decompress flat and memcpy to sdl_framebuffer instead.
//
	MM_GetPtr(&tempbuf, expanded);
	CAL_HuffExpand (source, (byte *)tempbuf, expanded, grhuffman, false);

	if (sdl_framebuffer)
		memcpy(sdl_framebuffer, tempbuf, (size_t)expanded);

	MM_FreePtr(&tempbuf);

	VW_MarkUpdateBlock (0,0,319,199);
	MM_FreePtr(&bigbufferseg);
}

//==========================================================================

/*
======================
=
= CA_CacheMap
=
= WOLF: This is specialized for a 64*64 map size
=
======================
*/

void CA_CacheMap (int mapnum)
{
	long	pos,compressed;
	int		plane;
	memptr	*dest,bigbufferseg;
	unsigned	size;
	unsigned	*source;
#ifdef CARMACIZED
	memptr	buffer2seg;
	long	expanded;
#endif

	mapon = mapnum;

//
// load the planes into the allready allocated buffers
//
	size = 64*64*2;

	for (plane = 0; plane<MAPPLANES; plane++)
	{
		pos = mapheaderseg[mapnum]->planestart[plane];
		compressed = mapheaderseg[mapnum]->planelength[plane];

		dest = (memptr *)&mapsegs[plane];

		lseek(maphandle,pos,SEEK_SET);
		if (compressed<=BUFFERSIZE)
			source = (unsigned *)bufferseg;
		else
		{
			MM_GetPtr(&bigbufferseg,compressed);
			MM_SetLock (&bigbufferseg,true);
			source = (unsigned *)bigbufferseg;
		}

		CA_FarRead(maphandle,(byte *)source,compressed);
#ifdef CARMACIZED
		//
		// unhuffman, then unRLEW
		// The huffman'd chunk has a two byte expanded length first
		// The resulting RLEW chunk also does, even though it's not really
		// needed
		//
		expanded = *source;
		source++;
		MM_GetPtr (&buffer2seg,expanded);
		CAL_CarmackExpand (source, (unsigned *)buffer2seg,expanded);
		CA_RLEWexpand (((unsigned *)buffer2seg)+1,*dest,size,
		((mapfiletype *)tinf)->RLEWtag);
		MM_FreePtr (&buffer2seg);

#else
		//
		// unRLEW, skipping expanded length
		//
		CA_RLEWexpand (source+1, *dest,size,
		((mapfiletype *)tinf)->RLEWtag);
#endif

		if (compressed>BUFFERSIZE)
			MM_FreePtr(&bigbufferseg);
	}
}

//===========================================================================

/*
======================
=
= CA_UpLevel
=
= Goes up a bit level in the needed lists and clears it out.
= Everything is made purgable
=
======================
*/

void CA_UpLevel (void)
{
	int	i;

	if (ca_levelnum==7)
		Quit ("CA_UpLevel: Up past level 7!");

	for (i=0;i<NUMCHUNKS;i++)
		if (grsegs[i])
			MM_SetPurge (&grsegs[i],3);
	ca_levelbit<<=1;
	ca_levelnum++;
}

//===========================================================================

/*
======================
=
= CA_DownLevel
=
= Goes down a bit level in the needed lists and recaches
= everything from the lower level
=
======================
*/

void CA_DownLevel (void)
{
	if (!ca_levelnum)
		Quit ("CA_DownLevel: Down past level 0!");
	ca_levelbit>>=1;
	ca_levelnum--;
	CA_CacheMarks();
}

//===========================================================================

/*
======================
=
= CA_ClearMarks
=
= Clears out all the marks at the current level
=
======================
*/

void CA_ClearMarks (void)
{
	int i;

	for (i=0;i<NUMCHUNKS;i++)
		grneeded[i]&=~ca_levelbit;
}


//===========================================================================

/*
======================
=
= CA_ClearAllMarks
=
= Clears out all the marks on all the levels
=
======================
*/

void CA_ClearAllMarks (void)
{
	memset (grneeded,0,sizeof(grneeded));
	ca_levelbit = 1;
	ca_levelnum = 0;
}


//===========================================================================


/*
======================
=
= CA_FreeGraphics
=
======================
*/


void CA_SetGrPurge (void)
{
	int i;

//
// free graphics
//
	CA_ClearMarks ();

	for (i=0;i<NUMCHUNKS;i++)
		if (grsegs[i])
			MM_SetPurge (&grsegs[i],3);
}



/*
======================
=
= CA_SetAllPurge
=
= Make everything possible purgable
=
======================
*/

void CA_SetAllPurge (void)
{
	int i;


//
// free sounds
//
	for (i=0;i<NUMSNDCHUNKS;i++)
		if (audiosegs[i])
			MM_SetPurge ((memptr *)&audiosegs[i],3);

//
// free graphics
//
	CA_SetGrPurge ();
}


//===========================================================================

/*
======================
=
= CA_CacheMarks
=
======================
*/
#define MAXEMPTYREAD	1024

void CA_CacheMarks (void)
{
	int 	i,next,numcache;
	long	pos,endpos,nextpos,nextendpos,compressed;
	long	bufferstart,bufferend;	// file position of general buffer
	byte	*source;
	memptr	bigbufferseg;

	numcache = 0;
//
// go through and make everything not needed purgable
//
	for (i=0;i<NUMCHUNKS;i++)
		if (grneeded[i]&ca_levelbit)
		{
			if (grsegs[i])					// its allready in memory, make
				MM_SetPurge(&grsegs[i],0);	// sure it stays there!
			else
				numcache++;
		}
		else
		{
			if (grsegs[i])					// not needed, so make it purgeable
				MM_SetPurge(&grsegs[i],3);
		}

	if (!numcache)			// nothing to cache!
		return;


//
// go through and load in anything still needed
//
	bufferstart = bufferend = 0;		// nothing good in buffer now

	for (i=0;i<NUMCHUNKS;i++)
		if ( (grneeded[i]&ca_levelbit) && !grsegs[i])
		{
			pos = GRFILEPOS(i);
			if (pos<0)
				continue;

			next = i +1;
			while (GRFILEPOS(next) == -1)		// skip past any sparse tiles
				next++;

			compressed = GRFILEPOS(next)-pos;
			endpos = pos+compressed;

			if (compressed<=BUFFERSIZE)
			{
				if (bufferstart<=pos
				&& bufferend>= endpos)
				{
				// data is allready in buffer
					source = (byte *)bufferseg+(pos-bufferstart);
				}
				else
				{
				// load buffer with a new block from disk
				// try to get as many of the needed blocks in as possible
					while ( next < NUMCHUNKS )
					{
						while (next < NUMCHUNKS &&
						!(grneeded[next]&ca_levelbit && !grsegs[next]))
							next++;
						if (next == NUMCHUNKS)
							continue;

						nextpos = GRFILEPOS(next);
						while (GRFILEPOS(++next) == -1)	// skip past any sparse tiles
							;
						nextendpos = GRFILEPOS(next);
						if (nextpos - endpos <= MAXEMPTYREAD
						&& nextendpos-pos <= BUFFERSIZE)
							endpos = nextendpos;
						else
							next = NUMCHUNKS;			// read pos to posend
					}

					lseek(grhandle,pos,SEEK_SET);
					CA_FarRead(grhandle,(byte *)bufferseg,endpos-pos);
					bufferstart = pos;
					bufferend = endpos;
					source = (byte *)bufferseg;
				}
			}
			else
			{
			// big chunk, allocate temporary buffer
				MM_GetPtr(&bigbufferseg,compressed);
				if (mmerror)
					return;
				MM_SetLock (&bigbufferseg,true);
				lseek(grhandle,pos,SEEK_SET);
				CA_FarRead(grhandle,(byte *)bigbufferseg,compressed);
				source = (byte *)bigbufferseg;
			}

			CAL_ExpandGrChunk (i,source);
			if (mmerror)
				return;

			if (compressed>BUFFERSIZE)
				MM_FreePtr(&bigbufferseg);

		}
}

void CA_CannotOpen(char *string)
{
 char str[30];

 strcpy(str,"Can't open ");
 strcat(str,string);
 strcat(str,"!\n");
 Quit (str);
}

