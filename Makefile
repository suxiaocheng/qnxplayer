
CC=gcc
LDFLAGS=-lavformat -lavcodec -lavutil -lswscale -lswresample
CFLAGS= -g

CFLAGS += -I/usr/local/include/SDL2 -D_REENTRANT -I./Include
LDFLAGS += -L/usr/local/lib -Wl,-rpath,/usr/local/lib -Wl,--enable-new-dtags -lSDL2 -lEGL -lGLESv2 -lm
LDFLAGS += -L/usr/lib/x86_64-linux-gnu -lX11

SRC := $(shell find . -maxdepth 2 -type f -regex ".*\.c")
SRC += ./Source/LinuxX11/esUtil_X11.c
OBJ := $(patsubst %.c,%.o,${SRC})

qnxplayer:${OBJ}
	echo ${OBJ}
	${CC} ${CFLAGS} $^ -o $@  ${LDFLAGS}

depend:
	${CC} -MM ${SRC} ${CFLAGS} > depend

.PHONY: clean
clean:
	rm *.o ${OBJ} depend -rf
