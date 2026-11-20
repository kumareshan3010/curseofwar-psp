TARGET = curseofwar
OBJS = grid.o state.o king.o output-common.o main-common.o output-sdl.o main-psp.o

PSP_PREFIX = $(shell psp-config --psp-prefix)
PSPSDK = $(shell psp-config --pspsdk-path)

INCDIR = $(PSP_PREFIX)/include/SDL2
CFLAGS = -O2 -G0 -Wall -std=gnu99 -DNOGETOPT=1 -DVERSION=\"1.0.0-psp\"
CFLAGS += -I$(PSP_PREFIX)/include/SDL2
CXXFLAGS = $(CFLAGS)
LIBS = -lSDL2main -lSDL2 -lm -lpspctrl -lpspdisplay -lpspkernel -lpspdebug -lpspgu -lpspvram

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Curse of War

include $(PSPSDK)/lib/build.mak
