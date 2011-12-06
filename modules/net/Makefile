PROJECT = net

include ../../Makefile.common

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..

ifdef WIN32
	DAO_LFLAGS_NET = $(DAO_LFLAGS_DAO2) -lwsock32
else
	DAO_LFLAGS_NET = $(DAO_LFLAGS_DAO2)
endif

TARGET = $(PROJECT).$(DAO_DLL_EXT)

all: $(TARGET)

dao_network.o: dao_network.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) dao_network.c

$(TARGET): dao_network.o
	$(DAO_CC) $(DAO_DLLFLAGS) dao_network.o $(DAO_LIBS) $(DAO_LFLAGS_NET) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
