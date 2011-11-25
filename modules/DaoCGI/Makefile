
include ../../Makefile.common

DAO_INCS += -I../../kernel

ifdef WIN32
  DAO_LIBS += -L$(DAO_DIR) -ldao
endif

TARGET = DaoCGI.$(DAO_DLL_EXT)

all: $(TARGET)

daoCGI.o: daoCGI.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) daoCGI.c
	
$(TARGET): daoCGI.o
	$(DAO_CC) $(DAO_DLLFLAGS) daoCGI.o $(DAO_LIBS) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
