PROJECT = sync

include ../../Makefile.common

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..

# Build libdao_sys target, so that other program or module can link against it:
TARGET = lib$(PROJECT).$(DAO_DLL_EXT)

ifdef WIN32
  TARGET = $(PROJECT).$(DAO_DLL_EXT)
endif

all: $(TARGET)

dao_sync.o: dao_sync.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) dao_sync.c
	
$(TARGET): dao_sync.o
	$(DAO_CC) $(DAO_DLLFLAGS) dao_sync.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
