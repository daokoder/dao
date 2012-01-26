option(DAO_MODULES_DAOCXX "[INCOMPLETE] Build the \"DaoCXX\" module which allows embedding/inlining C/C++ code into Dao code." OFF)

if(DAO_MODULES_DAOCXX)
    # Taken straight out of the LLVM documentation.
    set(LLVM_ROOT "/usr" CACHE PATH "Root of LLVM install.")
    if(NOT EXISTS ${LLVM_ROOT}/include/llvm)
        message(FATAL_ERROR "LLVM_ROOT (${LLVM_ROOT}) is not a valid LLVM install")
    endif()
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_ROOT}/share/llvm/cmake")
    include(LLVMConfig)
    include_directories( ${LLVM_INCLUDE_DIRS} )
    link_directories( ${LLVM_LIBRARY_DIRS} )
    add_definitions( ${LLVM_DEFINITIONS} -fno-rtti )

    # Finally, we link the CLang libraries to our executable. No need for LLVM libs:
    add_library(DaoCXX SHARED daoCXX.cpp)
    target_link_libraries(DaoCXX ${DAOLIB_TO_LINK} clang clangFrontend clangDriver clangCodeGen clangSema clangAnalysis clangAST clangParse clangLex clangBasic clangIndex clangSerialization)

    install(TARGETS DaoCXX DESTINATION ${DAO_MODULES_INSTALL_DIR})
endif(DAO_MODULES_DAOCXX)
