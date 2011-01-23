
CC = gcc
INCLUDE = -I.
LIB =
CFLAG = -c -Wall -O2 -fPIC
LFLAG =

UNAME := $(shell uname)

TARGET = DaoNetwork.so

ifeq ($(UNAME), Linux)
  CFLAG += -DUNIX
  LFLAGLIB = -s -fPIC -shared
endif
ifeq ($(UNAME), Darwin)
  TARGET = DaoNetwork.dylib
  CFLAG += -DUNIX -DMAC_OSX
  LFLAGLIB = -s -fPIC -dynamiclib
endif

all: $(TARGET)

daoNetwork.o: daoNetwork.c
	$(CC) $(CFLAG) $(INCLUDE) daoNetwork.c
	
$(TARGET): daoNetwork.o
	$(CC) $(LFLAGLIB) $(LIB) daoNetwork.o -o $(TARGET)

bind: clean

clean:
	rm *.o
