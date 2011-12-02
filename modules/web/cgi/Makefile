PROJECT = cgi

include ../../../Makefile.common

DAO_INCS += -I../../../kernel
DAO_LIBS += -L../../..

TARGET = ../$(PROJECT).$(DAO_DLL_EXT)

all: $(TARGET)

dao_cgi.o: dao_cgi.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) dao_cgi.c
	
$(TARGET): dao_cgi.o
	$(DAO_CC) $(DAO_DLLFLAGS) dao_cgi.o $(DAO_LIBS) $(DAO_LFLAGS_DAO2) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
