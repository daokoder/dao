option(DAO_MODULES_META "Build the \"meta\" module which adds reflection to Dao." ON)

if(DAO_MODULES_META)
    add_library(dao_meta SHARED dao_meta.c)
    target_link_libraries(dao_meta ${DAOLIB_TO_LINK})
    install(TARGETS dao_meta DESTINATION ${DAO_MODULES_INSTALL_DIR})
endif(DAO_MODULES_META)
