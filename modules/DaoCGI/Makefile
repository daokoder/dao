
include ../../Makefile.common

DAO_INCS += -I../../kernel

all: DaoCGI.so

daoCGI.o: daoCGI.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) daoCGI.c
	
DaoCGI.so: daoCGI.o
	$(DAO_CC) $(DAO_DLLFLAGS) $(DAO_LIBS) daoCGI.o -o DaoCGI.so

clean:
	-$(DEL_FILE) *.o
