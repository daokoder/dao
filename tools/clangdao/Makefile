
CC = g++-4.2

DAO_INC =/usr/local/dao/include

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

TARGET = clangdao

OBJECTS = cdaoVariable.o cdaoFunction.o cdaoUserType.o cdaoNamespace.o cdaoModule.o clangdao.o

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
