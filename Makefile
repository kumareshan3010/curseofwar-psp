TARGET = curseofwar
OBJS = main.o
CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS)
LIBS = -lSDL2 -lGL -lGLU -lglut -lz -lm -lpspvram -lpspvfpu -lpspdebug -lpspgu -lpspge -lpspdisplay -lpspctrl -lpsppower -lpspaudio -lpsphprm -lpspnet -lpspnet_apctl
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Curse of War
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
