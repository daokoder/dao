PROJECT = DaoCGI

include ../../Makefile.common

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..

TARGET = $(PROJECT).$(DAO_DLL_EXT)

all: $(TARGET)

daoCGI.o: daoCGI.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) daoCGI.c
	
$(TARGET): daoCGI.o
	$(DAO_CC) $(DAO_DLLFLAGS) daoCGI.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
