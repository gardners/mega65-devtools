LIBVNCSERVERCONFIG=/Users/gardners/Projects/c64accel/git/libvncserver/libvncserver-config

HDRS=	Makefile \
	mega65.h \
	instructions.h
OBJS=	machinestate.o \
	main.o \
	gs4510.o \
	viciv.o \
	instruction_names.o \
	instruction_table.o

CC=	gcc
CFLAGS= -Wall -O3 -g
DEFS=
LDFLAGS=`$(LIBVNCSERVERCONFIG) --libs`

all:	mega65-emu

mega65-emu:	$(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o mega65-emu $(OBJS)

%.o:	%.c $(HDRS)
	$(CC) $(CFLAGS) $(DEFS) -c $< -o $@

instructions.h:	Makefile 64net.opc mkinstructions
	./mkinstructions

instruction_names.c:	Makefile 64net.opc mkinstructions
	./mkinstructions

instruction_table.c:	Makefile 64net.opc mkinstructions
	./mkinstructions
