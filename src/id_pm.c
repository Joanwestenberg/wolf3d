// ID_PM.C - Page manager (macOS/SDL2 port)
//
// Loads VSWAP data from disk and caches pages in memory.

#include "id_heads.h"
#include <ctype.h>

static int PM_OpenCaseInsensitive(const char *filename, int flags)
{
	int handle, i;
	char buf[64];
	handle = open(filename, flags);
	if (handle != -1) return handle;
	for (i = 0; filename[i] && i < 63; i++)
		buf[i] = tolower((unsigned char)filename[i]);
	buf[i] = '\0';
	handle = open(buf, flags);
	if (handle != -1) return handle;
	for (i = 0; filename[i] && i < 63; i++)
		buf[i] = toupper((unsigned char)filename[i]);
	buf[i] = '\0';
	return open(buf, flags);
}

char       PageFileName[13] = {"VSWAP."};
static int PageFile = -1;

word       ChunksInFile;
word       PMSpriteStart, PMSoundStart;

static longword *PageOffsets;
static word     *PageLengths;
static memptr   *PageCache;
static long      PMFrameCount;

static boolean PMStarted;

void PM_Startup(void)
{
	long size;

	if (PMStarted)
		return;

	PageFile = PM_OpenCaseInsensitive(PageFileName, O_RDONLY | O_BINARY);
	if (PageFile == -1)
		Quit("PM_Startup: Unable to open page file");

	read(PageFile, &ChunksInFile, sizeof(ChunksInFile));
	read(PageFile, &PMSpriteStart, sizeof(PMSpriteStart));
	read(PageFile, &PMSoundStart, sizeof(PMSoundStart));

	size = sizeof(longword) * ChunksInFile;
	PageOffsets = (longword *)malloc(size);
	if (!PageOffsets)
		Quit("PM_Startup: Failed to allocate page offsets");
	read(PageFile, PageOffsets, size);

	size = sizeof(word) * ChunksInFile;
	PageLengths = (word *)malloc(size);
	if (!PageLengths)
		Quit("PM_Startup: Failed to allocate page lengths");
	read(PageFile, PageLengths, size);

	size = sizeof(memptr) * ChunksInFile;
	PageCache = (memptr *)malloc(size);
	if (!PageCache)
		Quit("PM_Startup: Failed to allocate page cache");
	memset(PageCache, 0, size);

	PMFrameCount = 0;
	PMStarted = true;
}

void PM_Shutdown(void)
{
	int i;

	if (!PMStarted)
		return;

	if (PageCache)
	{
		for (i = 0; i < ChunksInFile; i++)
		{
			if (PageCache[i])
			{
				free(PageCache[i]);
				PageCache[i] = NULL;
			}
		}
		free(PageCache);
		PageCache = NULL;
	}

	if (PageOffsets) { free(PageOffsets); PageOffsets = NULL; }
	if (PageLengths) { free(PageLengths); PageLengths = NULL; }

	if (PageFile != -1)
	{
		close(PageFile);
		PageFile = -1;
	}

	PMStarted = false;
}

void PM_Reset(void)
{
	int i;

	if (!PageCache)
		return;

	for (i = 0; i < ChunksInFile; i++)
	{
		if (PageCache[i])
		{
			free(PageCache[i]);
			PageCache[i] = NULL;
		}
	}

	PMFrameCount = 0;
}

memptr PM_GetPageAddress(int pagenum)
{
	if (pagenum < 0 || pagenum >= ChunksInFile)
		return NULL;
	return PageCache[pagenum];
}

memptr PM_GetPage(int pagenum)
{
	word  length;
	void *buf;

	if (pagenum >= ChunksInFile)
		Quit("PM_GetPage: Invalid page request");

	if (PageCache[pagenum])
		return PageCache[pagenum];

	if (!PageOffsets[pagenum])
		Quit("PM_GetPage: Tried to load a sparse page!");

	length = PageLengths[pagenum];
	if (length == 0)
		length = PMPageSize;

	buf = malloc(PMPageSize);
	if (!buf)
		Quit("PM_GetPage: Out of memory");

	memset(buf, 0, PMPageSize);

	if (lseek(PageFile, PageOffsets[pagenum], SEEK_SET) == -1)
		Quit("PM_GetPage: Seek failed");

	read(PageFile, buf, length);

	PageCache[pagenum] = buf;
	return buf;
}

void PM_Preload(boolean (*update)(word current, word total))
{
	int  i;
	word total, current;

	total = 0;
	for (i = 0; i < ChunksInFile; i++)
	{
		if (PageOffsets[i] && !PageCache[i])
			total++;
	}

	if (!total)
	{
		if (update)
			update(1, 1);
		return;
	}

	current = 0;
	for (i = 0; i < ChunksInFile; i++)
	{
		if (PageOffsets[i] && !PageCache[i])
		{
			PM_GetPage(i);
			current++;
			if (update)
				update(current, total);
		}
	}
}

void PM_NextFrame(void) { PMFrameCount++; }
void PM_SetPageLock(int pagenum, PMLockType lock) { (void)pagenum; (void)lock; }
void PM_SetMainPurge(int level) { (void)level; }
void PM_SetMainMemPurge(int level) { (void)level; }
void PM_CheckMainMem(void) { }
