
CC=gcc
LDFLAGS=-lavformat -lavcodec -lavutil -lswscale -lswresample
CFLAGS= -g

# CFLAGS += -I/usr/include/SDL2 -D_REENTRANT
CFLAGS += -I/usr/local/include/SDL2 -D_REENTRANT
# LDFLAGS += -L/usr/lib/arm-linux-gnueabihf -lSDL2
LDFLAGS += -L/usr/local/lib -Wl,-rpath,/usr/local/lib -Wl,--enable-new-dtags -lSDL2

qnxplayer:qnxplayer.c
	${CC} ${CFLAGS} $< -o $@  ${LDFLAGS}
