
CC = gcc
INCLUDE = -I. -I../../kernel
LIB =
CFLAG = -c -Wall -O2 -fPIC
LFLAG = -s -shared 

TARGET = DaoInode.so

UNAME = $(shell uname)

ifeq ($(UNAME), Linux)
  CFLAG += -DUNIX
  LFLAG = -s -fPIC -shared
endif

ifeq ($(UNAME), Darwin)
  TARGET	= DaoInode.dylib
  CFLAG += -DUNIX -DMAC_OSX
  LFLAG = -fPIC -dynamiclib
endif

ifeq ($(CC), gcc)
  ifeq ($(debug),yes)
    CFLAGS += -ggdb -DDEBUG
  endif
endif


all: $(TARGET)

daoInode.o: daoInode.c
	$(CC) $(CFLAG) $(INCLUDE) daoInode.c
	
$(TARGET): daoInode.o
	$(CC) $(LFLAG) daoInode.o $(LIB) -o $(TARGET)

bind: clean

clean:
	rm *.o
