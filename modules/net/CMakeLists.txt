option(DAO_MODULES_NETWORKING "Build the \"net\" module which adds low- and highlevel networking to Dao." ON)

if(DAO_MODULES_NETWORKING)
    add_library(dao_net SHARED dao_network.c)
    target_link_libraries(dao_net ${DAOLIB_TO_LINK})
    install(TARGETS dao_net DESTINATION ${DAO_MODULES_INSTALL_DIR})
endif(DAO_MODULES_NETWORKING)
