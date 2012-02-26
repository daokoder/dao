
PROJECT = DaoJIT

include ../../Makefile.common

TARGET = $(PROJECT).$(DAO_DLL_EXT)


LLVM_CFLAGS = $(shell llvm-config --cppflags)
LLVM_LFLAGS = $(shell llvm-config --ldflags)
LLVM_LIBS = $(shell llvm-config --libs)

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..
DAO_CFLAGS += -fno-rtti $(LLVM_CFLAGS)
DAO_LFLAGS += $(LLVM_LIBS) $(LLVM_LFLAGS)

first: all

all: $(TARGET)

.SUFFIXES: .c .obj .cpp .cc .cxx .C

.cxx.o:
	$(DAO_CXX) -c $(DAO_CFLAGS) $(DAO_INCS) -o $@ $<

.cpp.o:
	$(DAO_CXX) -c $(DAO_CFLAGS) $(DAO_INCS) -o $@ $<

$(TARGET): daoJIT.o
	$(DAO_CXX) $(DAO_DLLFLAGS) daoJIT.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) $(DAO_LFLAGS) -o $(TARGET)

clean:
	rm *.o
