
CC = gcc
INCLUDE = -I. -I../../kernel
LIB =
CFLAG = -c -Wall -O2 -fPIC
LFLAG =

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
  CFLAG += -DUNIX
  LFLAGLIB = -s -fPIC -shared
endif
ifeq ($(UNAME), Darwin)
  CFLAG += -DUNIX -DMAC_OSX
  LFLAGLIB = -s -fPIC -dynamiclib
endif

all: DaoCGI.so

daoCGI.o: daoCGI.c
	$(CC) $(CFLAG) $(INCLUDE) daoCGI.c
	
DaoCGI.so: daoCGI.o
	$(CC) $(LFLAGLIB) $(LIB) daoCGI.o -o DaoCGI.so

bind: clean

clean:
	rm *.o
