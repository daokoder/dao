option(DAO_MODULES_GRAPH "Build the \"graph\" module which adds the graph datastructure to Dao." ON)

if(DAO_MODULES_GRAPH)
    add_library(dao_graph SHARED dao_graph.c dao_graph.h)
    target_link_libraries(dao_graph ${DAOLIB_TO_LINK})
    install(TARGETS dao_graph DESTINATION ${DAO_MODULES_INSTALL_DIR})
endif(DAO_MODULES_GRAPH)
