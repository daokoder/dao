
TARGET = daomake$(EXESUFFIX)

DAO_SRC_DIR =../../../kernel

INCS = -I$(DAO_SRC_DIR)
CFLAGS = $(DEBUG) -O0 $(SYSCFLAGS) -DDAO_WITH_REGEX
LFLAGS = $(SYSLIBS)


HEADERS = $(DAO_SRC_DIR)/dao.h $(DAO_SRC_DIR)/daoBase.h $(DAO_SRC_DIR)/daoGC.h \
		  $(DAO_SRC_DIR)/daoObject.h $(DAO_SRC_DIR)/daoRoutine.h \
		  $(DAO_SRC_DIR)/daoThread.h $(DAO_SRC_DIR)/daoArray.h \
		  $(DAO_SRC_DIR)/daoLexer.h $(DAO_SRC_DIR)/daoOptimizer.h \
		  $(DAO_SRC_DIR)/daoStdlib.h $(DAO_SRC_DIR)/daoType.h \
		  $(DAO_SRC_DIR)/daoParser.h $(DAO_SRC_DIR)/daoStdtype.h \
		  $(DAO_SRC_DIR)/daoValue.h $(DAO_SRC_DIR)/daoBytecode.h \
		  $(DAO_SRC_DIR)/daoMap.h $(DAO_SRC_DIR)/daoPlatforms.h \
		  $(DAO_SRC_DIR)/daoStream.h $(DAO_SRC_DIR)/daoVmcode.h \
		  $(DAO_SRC_DIR)/daoClass.h $(DAO_SRC_DIR)/daoNamespace.h \
		  $(DAO_SRC_DIR)/daoProcess.h $(DAO_SRC_DIR)/daoString.h \
		  $(DAO_SRC_DIR)/daoVmspace.h $(DAO_SRC_DIR)/daoConst.h \
		  $(DAO_SRC_DIR)/daoNumtype.h $(DAO_SRC_DIR)/daoRegex.h \
		  $(DAO_SRC_DIR)/daoTasklet.h


first: all

daoArray.o: $(HEADERS) $(DAO_SRC_DIR)/daoArray.c 
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoArray.c -o daoArray.o

daoMap.o: $(HEADERS) $(DAO_SRC_DIR)/daoMap.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoMap.c -o daoMap.o

daoType.o: $(HEADERS) $(DAO_SRC_DIR)/daoType.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoType.c -o daoType.o

daoValue.o: $(HEADERS) $(DAO_SRC_DIR)/daoValue.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoValue.c -o daoValue.o

daoProcess.o: $(HEADERS) $(DAO_SRC_DIR)/daoProcess.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoProcess.c -o daoProcess.o

daoRoutine.o: $(HEADERS) $(DAO_SRC_DIR)/daoRoutine.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoRoutine.c -o daoRoutine.o

daoGC.o: $(HEADERS) $(DAO_SRC_DIR)/daoGC.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoGC.c -o daoGC.o

daoStdtype.o: $(HEADERS) $(DAO_SRC_DIR)/daoStdtype.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoStdtype.c -o daoStdtype.o

daoNamespace.o: $(HEADERS) $(DAO_SRC_DIR)/daoNamespace.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoNamespace.c -o daoNamespace.o

daoString.o: $(HEADERS) $(DAO_SRC_DIR)/daoString.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoString.c -o daoString.o

daoStdlib.o: $(HEADERS) $(DAO_SRC_DIR)/daoStdlib.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoStdlib.c -o daoStdlib.o

daoLexer.o: $(HEADERS) $(DAO_SRC_DIR)/daoLexer.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoLexer.c -o daoLexer.o

daoParser.o: $(HEADERS) $(DAO_SRC_DIR)/daoParser.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoParser.c -o daoParser.o

daoBytecode.o: $(HEADERS) $(DAO_SRC_DIR)/daoBytecode.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoBytecode.c -o daoBytecode.o

daoOptimizer.o: $(HEADERS) $(DAO_SRC_DIR)/daoOptimizer.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoOptimizer.c -o daoOptimizer.o

daoThread.o: $(HEADERS) $(DAO_SRC_DIR)/daoThread.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoThread.c -o daoThread.o

daoPlatforms.o: $(HEADERS) $(DAO_SRC_DIR)/daoPlatforms.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoPlatforms.c -o daoPlatforms.o

daoNumtype.o: $(HEADERS) $(DAO_SRC_DIR)/daoNumtype.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoNumtype.c -o daoNumtype.o

daoClass.o: $(HEADERS) $(DAO_SRC_DIR)/daoClass.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoClass.c -o daoClass.o

daoConst.o: $(HEADERS) $(DAO_SRC_DIR)/daoConst.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoConst.c -o daoConst.o

daoObject.o: $(HEADERS) $(DAO_SRC_DIR)/daoObject.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoObject.c -o daoObject.o

daoTasklet.o: $(HEADERS) $(DAO_SRC_DIR)/daoTasklet.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoTasklet.c -o daoTasklet.o

daoStream.o: $(HEADERS) $(DAO_SRC_DIR)/daoStream.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoStream.c -o daoStream.o

daoVmcode.o: $(HEADERS) $(DAO_SRC_DIR)/daoVmcode.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoVmcode.c -o daoVmcode.o

daoVmspace.o: $(HEADERS) $(DAO_SRC_DIR)/daoVmspace.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoVmspace.c -o daoVmspace.o

daoRegex.o: $(HEADERS) $(DAO_SRC_DIR)/daoRegex.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoRegex.c -o daoRegex.o

daoMake.o: $(HEADERS) ../source/daoMake.c
	$(CC) -c $(CFLAGS) $(INCS) ../source/daoMake.c -o daoMake.o


OBJECTS = daoConst.o daoVmcode.o \
		  daoString.o daoArray.o daoMap.o \
		  daoValue.o daoStdtype.o daoNumtype.o \
		  daoRoutine.o daoClass.o daoObject.o \
		  daoProcess.o daoNamespace.o daoVmspace.o \
		  daoLexer.o daoParser.o daoBytecode.o \
		  daoType.o daoOptimizer.o daoStdlib.o \
		  daoStream.o daoRegex.o daoGC.o \
		  daoThread.o daoTasklet.o daoPlatforms.o daoMake.o



all: $(TARGET)

$(TARGET): $(OBJECTS) daoMake.o
	$(CC) $(OBJECTS) -o $(TARGET) $(LFLAGS)

clean:
	./daomake remove $(OBJECTS)



linux:
	$(MAKE) DEBUG=$(DEBUG) SYSCFLAGS="-DUNIX -DLINUX -fPIC" SYSLIBS="-lm -ldl"

macosx:
	$(MAKE) DEBUG=$(DEBUG) SYSCFLAGS="-DUNIX -DMAC_OSX -fPIC" SYSLIBS=""

freebsd:
	$(MAKE) DEBUG=$(DEBUG) SYSCFLAGS="-DUNIX -DFREEBSD -fPIC" SYSLIBS="-lm"

mingw:
	$(MAKE) DEBUG=$(DEBUG) SYSCFLAGS="-DWIN32 -DMINGW" SYSLIBS="-lwinmm" EXESUFFIX=".exe"

minix:
	$(MAKE) DEBUG=$(DEBUG) SYSCFLAGS="-DUNIX -DMINIX -fPIC" SYSLIBS="-lm"

beos haiku:
	$(MAKE) DEBUG=$(DEBUG) SYSCFLAGS="-DUNIX -DBEOS -fPIC" SYSLIBS=""
