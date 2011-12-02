PROJECT = reflection

include ../../Makefile.common

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..

TARGET = $(PROJECT).$(DAO_DLL_EXT)

all: $(TARGET)

dao_reflection.o: dao_reflection.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) dao_reflection.c
	
$(TARGET): dao_reflection.o
	$(DAO_CC) $(DAO_DLLFLAGS) dao_reflection.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
