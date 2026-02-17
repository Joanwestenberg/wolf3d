// signon.c - Placeholder signon screen data and game palette
// Originally linked from SIGNON.OBJ and GAMEPAL.OBJ in the DOS build.
// The signon screen is 320x200 = 64000 bytes of VGA indexed color data.
// The game palette is 256 RGB triplets (768 bytes) in 6-bit VGA format.

#include "wl_compat.h"

// Blank signon screen (will show black until real data loaded)
// introscn is #defined as signon in id_heads.h
byte signon[64000] = {0};

// Default VGA palette - placeholder (real palette loaded from game data)
byte gamepal[768] = {0};
