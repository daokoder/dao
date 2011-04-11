
CC = g++

DAO_INC =/usr/local/dao/include

CFLAGS = -ggdb -O2 -Wall -Wno-unused -I. -I$(DAO_INC) `llvm-config --cppflags`
LFLAGS = -fPIC `llvm-config --ldflags --libs`
LFLAGS2 = -fPIC -ldao `llvm-config --ldflags --libs`

TARGET = libDaoJIT.so
TARGET2 = test

UNAME = $(shell uname)

ifeq ($(UNAME), Linux)
	CFLAGS += -DUNIX
	LFLAGS += -shared -Wl,-soname,libDaoJIT.so
endif

ifeq ($(UNAME), Darwin)
	CFLAGS += -DUNIX -DMAC_OSX
	LFLAGS += -dynamiclib -undefined dynamic_lookup -install_name libDaoJIT.dylib
	TARGET = libDaoJIT.dylib
endif


OBJECTS = daoJIT.o

first: all

all: $(TARGET) $(TARGET2)

.SUFFIXES: .c .obj .cpp .cc .cxx .C

.cxx.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.cpp.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(TARGET): $(OBJECTS)
	$(CC) $(LFLAGS) $(OBJECTS) -o $(TARGET)

$(TARGET2): test.o
	$(CC) $(LFLAGS2) test.o -o $(TARGET2)

clean:
	rm *.o
