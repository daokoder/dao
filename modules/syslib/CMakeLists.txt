option(DAO_MODULES_SYS "Build the \"sys\" module which adds buffers and low-level OS interaction to Dao." ON)

if(DAO_MODULES_SYS)
    add_library(dao_sys SHARED dao_sys.c dao_sys.h)
    target_link_libraries(dao_sys ${DAOLIB_TO_LINK})
    install(TARGETS dao_sys DESTINATION ${DAO_MODULES_INSTALL_DIR})
endif(DAO_MODULES_SYS)
