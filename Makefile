CC 	= gcc

CFLAGS  = -Wall -g -Iinclude

LD 	= gcc

LDFLAGS  = -Wall -g 

PROGS	= snakes nums hungry

SNAKEOBJS  = demos/randomsnakes.o demos/util.o 

HUNGRYOBJS = demos/hungrysnakes.o demos/util.o

NUMOBJS    = demos/numbersmain.o

OBJS	= $(SNAKEOBJS) $(HUNGRYOBJS) $(NUMOBJS) 

SRCS	= demos/randomsnakes.c demos/numbersmain.c demos/hungrysnakes.c

HDRS	= 

EXTRACLEAN = core $(PROGS)

all: 	$(PROGS)

allclean: clean
	@rm -f $(EXTRACLEAN)

clean:	
	rm -f $(OBJS) libLWP.a *~ TAGS

snakes: demos/randomsnakes.o demos/util.o libLWP.a libsnakes.a
	$(LD) $(LDFLAGS) -o snakes demos/randomsnakes.o demos/util.o -L. -lncurses -lsnakes -lLWP

hungry: demos/hungrysnakes.o demos/util.o libLWP.a libsnakes.a
	$(LD) $(LDFLAGS) -o hungry demos/hungrysnakes.o demos/util.o -L. -lncurses -lsnakes -lLWP

nums: demos/numbersmain.o libLWP.a 
	$(LD) $(LDFLAGS) -o nums demos/numbersmain.o -L. -lLWP

demos/hungrysnakes.o: include/lwp.h include/snakes.h

demos/randomsnakes.o: include/lwp.h include/snakes.h

numbermain.o: include/lwp.h

libLWP.a: src/lwp.c src/roundrobin.c
	$(CC) $(CFLAGS) -c src/roundrobin.c src/lwp.c src/magic64.S 
	ar r libLWP.a lwp.o roundrobin.o magic64.o
	rm -f lwp.o roundrobin.o magic64.o
