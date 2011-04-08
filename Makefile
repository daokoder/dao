
####### Installation directory
DAO_DIR = /usr/local/dao
DAO_LIB_DIR = $(DAO_DIR)/lib
DAO_INC_DIR = $(DAO_DIR)/include
DAO_TOOL_DIR = $(DAO_DIR)/tools

DAO_MACRO = -DDAO_WITH_MACRO
DAO_THREAD = -DDAO_WITH_THREAD
DAO_NUMARRAY = -DDAO_WITH_NUMARRAY
DAO_SYNCLASS = -DDAO_WITH_SYNCLASS

#DAO_ASMBC = -DDAO_WITH_ASMBC

#USE_READLINE = -DDAO_USE_READLINE
#LIB_READLINE = -lreadline

DAO_CONFIG = $(DAO_MACRO) $(DAO_THREAD) $(DAO_NUMARRAY) $(DAO_SYNCLASS) $(DAO_ASMBC) $(USE_READLINE)

CC        = $(CROSS_COMPILE)gcc
CFLAGS    += -Wall -Wno-unused -fPIC -O2 -DUNIX $(DAO_CONFIG) #-DDEBUG -ggdb #-DDAO_GC_PROF
INCPATH   = -I. -Ikernel
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
  LFLAGS  += -s
  LFLAGLIB = -s -fPIC -shared
  LFLAGSDLL += -shared -Wl,-soname,libdao.so
endif

ifeq ($(UNAME), Darwin)
  TARGETDLL	= dao.dylib
  CFLAGS += -DUNIX -DMAC_OSX
  LFLAGLIB = -fPIC -dynamiclib
  LFLAGSDLL += -dynamiclib -install_name libdao.dylib
  LIBS += -L/usr/local/lib
endif

ifeq ($(CC), gcc)
  ifeq ($(debug),yes)
    CFLAGS += -ggdb -DDEBUG
  endif
endif

ifeq ($(std),C90)
  CFLAGS += -ansi -pedantic
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

SOURCES = kernel/daoType.c \
    kernel/daoStdtype.c \
		kernel/daoNamespace.c \
		kernel/daoGC.c \
		kernel/daoNumtype.c \
		kernel/daoMaindl.c \
		kernel/daoClass.c \
		kernel/daoLexer.c \
		kernel/daoParser.c \
		kernel/daoMacro.c \
		kernel/daoAsmbc.c \
		kernel/daoRegex.c \
		kernel/daoValue.c \
		kernel/daoContext.c \
		kernel/daoProcess.c \
		kernel/daoStdlib.c \
		kernel/daoArray.c \
		kernel/daoMap.c \
		kernel/daoConst.c \
		kernel/daoRoutine.c \
		kernel/daoObject.c \
		kernel/daoThread.c \
		kernel/daoSched.c \
		kernel/daoStream.c \
		kernel/daoString.c \
		kernel/daoVmspace.c
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
objs/daoMaindl.o: kernel/daoMaindl.c
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoMaindl.o kernel/daoMaindl.c

objs/daoMain.o: kernel/daoMain.c
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoMain.o kernel/daoMain.c

objs/daoMainv.o: kernel/daoMainv.c
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoMainv.o kernel/daoMainv.c

#dll
objs/daoType.o: kernel/daoType.c kernel/daoType.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoType.o kernel/daoType.c

objs/daoStdtype.o: kernel/daoStdtype.c kernel/daoStdtype.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoStdtype.o kernel/daoStdtype.c

objs/daoNamespace.o: kernel/daoNamespace.c kernel/daoNamespace.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoNamespace.o kernel/daoNamespace.c

objs/daoNumtype.o: kernel/daoNumtype.c kernel/daoNumtype.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoNumtype.o kernel/daoNumtype.c

objs/daoClass.o: kernel/daoClass.c kernel/daoClass.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoClass.o kernel/daoClass.c

objs/daoRegex.o: kernel/daoRegex.c kernel/daoRegex.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoRegex.o kernel/daoRegex.c

objs/daoContext.o: kernel/daoContext.c kernel/daoType.h kernel/daoContext.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoContext.o kernel/daoContext.c

