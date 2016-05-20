## Process this file with automake to produce Makefile.in

bin_PROGRAMS = iec104

iec104_SOURCES = iec104.c iec104.h log.c log.h debug.h \
	tcp_server.c tcp_server.h client.h client.c \
	apdu.h apdu.c asdu.h asdu.c timer.c \
	cur_values.c cur_values.h \
	settings.h settings.c

AM_CFLAGS = -std=gnu99 -D_FILE_OFFSET_BITS=64 -Wall
#for debug: ./configure CFLAGS='-ggdb -O0'

AM_CPPFLAGS = -I$(top_srcdir)

AM_LDFLAGS = -lrt -luv -lmedb -ljson-c
#AM_LDFLAGS = -lrt -lpthread -lm -lcrypt
## EOF
