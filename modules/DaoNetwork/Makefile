PROJECT = DaoNetwork

include ../../Makefile.common

DAO_INCS += -I../../kernel
DAO_LIBS += -L../..

ifneq (,$(findstring DWIN32,$(DAO_CFLAGS)))
	DAO_LFLAGS_NET = $(DAO_LFLAGS_DAO2) -lwsock32
else
	DAO_LFLAGS_NET = $(DAO_LFLAGS_DAO2)
endif

TARGET = $(PROJECT).$(DAO_DLL_EXT)

all: $(TARGET)

daoNetwork.o: daoNetwork.c
	$(DAO_CC) -c $(DAO_CFLAGS) $(DAO_INCS) daoNetwork.c

$(TARGET): daoNetwork.o
	$(DAO_CC) $(DAO_DLLFLAGS) daoNetwork.o $(DAO_LIBS) $(DAO_LFLAGS_NET) -o $(TARGET)

clean:
	-$(DEL_FILE) *.o
