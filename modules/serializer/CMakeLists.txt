option(DAO_MODULES_AUXLIB "Build the \"serializer\" module which adds serialization to Dao." ON)

if(DAO_MODULES_AUXLIB)
    add_library(dao_serializer SHARED dao_serializer.c dao_serializer.h)
    target_link_libraries(dao_serializer ${DAOLIB_TO_LINK})
    install(TARGETS dao_serializer DESTINATION ${DAO_MODULES_INSTALL_DIR})
endif(DAO_MODULES_AUXLIB)
