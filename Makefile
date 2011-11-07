
####### Installation directory
DAO_DIR = /usr/local/dao
DAO_LIB_DIR = $(DAO_DIR)/lib
DAO_INC_DIR = $(DAO_DIR)/include
DAO_TOOL_DIR = $(DAO_DIR)/tools

DAO_MACRO = -DDAO_WITH_MACRO
DAO_THREAD = -DDAO_WITH_THREAD
DAO_NUMARRAY = -DDAO_WITH_NUMARRAY
DAO_ASYNCLASS = -DDAO_WITH_ASYNCLASS
DAO_DYNCLASS = -DDAO_WITH_DYNCLASS
DAO_DECORATOR = -DDAO_WITH_DECORATOR
DAO_SERIALIZ = -DDAO_WITH_SERIALIZATION

USE_READLINE = -DDAO_USE_READLINE
LIB_READLINE = -lreadline -lncurses

DAO_CONFIG = $(DAO_MACRO) $(DAO_THREAD) $(DAO_NUMARRAY) $(DAO_SERIALIZ) $(DAO_ASYNCLASS) $(DAO_DYNCLASS) $(DAO_DECORATOR)

CC       ?= gcc
CFLAGS    = -Wall -Wno-unused -fPIC $(DAO_CONFIG)
INCPATH   = -I. -Ikernel
LFLAGS    = -fPIC
LFLAGSDLL = -fPIC
LIBS      = -L. -ldl -lpthread -lm $(LIB_READLINE)

# dynamic linked Dao interpreter, requires dao.so to run:
DAO_EXE   = dao

# Dao dynamic linking library
DAO_DLL	= dao.so

ARCHIVE = dao.a


CHANGESET_ID = $(shell hg id -i)

ifneq ($(CHANGESET_ID),)
  CFLAGS += -DCHANGESET_ID=\"HG.$(CHANGESET_ID)\"
endif


UNAME = $(shell uname)

ifeq ($(UNAME), Linux)
  CFLAGS += -DUNIX
  LFLAGSDLL += -shared -Wl,-soname,libdao.so
endif

ifeq ($(UNAME), Darwin)
  DAO_DLL	= dao.dylib
  CFLAGS += -DUNIX -DMAC_OSX
  LFLAGSDLL += -dynamiclib -install_name libdao.dylib
  LIBS += -L/usr/local/lib
endif

ifeq ($(debug),yes)
  CFLAGS += -ggdb -DDEBUG
  LFLAGS += -ggdb
else
  CFLAGS += -O2
  LFLAGS += -s
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

OBJECTS = kernel/daoArray.o \
		  kernel/daoMap.o \
		  kernel/daoType.o \
		  kernel/daoValue.o \
		  kernel/daoContext.o \
		  kernel/daoProcess.o \
		  kernel/daoRoutine.o \
		  kernel/daoGC.o \
		  kernel/daoStdtype.o \
		  kernel/daoNamespace.o \
		  kernel/daoString.o \
		  kernel/daoStdlib.o \
		  kernel/daoMacro.o \
		  kernel/daoLexer.o \
		  kernel/daoParser.o \
		  kernel/daoThread.o \
		  kernel/daoNumtype.o \
		  kernel/daoClass.o \
		  kernel/daoConst.o \
		  kernel/daoObject.o \
		  kernel/daoSched.o \
		  kernel/daoStream.o \
		  kernel/daoVmcode.o \
		  kernel/daoVmspace.o \
		  kernel/daoRegex.o

first: all

####### Implicit rules

.SUFFIXES: .c

.c.o:
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<

kernel/daoMaindl.o: kernel/daoMaindl.c
	$(CC) -c $(CFLAGS) $(USE_READLINE) $(INCPATH) -o $@ $<

####### Build rules

all: Makefile $(DAO_DLL) $(DAO_EXE) $(ARCHIVE)

static:  $(OBJECTS) kernel/daoMain.o
	$(CC) $(LFLAGS) -o dao $(OBJECTS) kernel/daoMain.o $(LIBS)

one:  $(OBJECTS) kernel/daoMainv.o
	$(CC) $(LFLAGS) -o daov $(OBJECTS) kernel/daoMainv.o $(LIBS)

$(DAO_EXE):  kernel/daoMaindl.o
	$(CC) $(LFLAGS) $(LIB_READLINE) -o $(DAO_EXE) kernel/daoMaindl.o $(LIBS) -ldao

$(DAO_DLL):  $(OBJECTS)
	$(CC) $(LFLAGSDLL) -o $(DAO_DLL) $(OBJECTS) $(LIBS)

$(ARCHIVE):  $(OBJECTS)
	$(AR) $(ARCHIVE) $(OBJECTS)

clean:
	-$(DEL_FILE) kernel/*.o dao.a
	-$(DEL_FILE) *~ core *.core

FORCE:

####### Install

install:
	@$(HAS_DIR) $(DAO_DIR) || $(MKDIR) $(DAO_DIR)
	@$(HAS_DIR) $(DAO_LIB_DIR) || $(MKDIR) $(DAO_LIB_DIR)
	@$(HAS_DIR) $(DAO_INC_DIR) || $(MKDIR) $(DAO_INC_DIR)
	@$(HAS_DIR) $(DAO_TOOL_DIR) || $(MKDIR) $(DAO_TOOL_DIR)

	$(HAS_FILE) addpath.dao && $(COPY_FILE) addpath.dao $(DAO_DIR)
	$(HAS_FILE) tools/autobind.dao && $(COPY_FILE) tools/autobind.dao $(DAO_TOOL_DIR)
	$(HAS_FILE) tools/autobind.dao && $(SYMLINK) $(DAO_TOOL_DIR)/autobind.dao /usr/bin/autobind.dao
	$(HAS_FILE) dao.conf && $(COPY_FILE) dao.conf $(DAO_DIR)
	$(HAS_FILE) $(DAO_DLL) && $(COPY_FILE) $(DAO_DLL) $(DAO_DIR) && $(SYMLINK) $(DAO_DIR)/$(DAO_DLL) /usr/lib/lib$(DAO_DLL)

	$(COPY_FILE) kernel/*.h $(DAO_INC_DIR)
	$(COPY_FILE) $(DAO_EXE) kernel/dao.h $(DAO_DIR)
	$(SYMLINK) $(DAO_DIR)/$(DAO_EXE) /usr/bin/$(DAO_EXE)
	$(SYMLINK) $(DAO_DIR)/dao.h /usr/include/dao.h

uninstall:
	@$(HAS_FILE) /usr/bin/autobind.dao && $(DEL_FILE) /usr/bin/autobind.dao
	$(DEL_FILE) /usr/bin/$(DAO_EXE) /usr/lib/lib$(DAO_DLL) /usr/include/dao.h $(DAO_DIR)
