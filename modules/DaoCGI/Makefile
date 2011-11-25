
include ../../Makefile.common

DAO_INCS += -I../../kernel

ifdef WIN32
  DAO_LIBS += -L$(DAO_DIR) -ldao
endif

all: DaoCGI.so

daoCGI.o: daoCGI.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) daoCGI.c
	
DaoCGI.so: daoCGI.o
	$(DAO_CC) $(DAO_DLLFLAGS) daoCGI.o $(DAO_LIBS) -o DaoCGI.so

clean:
	-$(DEL_FILE) *.o
