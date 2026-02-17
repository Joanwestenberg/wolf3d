// wl_compat.h - Platform compatibility for macOS/SDL2 port
#ifndef __WL_COMPAT_H__
#define __WL_COMPAT_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <math.h>
#include <time.h>

#include <SDL.h>

// DOS type mappings
typedef uint8_t   byte;
typedef uint16_t  word;
typedef uint32_t  longword;
typedef int32_t   fixed;
typedef void     *memptr;
typedef uint8_t  *Ptr;

typedef enum { false_val, true_val } boolean_t;
// Use int for boolean to match original sizeof
typedef int boolean;
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

typedef struct {
    int x, y;
} Point;

typedef struct {
    Point ul, lr;
} Rect;

// Eliminate Borland-specific keywords
#define far
#define near
#define huge
#define interrupt
#define _seg

// DOS function replacements
#define _fmemcpy  memcpy
#define _fmemset  memset
#define _fmemcmp  memcmp
#define _fstricmp strcasecmp
#define stricmp   strcasecmp
#define strnicmp  strncasecmp

// File I/O compatibility
#ifndef O_BINARY
#define O_BINARY 0
#endif

// NULL pointer
#define nil ((void *)0)

// VGA hardware macros - no-ops on modern systems
#define VGAWRITEMODE(x)
#define VGAMAPMASK(x)
#define VGAREADMAP(x)
#define COLORBORDER(x)

// Inline asm stubs for Borland register pseudovariables
#define _AX
#define _BX
#define _CX
#define _DX
#define _AH
#define _AL
#define _BH
#define _BL
#define geninterrupt(x)

// Borland memory functions
#define movedata(srcseg, srcoff, dstseg, dstoff, len) \
    memcpy((void*)(uintptr_t)(dstoff), (void*)(uintptr_t)(srcoff), (len))

#define FP_SEG(x) 0
#define FP_OFF(x) ((uintptr_t)(x))
#define MK_FP(s,o) ((void*)(uintptr_t)(o))

// Borland misc
#define farmalloc(x)  malloc(x)
#define farfree(x)    free(x)
#define farcoreleft()  (1024L*1024L*64L)
#define coreleft()     (1024L*64L)

// Screen constants - kept for compatibility but not tied to hardware
#define SCREENSEG       0xa000
#define SCREENWIDTH     80
#define MAXSCANLINES    200
#define CHARWIDTH       2
#define TILEWIDTH       4

// Timing
#define TICRATE 70

// Sound port constants (kept as defines but not used for I/O)
#define pcTimer     0x42
#define pcTAccess   0x43
#define pcSpeaker   0x61
#define pcSpkBits   3

// Utility macros
#define UNCACHEGRCHUNK(chunk) { if (grsegs[chunk]) { free(grsegs[chunk]); grsegs[chunk] = NULL; } grneeded[chunk] &= ~ca_levelbit; }

// DOS path separator replacement
#define SPEAR_DIR "sod"
#define WOLF_DIR  "wl6"

// itoa replacement (not available on macOS)
static inline char *itoa(int value, char *str, int base) {
    if (base == 10) sprintf(str, "%d", value);
    else if (base == 16) sprintf(str, "%x", value);
    else sprintf(str, "%d", value);
    return str;
}

// ltoa replacement
static inline char *ltoa(long value, char *str, int base) {
    if (base == 10) sprintf(str, "%ld", value);
    else if (base == 16) sprintf(str, "%lx", value);
    else sprintf(str, "%ld", value);
    return str;
}

// outportb stub
#define outportb(port, val) ((void)0)

// peekb stub - read from memory address (not meaningful on modern systems)
#define peekb(seg, ofs) ((byte)0)

// _argc/_argv (defined in wl_main.c)
extern int _argc;
extern char **_argv;

// bioskey stub
#define bioskey(cmd) 0

// S_IFREG if not defined
#ifndef S_IFREG
#define S_IFREG 0100000
#endif

// O_TEXT is DOS-specific
#ifndef O_TEXT
#define O_TEXT 0
#endif

// Helper for storing integer values in pointer arrays (actorat pattern)
// Wolf3D stores small integers and pointers in the same array
#include <stdint.h>
#define ACTORAT_INT(val) ((objtype *)(uintptr_t)(unsigned)(val))
#define ACTORAT_IS_INT(ptr) ((uintptr_t)(ptr) < 256)
#define ACTORAT_TO_INT(ptr) ((unsigned)(uintptr_t)(ptr))

// _fstrcpy/_fstrlen (Borland far string functions)
#define _fstrcpy strcpy
#define _fstrlen strlen

#endif // __WL_COMPAT_H__
