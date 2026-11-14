TARGET = curseofwar
OBJS = grid.o state.o king.o output-common.o main-common.o output-sdl.o main-psp.o
PSP_PREFIX = $(shell psp-config --psp-prefix)
CFLAGS = -O2 -G0 -Wall -std=gnu99 -DNOGETOPT=1 -I$(PSP_PREFIX)/include/SDL2
CXXFLAGS = $(CFLAGS)
LIBS = -lSDL2main -lSDL2 -lGL -lGLU -lglut -lz -lm -lpspvram -lpspvfpu -lpspdebug -lpspgu -lpspge -lpspdisplay -lpspctrl -lpsppower -lpspaudio -lpsphprm -lpspnet -lpspnet_apctl
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Curse of War
PSP_EBOOT_ICON = ICON0.PNG
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
