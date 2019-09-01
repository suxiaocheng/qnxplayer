
CC=gcc
LDFLAGS=-lavformat -lavcodec -lavutil -lswscale -lswresample
CFLAGS= -g

# CFLAGS += -I/usr/include/SDL2 -D_REENTRANT
CFLAGS += -I/home/pi/program/c/SDL2-2.0.10/install/include/SDL2 -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -D_REENTRANT
# LDFLAGS += -L/usr/lib/arm-linux-gnueabihf -lSDL2
LDFLAGS += -L/home/pi/program/c/SDL2-2.0.10/install/lib -Wl,-rpath,/home/pi/program/c/SDL2-2.0.10/install/lib -Wl,--enable-new-dtags -lSDL2

qnxplayer:qnxplayer.c
	${CC} ${CFLAGS} $< -o $@  ${LDFLAGS}
