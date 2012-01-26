option(DAO_MODULES_STREAM_CLPRINT "Build the \"clprint\" module which adds colored console output to Dao." ON)

if(DAO_MODULES_STREAM_CLPRINT)
    add_library(dao_clprint SHARED clprint/dao_clprint.c)
    target_link_libraries(dao_clprint ${DAOLIB_TO_LINK})
    install(TARGETS dao_clprint DESTINATION "${DAO_MODULES_INSTALL_DIR}/stream")
endif(DAO_MODULES_STREAM_CLPRINT)

# Enable the stream buffer module only when the sys module is built, as that one contains the buffer.
INCLUDE(CMakeDependentOption)
CMAKE_DEPENDENT_OPTION(DAO_MODULES_STREAM_BUFFER "Build the \"buffer\" module which adds buffer support to Dao streams. Needs the \"sys\" module." ON "DAO_MODULES_SYS" OFF)

if(DAO_MODULES_STREAM_BUFFER)
    add_library(dao_buffer SHARED buffer/dao_buffer.c)
    include_directories("${PROJECT_SOURCE_DIR}/modules/syslib")
    target_link_libraries(dao_buffer ${DAOLIB_TO_LINK} dao_sys)
    install(TARGETS dao_buffer DESTINATION "${DAO_MODULES_INSTALL_DIR}/stream")
endif(DAO_MODULES_STREAM_BUFFER)

# Also add the "stream" module containing all of these.
if(DAO_MODULES_STREAM_CLPRINT OR DAO_MODULES_STREAM_BUFFER)
    install(FILES "stream.dao" DESTINATION "${DAO_MODULES_INSTALL_DIR}/stream")
endif()
