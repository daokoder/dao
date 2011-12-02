PROJECT = json

include ../../../Makefile.common

DAO_INCS += -I../../../kernel
DAO_LIBS += -L../../..

TARGET = ../$(PROJECT).$(DAO_DLL_EXT)

all: $(TARGET)

dao_json.o: dao_json.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) dao_json.c

$(TARGET): dao_json.o
	$(DAO_CC) $(DAO_DLLFLAGS) dao_json.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
