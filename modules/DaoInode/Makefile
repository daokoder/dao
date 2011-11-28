PROJECT = DaoInode

include ../../Makefile.common

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..

TARGET = $(PROJECT).$(DAO_DLL_EXT)

all: $(TARGET)

daoInode.o: daoInode.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) daoInode.c

$(TARGET): daoInode.o
	$(DAO_CC) $(DAO_DLLFLAGS) daoInode.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
