cmake_minimum_required(VERSION 2.6)

if( "${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}" )
	MESSAGE(FATAL_ERROR "\nDirectory ${CMAKE_CURRENT_SOURCE_DIR} already contains other Makefiles.\n"
			"Please remove CMakeCache.txt and CMakeFiles, and use a different folder to make "
			"an out-of-source build!")
endif()

project(dao)

set(DAO_SYSTEM_LIBS "")

# Declare all options visible to the user.
set(DAO_LIB_NAME dao CACHE STRING "The name of the generated libraries (without prefix and extension)")
set(DAO_EXE_NAME dao CACHE STRING "The name of the generated interpreter (without extension)")
option(DAO_STATIC_INTERPRETER "Statically link the dao interpreter to the dao libraries?" ON)
option(DAO_WITH_NUMARRAY "Add support for numeric array operations." ON)
option(DAO_WITH_THREAD "Add support for multithreading primitives." ON)
option(DAO_WITH_CONCURRENT "Add support for higher level concurrency." ON)
option(DAO_WITH_DYNCLASS "Add support for dynamic classes." ON)
option(DAO_WITH_DECORATOR "Add support for decorators." ON)
option(DAO_WITH_MACRO "Add support for synatx macros." ON)

set(DAO_KERNEL_HEADERS "${PROJECT_SOURCE_DIR}/kernel/dao.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoArray.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoBase.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoClass.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoConst.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoGC.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoLexer.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoMacro.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoMap.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoNamespace.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoNumtype.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoObject.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoParser.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoBytecode.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoOptimizer.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoProcess.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoRegex.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoRoutine.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoTasklet.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoStdlib.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoStdtype.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoStream.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoString.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoThread.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoType.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoValue.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoVmcode.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoVmspace.h"
                       "${PROJECT_SOURCE_DIR}/kernel/daoPlatforms.h"
   )
set(DAO_KERNEL_SOURCES "${PROJECT_SOURCE_DIR}/kernel/daoArray.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoClass.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoConst.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoGC.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoLexer.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoMacro.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoMap.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoNamespace.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoNumtype.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoObject.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoParser.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoBytecode.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoOptimizer.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoProcess.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoRegex.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoRoutine.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoTasklet.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoStdlib.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoStdtype.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoStream.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoString.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoThread.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoType.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoValue.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoVmcode.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoVmspace.c"
                       "${PROJECT_SOURCE_DIR}/kernel/daoPlatforms.c"
   )

# Check whether the system has fenv.h or not.
find_path(DAO_HAS_FENV "fenv.h")
if(DAO_HAS_FENV)
    include_directories("${DAO_HAS_FENV}")
else(DAO_HAS_FENV)
    add_definitions(-DNO_FENV)
endif(DAO_HAS_FENV)

# Check whether the system has readline.h or not.
find_path(DAO_HAS_READLINE NAMES "readline/readline.h" "readline/history.h")
if(DAO_HAS_READLINE)
    include_directories("${DAO_HAS_READLINE}")
    add_definitions(-DDAO_USE_READLINE)
    list(APPEND DAO_SYSTEM_LIBS "readline")
endif(DAO_HAS_READLINE)

# Setup for Minix:
if( "${CMAKE_SYSTEM_NAME}" STREQUAL "Minix" )
	set( UNIX 1 )
	set( DAO_WITH_THREAD 0 )
	set( DAO_WITH_CONCURRENT 0 )
	MESSAGE(WARNING "Multithreading disable for Minix")
endif()

# Enable or disable compile-time language features
if(DAO_WITH_NUMARRAY)
    add_definitions(-DDAO_WITH_NUMARRAY)
endif(DAO_WITH_NUMARRAY)

if(DAO_WITH_THREAD)
    add_definitions(-DDAO_WITH_THREAD)
endif(DAO_WITH_THREAD)

if(DAO_WITH_CONCURRENT)
    add_definitions(-DDAO_WITH_CONCURRENT)
endif(DAO_WITH_CONCURRENT)

if(DAO_WITH_DYNCLASS)
    add_definitions(-DDAO_WITH_DYNCLASS)
endif(DAO_WITH_DYNCLASS)

if(DAO_WITH_DECORATOR)
    add_definitions(-DDAO_WITH_DECORATOR)
endif(DAO_WITH_DECORATOR)

if(DAO_WITH_MACRO)
    add_definitions(-DDAO_WITH_MACRO)
endif(DAO_WITH_MACRO)

