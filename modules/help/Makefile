PROJECT = dao_help

include ../../Makefile.common

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..

# Build libdao_help target, so that other program or module can link against it:
TARGET = lib$(PROJECT).$(DAO_DLL_EXT)

ifdef WIN32
  TARGET = $(PROJECT).$(DAO_DLL_EXT)
endif

all: $(TARGET)

dao_help.o: dao_help.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) dao_help.c
	
$(TARGET): dao_help.o
	$(DAO_CC) $(DAO_DLLFLAGS) dao_help.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
