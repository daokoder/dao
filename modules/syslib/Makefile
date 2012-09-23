PROJECT = dao_sys

include ../../Makefile.common

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..

# Build libdao_sys target, so that other program or module can link against it:
TARGET = lib$(PROJECT).$(DAO_DLL_EXT)

ifdef WIN32
  TARGET = $(PROJECT).$(DAO_DLL_EXT)
endif

all: $(TARGET)

dao_sys.o: dao_sys.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) dao_sys.c
	
$(TARGET): dao_sys.o
	$(DAO_CC) $(DAO_DLLFLAGS) dao_sys.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

static_example: ${TARGET} dao_sys.o
	../../dao -c example.dao
	../../dao -a example.dac
	../../dao ../../tools/filetools/archive.dao example.dar
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) -DDAO_WITH_STATIC_MODULES ../../kernel/daoMain.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) example.dar.c
	$(DAO_CC) $(DAO_LFLAGS) ../../libdao.a daoMain.o dao_sys.o example.dar.o -o example

clean:
	-$(DEL_FILE) *.o
