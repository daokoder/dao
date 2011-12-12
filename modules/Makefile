
include ../Makefile.common

all:
	-cd auxlib && $(MAKE)
	-cd syslib && $(MAKE)
	-cd math && $(MAKE)
	-cd string && $(MAKE)
	-cd meta && $(MAKE)
	-cd net && $(MAKE)
	-cd web && $(MAKE)
	-cd os && $(MAKE)
	-cd stream && $(MAKE)
#	-cd DaoCXX && $(MAKE)
#	-cd DaoJIT && $(MAKE)

clean:
	-cd auxlib && $(MAKE) clean
	-cd syslib && $(MAKE) clean
	-cd math && $(MAKE) clean
	-cd string && $(MAKE) clean
	-cd meta && $(MAKE) clean
	-cd net && $(MAKE) clean
	-cd web && $(MAKE) clean
	-cd os && $(MAKE) clean
	-cd stream && $(MAKE) clean
#	-cd DaoCXX && $(MAKE) clean
#	-cd DaoJIT && $(MAKE) clean

install-unix:
	@$(HAS_DIR) $(DAO_MOD_DIR)/string || $(MKDIR) $(DAO_MOD_DIR)/string
	@$(HAS_DIR) $(DAO_MOD_DIR)/web || $(MKDIR) $(DAO_MOD_DIR)/web
	@$(HAS_DIR) $(DAO_MOD_DIR)/os || $(MKDIR) $(DAO_MOD_DIR)/os
	@$(HAS_DIR) $(DAO_MOD_DIR)/stream || $(MKDIR) $(DAO_MOD_DIR)/stream
	-$(COPY) addpath.dao $(DAO_MOD_DIR)
	-$(COPY) auxlib/libdao_aux.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) syslib/libdao_sys.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) math/math.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) meta/meta.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) net/net.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) string/string.dao $(DAO_MOD_DIR)/string
	-$(COPY) string/format.$(DAO_DLL_EXT) $(DAO_MOD_DIR)/string
	-$(COPY) string/md5.$(DAO_DLL_EXT) $(DAO_MOD_DIR)/string
	-$(COPY) string/tokenize.$(DAO_DLL_EXT) $(DAO_MOD_DIR)/string
	-$(COPY) string/xxtea.$(DAO_DLL_EXT) $(DAO_MOD_DIR)/string
	-$(COPY) stream/stream.dao $(DAO_MOD_DIR)/stream
	-$(COPY) stream/clprint.$(DAO_DLL_EXT) $(DAO_MOD_DIR)/stream
	-$(COPY) web/cgi.$(DAO_DLL_EXT) $(DAO_MOD_DIR)/web
	-$(COPY) web/json.$(DAO_DLL_EXT) $(DAO_MOD_DIR)/web
	-$(COPY) os/fsnode.$(DAO_DLL_EXT) $(DAO_MOD_DIR)/os

install-win32:
	@$(HAS_DIR) $(DAO_MOD_DIR)\string || $(MKDIR) $(DAO_MOD_DIR)\string
	@$(HAS_DIR) $(DAO_MOD_DIR)\web || $(MKDIR) $(DAO_MOD_DIR)\web
	@$(HAS_DIR) $(DAO_MOD_DIR)\os || $(MKDIR) $(DAO_MOD_DIR)\os
	@$(HAS_DIR) $(DAO_MOD_DIR)\stream || $(MKDIR) $(DAO_MOD_DIR)\stream
	-$(COPY) addpath.dao $(DAO_MOD_DIR)
	-$(COPY) auxlib\libdao_aux.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) syslib\libdao_sys.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) math\math.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) meta\meta.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) net\net.$(DAO_DLL_EXT) $(DAO_MOD_DIR)
	-$(COPY) string\string.dao $(DAO_MOD_DIR)\string
	-$(COPY) string\format.$(DAO_DLL_EXT) $(DAO_MOD_DIR)\string
	-$(COPY) string\md5.$(DAO_DLL_EXT) $(DAO_MOD_DIR)\string
	-$(COPY) string\tokenize.$(DAO_DLL_EXT) $(DAO_MOD_DIR)\string
	-$(COPY) string\xxtea.$(DAO_DLL_EXT) $(DAO_MOD_DIR)\string
	-$(COPY) stream\stream.dao $(DAO_MOD_DIR)\stream
	-$(COPY) stream\clprint.$(DAO_DLL_EXT) $(DAO_MOD_DIR)\stream
	-$(COPY) web\cgi.$(DAO_DLL_EXT) $(DAO_MOD_DIR)\web
	-$(COPY) web\json.$(DAO_DLL_EXT) $(DAO_MOD_DIR)\web
	-$(COPY) os\fsnode.$(DAO_DLL_EXT) $(DAO_MOD_DIR)\os
