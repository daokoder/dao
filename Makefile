
####### Installation directory
DAO_DIR = /usr/local/dao
DAO_LIB_DIR = $(DAO_DIR)/lib
DAO_INC_DIR = $(DAO_DIR)/include
DAO_TOOL_DIR = $(DAO_DIR)/tools

#DAO_AFC = -DDAO_WITH_AFC
#DAO_MPI = -DDAO_WITH_MPI
DAO_MACRO = -DDAO_WITH_MACRO
DAO_THREAD = -DDAO_WITH_THREAD
DAO_NETWORK = -DDAO_WITH_NETWORK
DAO_NUMARRAY = -DDAO_WITH_NUMARRAY

#DAO_ASMBC = -DDAO_WITH_ASMBC
#DAO_JIT = -DDAO_WITH_JIT

USE_READLINE = -DDAO_USE_READLINE
LIB_READLINE = -lreadline

DAO_CONFIG = $(DAO_MACRO) $(DAO_THREAD) $(DAO_NUMARRAY) $(DAO_NETWORK) $(DAO_MPI) $(DAO_AFC) $(DAO_ASMBC) $(DAO_JIT) $(USE_READLINE)

CC        = gcc -ggdb
CFLAGS    = -Wall -fPIC -O2 -DUNIX $(DAO_CONFIG) #-DDEBUG -ggdb #-DDAO_GC_PROF
INCPATH   = -I. -Isrc
LFLAGS    = -fPIC #-s
LFLAGSDLL = -fPIC #-s
#LFLAGSDLL = -shared -fPIC -s -Wl,--version-script=daolibsym.map
LIBS      = -L. -ldl -lpthread -lm

# dynamic linked Dao interpreter, requires dao.so to run:
TARGET   = dao

# Dao dynamic linking library
TARGETDLL	= dao.so

ARCHIVE = dao.a


UNAME = $(shell uname)

ifeq ($(UNAME), Linux)
  CFLAGS += -DUNIX
  LFLAGLIB = -s -fPIC -shared
  LFLAGSDLL += -shared -Wl,-soname,libdao.so
endif

ifeq ($(UNAME), Darwin)
  TARGETDLL	= dao.dylib
  CFLAGS += -DUNIX -DMAC_OSX
  LFLAGLIB = -fPIC -dynamiclib
  LFLAGSDLL += -dynamiclib -install_name libdao.dylib
  LIBS += -L/opt/local/lib
endif

AR = ar rcs

COPY      = cp
COPY_FILE = $(COPY) -f
COPY_DIR  = $(COPY) -r
DEL_FILE  = rm -fR
SYMLINK   = ln -sf
MKDIR     = mkdir -p
HAS_DIR = test -d
HAS_FILE = test -f

####### Output directory

OBJECTS_DIR = objs/

####### Files

SOURCES = src/daoType.c \
    src/daoStdtype.c \
		src/daoNamespace.c \
		src/daoGC.c \
		src/daoNumtype.c \
		src/daoMaindl.c \
		src/daoClass.c \
		src/daoLexer.c \
		src/daoParser.c \
		src/daoMacro.c \
		src/daoAsmbc.c \
		src/daoRegex.c \
		src/daoValue.c \
		src/daoContext.c \
		src/daoProcess.c \
		src/daoJit.c \
		src/daoStdlib.c \
		src/daoArray.c \
		src/daoMap.c \
		src/daoConst.c \
		src/daoRoutine.c \
		src/daoObject.c \
		src/daoThread.c \
		src/daoNetwork.c \
		src/daoSched.c \
		src/daoStream.c \
		src/daoString.c \
		src/daoVmspace.c
OBJECTS = \
			 objs/daoArray.o \
			 objs/daoMap.o \
			 objs/daoValue.o \
			 objs/daoContext.o \
			 objs/daoProcess.o \
		objs/daoType.o \
		objs/daoStdtype.o \
		objs/daoNamespace.o \
		objs/daoGC.o \
		objs/daoRoutine.o \
		objs/daoString.o \
		objs/daoJit.o \
		objs/daoStdlib.o \
		objs/daoMacro.o \
		objs/daoAsmbc.o \
		objs/daoLexer.o \
		objs/daoParser.o \
		objs/daoThread.o \
		objs/daoNumtype.o \
		objs/daoClass.o \
		objs/daoConst.o \
		objs/daoObject.o \
		objs/daoNetwork.o \
		objs/daoSched.o \
		objs/daoStream.o \
		objs/daoVmspace.o \
		objs/daoRegex.o 

first: all
####### Implicit rules

.SUFFIXES: .c .o .cpp .cc .cxx .C

.cpp.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<

.cc.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<

.cxx.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<

.C.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<

.c.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<

####### Build rules

all: Makefile OutputFold $(TARGET) $(TARGETDLL) $(ARCHIVE)

static:  $(OBJECTS) objs/daoMain.o
	$(CC) $(LFLAGS) -o dao $(OBJECTS) objs/daoMain.o $(LIBS)

one:  $(OBJECTS) objs/daoMainv.o
	$(CC) $(LFLAGS) -o daov $(OBJECTS) objs/daoMainv.o $(LIBS)
	
$(TARGET):  objs/daoMaindl.o
	$(CC) $(LFLAGS) -o $(TARGET) objs/daoMaindl.o $(LIBS) $(LIB_READLINE)
	
$(TARGETDLL):  $(OBJECTS)
	$(CC) $(LFLAGSDLL) -o $(TARGETDLL) $(OBJECTS) $(LIBS)

