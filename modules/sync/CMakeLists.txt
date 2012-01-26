option(DAO_MODULES_SYNC "Build the \"sync\" module which adds threadsafe datastructures to Dao." ON)

if(DAO_MODULES_SYNC)
    add_library(dao_sync SHARED dao_sync.c)
    target_link_libraries(dao_sync ${DAOLIB_TO_LINK})
    install(TARGETS dao_sync DESTINATION ${DAO_MODULES_INSTALL_DIR})
endif(DAO_MODULES_SYNC)
