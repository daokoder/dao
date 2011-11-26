
PROJECT = DaoCXX

include ../../Makefile.common

TARGET = $(PROJECT).$(DAO_DLL_EXT)

CLANG_LIBS = -lclangFrontend \
-lclangDriver \
-lclangCodeGen \
-lclangSema \
-lclangAnalysis \
-lclangAST \
-lclangParse \
-lclangLex \
-lclangBasic \
-lclangIndex \
-lclangSerialization


LLVM_CFLAGS = $(shell llvm-config --cppflags)
LLVM_LFLAGS = $(shell llvm-config --ldflags)
LLVM_LIBS = $(shell llvm-config --libs)

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..
DAO_CFLAGS += -fno-rtti $(LLVM_CFLAGS)
DAO_LFLAGS += $(CLANG_LIBS) $(LLVM_LIBS) $(LLVM_LFLAGS)

first: all

all: $(TARGET)

.SUFFIXES: .c .obj .cpp .cc .cxx .C

.cxx.o:
	$(DAO_CXX) -c $(DAO_CFLAGS) $(DAO_INCS) -o $@ $<

.cpp.o:
	$(DAO_CXX) -c $(DAO_CFLAGS) $(DAO_INCS) -o $@ $<

$(TARGET): daoCXX.o
	$(DAO_CXX) $(DAO_DLLFLAGS) daoCXX.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) $(DAO_LFLAGS) -o $(TARGET)

clean:
	rm *.o
