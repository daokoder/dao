PROJECT = DaoJSON

include ../../Makefile.common

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..

TARGET = $(PROJECT).$(DAO_DLL_EXT)

all: $(TARGET)

DaoJSON.o: DaoJSON.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) DaoJSON.c

$(TARGET): DaoJSON.o
	$(DAO_CC) $(DAO_DLLFLAGS) DaoJSON.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
