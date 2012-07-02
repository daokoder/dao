
include ../Makefile.common

all:
	-cd auxlib && $(MAKE)
	-cd syslib && $(MAKE)
	-cd math && $(MAKE)
	-cd meta && $(MAKE)
	-cd help && $(MAKE)
	-cd serializer && $(MAKE)
	-$(HAS_FILE) Makefile2 && $(MAKE) -f Makefile2

clean:
	-cd auxlib && $(MAKE) clean
	-cd syslib && $(MAKE) clean
	-cd math && $(MAKE) clean
	-cd meta && $(MAKE) clean
	-cd help && $(MAKE) clean
	-cd serializer && $(MAKE) clean
	-$(HAS_FILE) Makefile2 && $(MAKE) -f Makefile2 clean

install:
	-$(COPY) addpath.dao $(DAO_MOD_DIR)
	-$(COPY) auxlib/libdao_aux.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) syslib/libdao_sys.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) math/math.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) meta/meta.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) serializer/libdao_aux.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(HAS_FILE) Makefile2 && $(MAKE) -f Makefile2 install

