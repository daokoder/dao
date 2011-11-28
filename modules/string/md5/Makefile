PROJECT = md5

include ../../../Makefile.common

DAO_INCS += -I../../../kernel
DAO_LIBS += -L../../..

TARGET = ../$(PROJECT).$(DAO_DLL_EXT)

all: $(TARGET)

dao_md5.o: dao_md5.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) dao_md5.c
	
$(TARGET): dao_md5.o
	$(DAO_CC) $(DAO_DLLFLAGS) dao_md5.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
