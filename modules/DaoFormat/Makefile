
CC = gcc
INCLUDE = -I. -I../../kernel
CFLAG = -c -Wall -O2 -fPIC
LFLAG = -s -shared
LIB =

TARGET = DaoFormat.so

UNAME = $(shell uname)

ifeq ($(UNAME), Linux)
  CFLAG += -DUNIX
  LFLAG = -s -fPIC -shared
endif

ifeq ($(UNAME), Darwin)
  TARGET = DaoFormat.dylib
  CFLAG += -DUNIX -DMAC_OSX
  LFLAG = -fPIC -dynamiclib -undefined dynamic_lookup
endif

ifeq ($(CC), gcc)
  ifeq ($(debug),yes)
    CFLAGS += -ggdb -DDEBUG
  endif
endif


all: $(TARGET)

daoFormat.o: daoFormat.c
	$(CC) $(CFLAG) $(INCLUDE) daoFormat.c
	
$(TARGET): daoFormat.o
	$(CC) $(LFLAG) daoFormat.o $(LIB) -o $(TARGET)

bind: clean

clean:
	rm *.o
