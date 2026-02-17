CC = cc
CFLAGS = -arch arm64 -std=c99 -Wall -Wno-unused-variable -Wno-unused-function \
         -Wno-missing-braces -Wno-parentheses -Wno-switch \
         -DGOODTIMES -DCARMACIZED -DARTSEXTERN -DDEMOSEXTERN \
         $(shell sdl2-config --cflags)
LDFLAGS = $(shell sdl2-config --libs) -lm
SRCDIR = src
OBJDIR = obj

SRCS = $(SRCDIR)/id_ca.c \
       $(SRCDIR)/id_in.c \
       $(SRCDIR)/id_mm.c \
       $(SRCDIR)/id_pm.c \
       $(SRCDIR)/id_sd.c \
       $(SRCDIR)/id_us_1.c \
       $(SRCDIR)/id_vh.c \
       $(SRCDIR)/id_vl.c \
       $(SRCDIR)/wl_act1.c \
       $(SRCDIR)/wl_act2.c \
       $(SRCDIR)/wl_agent.c \
       $(SRCDIR)/wl_debug.c \
       $(SRCDIR)/wl_draw.c \
       $(SRCDIR)/wl_game.c \
       $(SRCDIR)/wl_inter.c \
       $(SRCDIR)/wl_main.c \
       $(SRCDIR)/wl_menu.c \
       $(SRCDIR)/wl_play.c \
       $(SRCDIR)/wl_scale.c \
       $(SRCDIR)/wl_state.c \
       $(SRCDIR)/wl_text.c \
       $(SRCDIR)/signon.c

OBJS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))
TARGET = wolf3d

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean
