option(DAO_MODULES_MATH "Build the \"math\" module which adds mathematical functions and constants to Dao." ON)

if(DAO_MODULES_MATH)
    add_library(dao_math SHARED dao_math.c)
    target_link_libraries(dao_math ${DAOLIB_TO_LINK})
    install(TARGETS dao_math DESTINATION ${DAO_MODULES_INSTALL_DIR})
endif(DAO_MODULES_MATH)
