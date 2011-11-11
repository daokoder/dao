
CC = clang++
CC = g++

DAO_INC =../../kernel/

CLANG_LIBS = -lclang \
-lclangAST \
-lclangAnalysis \
-lclangBasic \
-lclangCodeGen \
-lclangDriver \
-lclangFrontend \
-lclangFrontendTool \
-lclangIndex \
-lclangLex \
-lclangParse \
-lclangRewrite \
-lclangSema \
-lclangSerialization \
-lclangStaticAnalyzerCheckers \
-lclangStaticAnalyzerCore \
-lclangStaticAnalyzerFrontend

CFLAGS = -O2 -fno-rtti -Wall -Wno-unused -I. -I$(DAO_INC) `llvm-config --cppflags`
CFLAGS = -ggdb -fno-rtti -Wall -Wno-unused -I. -I$(DAO_INC) `llvm-config --cppflags`
LFLAGS = -fPIC $(CLANG_LIBS) `llvm-config --ldflags --libs`

TARGET = DaoCxxInliner.so


UNAME = $(shell uname)

ifeq ($(UNAME), Linux)
	CFLAGS += -DUNIX
	LFLAGS += -shared -Wl,-soname,DaoCxxInliner.so
endif

ifeq ($(UNAME), Darwin)
	CFLAGS += -DUNIX -DMAC_OSX
	LFLAGS += -dynamiclib -undefined dynamic_lookup -install_name DaoCxxInliner.dylib
	TARGET = DaoCxxInliner.dylib
endif

OBJECTS = daoCxxInliner.o

first: all

all: $(TARGET)

.SUFFIXES: .c .obj .cpp .cc .cxx .C

.cxx.o:
	$(CC) -c $(CFLAGS) -o $@ $<

.cpp.o:
	$(CC) -c $(CFLAGS) -o $@ $<

$(TARGET): $(OBJECTS)
	$(CC) $(LFLAGS) $(OBJECTS) -o $(TARGET)

clean:
	rm *.o
