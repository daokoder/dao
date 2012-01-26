option(DAO_MODULES_JIT "[BROKEN] Build the JIT compiler module. Requires LLVM. NOT used by the default interpreter." OFF)

if(DAO_MODULES_JIT)
    # Taken straight out of the LLVM documentation.
    set(LLVM_ROOT "/usr" CACHE PATH "Root of LLVM install.")
    if(NOT EXISTS ${LLVM_ROOT}/include/llvm)
        message(FATAL_ERROR "LLVM_ROOT (${LLVM_ROOT}) is not a valid LLVM install")
    endif()
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${LLVM_ROOT}/share/llvm/cmake")
    include(LLVMConfig)
    include_directories( ${LLVM_INCLUDE_DIRS} )
    link_directories( ${LLVM_LIBRARY_DIRS} )
    add_definitions( ${LLVM_DEFINITIONS} )
    llvm_map_components_to_libraries(REQ_LLVM_LIBRARIES jit native)

    # Finally, we link the LLVM libraries to our executable:
    add_library(DaoJIT SHARED daoJIT.cpp daoJIT.h)
    target_link_libraries(DaoJIT ${REQ_LLVM_LIBRARIES})
    add_executable(DaoJITTest test.cpp)
    target_link_libraries(DaoJITTest DaoJIT)
endif(DAO_MODULES_JIT)