$(ARCHIVE):  $(OBJECTS)
	$(AR) $(ARCHIVE) $(OBJECTS)

OutputFold:
	@$(HAS_DIR) $(OBJECTS_DIR) || $(MKDIR) $(OBJECTS_DIR)

clean:
	-$(DEL_FILE) objs/*.o dao.a
	-$(DEL_FILE) *~ core *.core

FORCE:

####### Compile

#main
objs/daoMaindl.o: src/daoMaindl.c 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoMaindl.o src/daoMaindl.c
	
objs/daoMain.o: src/daoMain.c 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoMain.o src/daoMain.c

objs/daoMainv.o: src/daoMainv.c 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoMainv.o src/daoMainv.c

#dll
objs/daoType.o: src/daoType.c src/daoType.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoType.o src/daoType.c

objs/daoStdtype.o: src/daoStdtype.c src/daoStdtype.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoStdtype.o src/daoStdtype.c

objs/daoNamespace.o: src/daoNamespace.c src/daoNamespace.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoNamespace.o src/daoNamespace.c

objs/daoNumtype.o: src/daoNumtype.c src/daoNumtype.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoNumtype.o src/daoNumtype.c

objs/daoClass.o: src/daoClass.c src/daoClass.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoClass.o src/daoClass.c

objs/daoRegex.o: src/daoRegex.c src/daoRegex.h 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoRegex.o src/daoRegex.c

objs/daoContext.o: src/daoContext.c src/daoType.h src/daoContext.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoContext.o src/daoContext.c

objs/daoProcess.o: src/daoProcess.c src/daoType.h src/daoProcess.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoProcess.o src/daoProcess.c

objs/daoValue.o: src/daoValue.c src/daoValue.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoValue.o src/daoValue.c

objs/daoArray.o: src/daoArray.c src/daoArray.h 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoArray.o src/daoArray.c

objs/daoMap.o: src/daoMap.c src/daoMap.h 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoMap.o src/daoMap.c
	
objs/daoConst.o: src/daoConst.c
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoConst.o src/daoConst.c
	
objs/daoRoutine.o: src/daoRoutine.c src/daoRoutine.h 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoRoutine.o src/daoRoutine.c

objs/daoObject.o: src/daoObject.c src/daoObject.h 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoObject.o src/daoObject.c

objs/daoNetwork.o: src/daoNetwork.c
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoNetwork.o src/daoNetwork.c

objs/daoSched.o: src/daoSched.c src/daoSched.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoSched.o src/daoSched.c

objs/daoStream.o: src/daoStream.c src/daoStream.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoStream.o src/daoStream.c

objs/daoString.o: src/daoString.c src/daoString.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoString.o src/daoString.c

objs/daoVmspace.o: src/daoVmspace.c src/daoVmspace.h 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoVmspace.o src/daoVmspace.c

objs/daoGC.o: src/daoGC.c src/daoGC.h 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoGC.o src/daoGC.c

objs/daoStdlib.o: src/daoStdlib.c src/daoStdlib.h 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoStdlib.o src/daoStdlib.c

objs/daoMacro.o: src/daoMacro.c src/daoMacro.h 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoMacro.o src/daoMacro.c

objs/daoLexer.o: src/daoLexer.c src/daoLexer.h 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoLexer.o src/daoLexer.c

objs/daoParser.o: src/daoParser.c src/daoParser.h 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoParser.o src/daoParser.c

objs/daoAsmbc.o: src/daoAsmbc.c src/daoAsmbc.h 
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoAsmbc.o src/daoAsmbc.c

objs/daoThread.o: src/daoThread.c src/daoThread.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoThread.o src/daoThread.c

objs/daoJit.o: src/daoJit.c
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoJit.o src/daoJit.c

####### Install

install:  
	@$(HAS_DIR) $(DAO_DIR) || $(MKDIR) $(DAO_DIR)
	@$(HAS_DIR) $(DAO_LIB_DIR) || $(MKDIR) $(DAO_LIB_DIR)
	@$(HAS_DIR) $(DAO_INC_DIR) || $(MKDIR) $(DAO_INC_DIR)
	@$(HAS_DIR) $(DAO_TOOL_DIR) || $(MKDIR) $(DAO_TOOL_DIR)

	$(HAS_FILE) addpath.dao && $(COPY_FILE) addpath.dao $(DAO_DIR)
	$(HAS_FILE) tools/autobind.dao && $(COPY_FILE) tools/autobind.dao $(DAO_TOOL_DIR)
	$(HAS_FILE) tools/autobind.dao && $(SYMLINK) $(DAO_TOOL_DIR)/autobind.dao /usr/bin/autobind.dao

	$(COPY_FILE) src/*.h $(DAO_INC_DIR)
	$(COPY_FILE) $(TARGET) $(TARGETDLL) src/dao.h src/daolib.h dao.conf $(DAO_DIR)
	$(SYMLINK) $(DAO_DIR)/$(TARGET) /usr/bin/$(TARGET)
	$(SYMLINK) $(DAO_DIR)/$(TARGETDLL) /usr/lib/lib$(TARGETDLL)
	$(SYMLINK) $(DAO_DIR)/dao.h /usr/include/dao.h
	$(SYMLINK) $(DAO_DIR)/daolib.h /usr/include/daolib.h

uninstall:
	@$(HAS_FILE) /usr/bin/autobind.dao && $(DEL_FILE) /usr/bin/autobind.dao
	$(DEL_FILE) /usr/bin/$(TARGET) /usr/lib/lib$(TARGETDLL) /usr/include/dao.h /usr/include/daolib.h $(DAO_DIR)