# System dependent settings
if(UNIX)
    add_definitions(-DUNIX)
	SET( CMAKE_EXE_LINKER_FLAGS "-Wl,-export-dynamic" )
	SET( CMAKE_SHARED_LINKER_FLAGS "-Wl,-export-dynamic" )
    list(APPEND DAO_SYSTEM_LIBS "m" )

    if(DAO_WITH_THREAD OR DAO_WITH_CONCURRENT)
        list(APPEND DAO_SYSTEM_LIBS "pthread")
    endif(DAO_WITH_THREAD OR DAO_WITH_CONCURRENT)

	if( "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux" )
    	list(APPEND DAO_SYSTEM_LIBS "dl" )
	endif()
elseif(WIN32)
    add_definitions(-DWIN32)

    # Dao threading needs timeGetTime on windows which is in winmm.lib
    if(DAO_WITH_THREAD OR DAO_WITH_CONCURRENT)
        list(APPEND DAO_SYSTEM_LIBS "winmm.lib")
    endif(DAO_WITH_THREAD OR DAO_WITH_CONCURRENT)
endif(UNIX)

if(APPLE)
	set( CMAKE_EXE_LINKER_FLAGS "" )
	set( CMAKE_SHARED_LINKER_FLAGS "-undefined dynamic_lookup" )
	if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		add_definitions(-DMAC_OSX)
	endif()
endif()

include_directories("${PROJECT_SOURCE_DIR}/kernel")

# The static dao lib
add_library(daolibs STATIC ${DAO_KERNEL_SOURCES} ${DAO_KERNEL_HEADERS})
set_target_properties(daolibs PROPERTIES
                        COMPILE_DEFINITIONS DAO_STATIC
                        OUTPUT_NAME ${DAO_LIB_NAME}
                        PRIVATE_HEADER "${DAO_KERNEL_HEADERS}"
                     )

# The dynamic dao lib
add_library(daolib SHARED ${DAO_KERNEL_SOURCES} ${DAO_KERNEL_HEADERS})
set_target_properties(daolib PROPERTIES
                        COMPILE_DEFINITIONS DAO_EXPORT
                        OUTPUT_NAME ${DAO_LIB_NAME}
                        PRIVATE_HEADER "${DAO_KERNEL_HEADERS}"
                     )
target_link_libraries(daolib ${DAO_SYSTEM_LIBS})

# Setup either static or dynamic compiling of the dao executable.
# Note: the variable DAOLIB_TO_LINK is what MODULES link against.
add_executable(daoexe "${PROJECT_SOURCE_DIR}/kernel/daoMain.c")
if(DAO_STATIC_INTERPRETER)
    set(DAOLIB_TO_LINK "")
    target_link_libraries(daoexe daolibs ${DAO_SYSTEM_LIBS})
else(DAO_STATIC_INTERPRETER)
    set(DAOLIB_TO_LINK daolib)
    target_link_libraries(daoexe ${DAOLIB_TO_LINK})
endif(DAO_STATIC_INTERPRETER)
# target_link_libraries(daoexe ${DAOLIB_TO_LINK})

set_target_properties(daoexe PROPERTIES OUTPUT_NAME ${DAO_EXE_NAME})

# Installation rules for the interpreter and dao libs
set(DAO_MODULES_INSTALL_DIR "lib/dao/modules")
install(TARGETS daoexe daolib daolibs
        RUNTIME DESTINATION "bin"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib"
        PRIVATE_HEADER DESTINATION "include"
        PUBLIC_HEADER  DESTINATION "include"
       )
#autobind.dao is obsolete:
#install(FILES "${PROJECT_SOURCE_DIR}/tools/autobind.dao" DESTINATION "bin")
install(FILES "${PROJECT_SOURCE_DIR}/license.txt" DESTINATION "share/doc/dao")
install(FILES "${PROJECT_SOURCE_DIR}/modules/addpath.dao" DESTINATION ${DAO_MODULES_INSTALL_DIR})
# TODO: dao.conf? demos?

# Now, find all modules and add options for them.
add_subdirectory("${PROJECT_SOURCE_DIR}/modules/auxlib")
add_subdirectory("${PROJECT_SOURCE_DIR}/modules/syslib")
add_subdirectory("${PROJECT_SOURCE_DIR}/modules/math")
add_subdirectory("${PROJECT_SOURCE_DIR}/modules/meta")
add_subdirectory("${PROJECT_SOURCE_DIR}/modules/serializer")


set( OPTIONAL_MODULES
		"DaoJIT"
		"DaoCXX"
		"graph"
		"net"
		"os"
		"stream"
		"string"
		"sync"
		"graphics"
		"web"
	)

foreach( module ${OPTIONAL_MODULES} )
	set( module_path "${PROJECT_SOURCE_DIR}/modules/${module}" )
	if( EXISTS "${module_path}" )
		add_subdirectory( "${module_path}" )
	endif()
endforeach(module)

