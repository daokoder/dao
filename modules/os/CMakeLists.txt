option(DAO_MODULES_OS_FS "Build the \"fs\" module which adds file and directory handling to Dao." ON)

if(DAO_MODULES_OS_FS)
    add_library(dao_fs SHARED fs/dao_fs.c)
    target_link_libraries(dao_fs ${DAOLIB_TO_LINK})
    install(TARGETS dao_fs DESTINATION ${DAO_MODULES_INSTALL_DIR})
endif(DAO_MODULES_OS_FS)
