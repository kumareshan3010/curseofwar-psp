TARGET = curseofwar
OBJS = grid.o state.o king.o output-common.o main-common.o output-sdl.o main-psp.o

PSP_PREFIX = $(shell psp-config --psp-prefix)
PSPSDK = $(shell psp-config --pspsdk-path)

INCDIR = $(PSP_PREFIX)/include/SDL2
CFLAGS = -O2 -G0 -Wall -std=gnu99 -DNOGETOPT=1 -DVERSION=\"1.0.0-psp\"
CFLAGS += -I$(PSP_PREFIX)/include/SDL2
CXXFLAGS = $(CFLAGS)
# SDL2's PSP backend uses GU, VFPU, GL, audio, HPRM, and VRAM APIs.
# Do not link libpspkernel directly: it duplicates newlib strtol/strtoul.
LIBS = -lSDL2 -lGL -lm -lpspaudio -lpsphprm -lpspvram -lpspgu -lpspvfpu

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Curse of War

include $(PSPSDK)/lib/build.mak
