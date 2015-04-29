LIBVNCSERVERCONFIG=/Users/gardners/Projects/c64accel/git/libvncserver/libvncserver-config

HDRS=	Makefile \
	mega65.h 
OBJS=	machinestate.o \
	main.o \
	gs4510.o \
	viciv.o

CC=	gcc
CFLAGS= -Wall -O3 -g
DEFS=
LDFLAGS=`$(LIBVNCSERVERCONFIG) --libs`

all:	mega65-emu

mega65-emu:	$(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o mega65-emu $(OBJS)

%.o:	%.c $(HDRS)
	$(CC) $(CFLAGS) $(DEFS) -c $< -o $@
