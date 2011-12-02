PROJECT = serialization

include ../../Makefile.common

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..

TARGET = $(PROJECT).$(DAO_DLL_EXT)

all: $(TARGET)

dao_serialization.o: dao_serialization.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) dao_serialization.c
	
$(TARGET): dao_serialization.o
	$(DAO_CC) $(DAO_DLLFLAGS) dao_serialization.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
