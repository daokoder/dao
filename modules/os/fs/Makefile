PROJECT = fs

include ../../../Makefile.common

DAO_INCS += -I../../../kernel
DAO_LIBS += -L../../..

TARGET = ../$(PROJECT).$(DAO_DLL_EXT)

all: $(TARGET)

dao_fs.o: dao_fs.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) dao_fs.c

$(TARGET): dao_fs.o
	$(DAO_CC) $(DAO_DLLFLAGS) dao_fs.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
