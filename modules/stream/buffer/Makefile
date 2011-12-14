PROJECT = buffer

include ../../../Makefile.common

DAO_INCS += -I../../../kernel -I../../syslib
DAO_LIBS += -L../../.. -L../../syslib -ldao_sys

TARGET = ../$(PROJECT).$(DAO_DLL_EXT)

all: $(TARGET)

dao_buffer.o: dao_buffer.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) dao_buffer.c

$(TARGET): dao_buffer.o
	$(DAO_CC) $(DAO_DLLFLAGS) dao_buffer.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
