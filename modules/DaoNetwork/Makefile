
CC = gcc
INCLUDE = -I.
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

all: DaoNetwork.so

daoNetwork.o: daoNetwork.c
	$(CC) $(CFLAG) $(INCLUDE) daoNetwork.c
	
DaoNetwork.so: daoNetwork.o
	$(CC) $(LFLAGLIB) $(LIB) daoNetwork.o -o DaoNetwork.so

bind: clean

clean:
	rm *.o
