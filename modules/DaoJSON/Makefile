
CC = gcc
INCLUDE = -I. -I../../kernel
LIB = dao.dll
CFLAG = -c -Wall -O2 -fPIC
LFLAG = -s -shared

TARGET = DaoJSON.so

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

DaoJSON.o: DaoJSON.c
	$(CC) $(CFLAG) $(INCLUDE) DaoJSON.c
	
$(TARGET): DaoJSON.o
	$(CC) $(LFLAG) DaoJSON.o $(LIB) -o $(TARGET)

bind: clean

clean:
	rm *.o