objs/daoProcess.o: kernel/daoProcess.c kernel/daoType.h kernel/daoProcess.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoProcess.o kernel/daoProcess.c

objs/daoValue.o: kernel/daoValue.c kernel/daoValue.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoValue.o kernel/daoValue.c

objs/daoArray.o: kernel/daoArray.c kernel/daoArray.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoArray.o kernel/daoArray.c

objs/daoMap.o: kernel/daoMap.c kernel/daoMap.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoMap.o kernel/daoMap.c

objs/daoConst.o: kernel/daoConst.c
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoConst.o kernel/daoConst.c

objs/daoRoutine.o: kernel/daoRoutine.c kernel/daoRoutine.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoRoutine.o kernel/daoRoutine.c

objs/daoObject.o: kernel/daoObject.c kernel/daoObject.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoObject.o kernel/daoObject.c

objs/daoSched.o: kernel/daoSched.c kernel/daoSched.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoSched.o kernel/daoSched.c

objs/daoStream.o: kernel/daoStream.c kernel/daoStream.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoStream.o kernel/daoStream.c

objs/daoString.o: kernel/daoString.c kernel/daoString.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoString.o kernel/daoString.c

objs/daoVmspace.o: kernel/daoVmspace.c kernel/daoVmspace.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoVmspace.o kernel/daoVmspace.c

objs/daoGC.o: kernel/daoGC.c kernel/daoGC.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoGC.o kernel/daoGC.c

objs/daoStdlib.o: kernel/daoStdlib.c kernel/daoStdlib.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoStdlib.o kernel/daoStdlib.c

objs/daoMacro.o: kernel/daoMacro.c kernel/daoMacro.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoMacro.o kernel/daoMacro.c

objs/daoLexer.o: kernel/daoLexer.c kernel/daoLexer.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoLexer.o kernel/daoLexer.c

objs/daoParser.o: kernel/daoParser.c kernel/daoParser.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoParser.o kernel/daoParser.c

objs/daoAsmbc.o: kernel/daoAsmbc.c kernel/daoAsmbc.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoAsmbc.o kernel/daoAsmbc.c

objs/daoThread.o: kernel/daoThread.c kernel/daoThread.h
	$(CC) -c $(CFLAGS) $(INCPATH) -o objs/daoThread.o kernel/daoThread.c


####### Install

install:
	@$(HAS_DIR) $(DAO_DIR) || $(MKDIR) $(DAO_DIR)
	@$(HAS_DIR) $(DAO_LIB_DIR) || $(MKDIR) $(DAO_LIB_DIR)
	@$(HAS_DIR) $(DAO_INC_DIR) || $(MKDIR) $(DAO_INC_DIR)
	@$(HAS_DIR) $(DAO_TOOL_DIR) || $(MKDIR) $(DAO_TOOL_DIR)

	$(HAS_FILE) addpath.dao && $(COPY_FILE) addpath.dao $(DAO_DIR)
	$(HAS_FILE) tools/autobind.dao && $(COPY_FILE) tools/autobind.dao $(DAO_TOOL_DIR)
	$(HAS_FILE) tools/autobind.dao && $(SYMLINK) $(DAO_TOOL_DIR)/autobind.dao /usr/bin/autobind.dao

	$(COPY_FILE) kernel/*.h $(DAO_INC_DIR)
	$(COPY_FILE) $(TARGET) $(TARGETDLL) kernel/dao.h kernel/daolib.h dao.conf $(DAO_DIR)
	$(SYMLINK) $(DAO_DIR)/$(TARGET) /usr/bin/$(TARGET)
	$(SYMLINK) $(DAO_DIR)/$(TARGETDLL) /usr/lib/lib$(TARGETDLL)
	$(SYMLINK) $(DAO_DIR)/dao.h /usr/include/dao.h
	$(SYMLINK) $(DAO_DIR)/daolib.h /usr/include/daolib.h

uninstall:
	@$(HAS_FILE) /usr/bin/autobind.dao && $(DEL_FILE) /usr/bin/autobind.dao
	$(DEL_FILE) /usr/bin/$(TARGET) /usr/lib/lib$(TARGETDLL) /usr/include/dao.h /usr/include/daolib.h $(DAO_DIR)
