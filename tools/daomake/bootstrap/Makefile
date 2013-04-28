
TARGET = daomake$(EXESUFFIX)

DAO_SRC_DIR =../../../kernel

INCS = -I$(DAO_SRC_DIR)
CFLAGS = $(DEBUG) -O0 $(SYSCFLAGS) -DDAO_WITH_REGEX
LFLAGS = $(SYSLIBS)


first: all

daoArray.o: $(DAO_SRC_DIR)/daoArray.c 
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoArray.c -o daoArray.o

daoMap.o: $(DAO_SRC_DIR)/daoMap.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoMap.c -o daoMap.o

daoType.o: $(DAO_SRC_DIR)/daoType.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoType.c -o daoType.o

daoValue.o: $(DAO_SRC_DIR)/daoValue.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoValue.c -o daoValue.o

daoProcess.o: $(DAO_SRC_DIR)/daoProcess.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoProcess.c -o daoProcess.o

daoRoutine.o: $(DAO_SRC_DIR)/daoRoutine.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoRoutine.c -o daoRoutine.o

daoGC.o: $(DAO_SRC_DIR)/daoGC.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoGC.c -o daoGC.o

daoStdtype.o: $(DAO_SRC_DIR)/daoStdtype.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoStdtype.c -o daoStdtype.o

daoNamespace.o: $(DAO_SRC_DIR)/daoNamespace.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoNamespace.c -o daoNamespace.o

daoString.o: $(DAO_SRC_DIR)/daoString.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoString.c -o daoString.o

daoStdlib.o: $(DAO_SRC_DIR)/daoStdlib.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoStdlib.c -o daoStdlib.o

daoMacro.o: $(DAO_SRC_DIR)/daoMacro.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoMacro.c -o daoMacro.o

daoLexer.o: $(DAO_SRC_DIR)/daoLexer.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoLexer.c -o daoLexer.o

daoParser.o: $(DAO_SRC_DIR)/daoParser.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoParser.c -o daoParser.o

daoBytecode.o: $(DAO_SRC_DIR)/daoBytecode.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoBytecode.c -o daoBytecode.o

daoOptimizer.o: $(DAO_SRC_DIR)/daoOptimizer.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoOptimizer.c -o daoOptimizer.o

daoThread.o: $(DAO_SRC_DIR)/daoThread.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoThread.c -o daoThread.o

daoPlatforms.o: $(DAO_SRC_DIR)/daoPlatforms.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoPlatforms.c -o daoPlatforms.o

daoNumtype.o: $(DAO_SRC_DIR)/daoNumtype.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoNumtype.c -o daoNumtype.o

daoClass.o: $(DAO_SRC_DIR)/daoClass.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoClass.c -o daoClass.o

daoConst.o: $(DAO_SRC_DIR)/daoConst.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoConst.c -o daoConst.o

daoObject.o: $(DAO_SRC_DIR)/daoObject.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoObject.c -o daoObject.o

daoSched.o: $(DAO_SRC_DIR)/daoSched.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoSched.c -o daoSched.o

daoStream.o: $(DAO_SRC_DIR)/daoStream.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoStream.c -o daoStream.o

daoVmcode.o: $(DAO_SRC_DIR)/daoVmcode.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoVmcode.c -o daoVmcode.o

daoVmspace.o: $(DAO_SRC_DIR)/daoVmspace.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoVmspace.c -o daoVmspace.o

daoRegex.o: $(DAO_SRC_DIR)/daoRegex.c
	$(CC) -c $(CFLAGS) $(INCS) $(DAO_SRC_DIR)/daoRegex.c -o daoRegex.o

daoMake.o: ../source/daoMake.c
	$(CC) -c $(CFLAGS) $(INCS) ../source/daoMake.c -o daoMake.o


OBJECTS = daoConst.o daoVmcode.o \
		  daoString.o daoArray.o daoMap.o \
		  daoValue.o daoStdtype.o daoNumtype.o \
		  daoRoutine.o daoClass.o daoObject.o \
		  daoProcess.o daoNamespace.o daoVmspace.o \
		  daoLexer.o daoParser.o daoBytecode.o \
		  daoType.o daoOptimizer.o daoStdlib.o \
		  daoMacro.o daoStream.o daoRegex.o \
		  daoGC.o daoThread.o daoSched.o daoPlatforms.o daoMake.o



all: $(TARGET)

$(TARGET): $(OBJECTS) daoMake.o
	$(CC) $(OBJECTS) -o $(TARGET) $(LFLAGS)

clean:
	./daomake remove $(OBJECTS)



linux:
	$(MAKE) DEBUG=$(DEBUG) SYSCFLAGS="-DUNIX -fPIC" SYSLIBS="-lm -ldl"

macosx:
	$(MAKE) DEBUG=$(DEBUG) SYSCFLAGS="-DUNIX -DMAC_OSX -fPIC" SYSLIBS=""

freebsd:
	$(MAKE) DEBUG=$(DEBUG) SYSCFLAGS="-DUNIX -fPIC" SYSLIBS="-lm"

mingw:
	$(MAKE) DEBUG=$(DEBUG) SYSCFLAGS="-DWIN32" SYSLIBS="-lwinmm" EXESUFFIX=".exe"

minix:
	$(MAKE) DEBUG=$(DEBUG) SYSCFLAGS="-DUNIX -fPIC" SYSLIBS="-lm"

beos haiku:
	$(MAKE) DEBUG=$(DEBUG) SYSCFLAGS="-DUNIX -fPIC" SYSLIBS=""
