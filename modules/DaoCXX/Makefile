
PROJECT = DaoCXX

include ../../Makefile.common

TARGET = $(PROJECT).$(DAO_DLL_EXT)

CLANG_LIBS = -lclang \
-lclangAST \
-lclangAnalysis \
-lclangBasic \
-lclangCodeGen \
-lclangDriver \
-lclangFrontend \
-lclangIndex \
-lclangLex \
-lclangParse \
-lclangSema \
-lclangSerialization 

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..
DAO_CFLAGS += -fno-rtti `llvm-config --cppflags`
DAO_LFLAGS += $(CLANG_LIBS) `llvm-config --ldflags --libs`

first: all

all: $(TARGET)

.SUFFIXES: .c .obj .cpp .cc .cxx .C

.cxx.o:
	$(DAO_CXX) -c $(DAO_CFLAGS) $(DAO_INCS) -o $@ $<

.cpp.o:
	$(DAO_CXX) -c $(DAO_CFLAGS) $(DAO_INCS) -o $@ $<

$(TARGET): daoCXX.o
	$(DAO_CC) $(DAO_DLLFLAGS) daoCXX.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) $(DAO_LFLAGS) -o $(TARGET)

clean:
	rm *.o
