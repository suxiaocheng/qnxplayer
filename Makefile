
CC=aarch64-unknown-nto-qnx7.0.0-gcc
LDFLAGS=-L/home/V01.NET/uidq2055/program/tmp/ffmpeg/install/lib -lavformat -lavcodec -lavutil -lswscale -lswresample -lscreen
CFLAGS=-I/home/V01.NET/uidq2055/program/tmp/ffmpeg/install/include

qnxplayer:qnxplayer.c
	${CC} ${CFLAGS} $< -o $@  ${LDFLAGS}
