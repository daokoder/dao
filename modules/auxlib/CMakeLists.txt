option(DAO_MODULES_AUXLIB "Build the \"aux\" module which adds serialization, hibernation and logging to Dao." ON)

if(DAO_MODULES_AUXLIB)
    add_library(dao_aux SHARED dao_aux.c dao_aux.h)
    target_link_libraries(dao_aux ${DAOLIB_TO_LINK})
    install(TARGETS dao_aux DESTINATION ${DAO_MODULES_INSTALL_DIR})
endif(DAO_MODULES_AUXLIB)
