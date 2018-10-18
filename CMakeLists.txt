# project name and specify the minumum cmake version.
project (mkrecv CXX)
cmake_minimum_required (VERSION 2.8)

# @mhein3:
# cmake -DCMAKE_BUILD_TYPE=DEBUG -DCMAKE_INSTALL_PREFIX=~ .
# cmake -D CMAKE_INSTALL_PREFIX=~ .

# The version number.
set (mkrecv_VERSION_MAJOR 0)
set (mkrecv_VERSION_MINOR 1)

# cmake setup.
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_SOURCE_DIR}/cmake)

# default installtion locations (note these will be prefixed by ${CMAKE_INSTALL_PREFIX})
if(NOT LIBRARY_INSTALL_DIR)
    set(LIBRARY_INSTALL_DIR "lib")
endif(NOT LIBRARY_INSTALL_DIR)

if(NOT INCLUDE_INSTALL_DIR)
    set(INCLUDE_INSTALL_DIR "include/${CMAKE_PROJECT_NAME}")
endif(NOT INCLUDE_INSTALL_DIR)

if(NOT MODULES_INSTALL_DIR)
    set (MODULES_INSTALL_DIR "share/${CMAKE_PROJECT_NAME}")
endif(NOT MODULES_INSTALL_DIR)

if(NOT BINARY_INSTALL_DIR)
    set(BINARY_INSTALL_DIR "bin/")
endif(NOT BINARY_INSTALL_DIR)

# get project dependencies and compiler settings.
include(dependencies)

# define the cheetah libraries
set(MKRECV_LIBRARIES ${DEPENDENCY_LIBRARIES})

include_directories(..)
include_directories(${PROJECT_BINARY_DIR})

set(mkrecv_src
  mkrecv_receiver.cpp
  mkrecv_options.cpp
  mkrecv_allocator.cpp
  mkrecv_destination.cpp
  mkrecv_stream.cpp
)

set(mkrecv_src_nt
  mkrecv_receiver_nt.cpp
  mkrecv_options.cpp
  mkrecv_storage.cpp
  mkrecv_storage_null.cpp
  mkrecv_storage_full_dada.cpp
  mkrecv_allocator.cpp
  mkrecv_allocator_nt.cpp
  mkrecv_destination.cpp
  mkrecv_stream_nt.cpp
)

#set(mkrecv_inc
#)

# === Print build options summary.
set(DEBUG_PRINT ON)
if (DEBUG_PRINT)
    message(STATUS "")
    message(STATUS "****************************************************************************")
    message(STATUS " name: ${CMAKE_PROJECT_NAME} version: ${PROJECT_VERSION}")
    message(STATUS "")
    message(STATUS "")
    message(STATUS " Compiler Options:")
    message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER}")
    if (${CMAKE_BUILD_TYPE} MATCHES release)
        message(STATUS "  C++ flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
    elseif (${CMAKE_BUILD_TYPE} MATCHES debug)
        message(STATUS "  C++ flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")
    elseif (${CMAKE_BUILD_TYPE} MATCHES relwithdebinfo)
        message(STATUS "  C++ flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
    elseif (${CMAKE_BUILD_TYPE} MATCHES minsizerel)
        message(STATUS "  C++ flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_MINSIZEREL}")
    endif ()
    message(STATUS "")
    message(STATUS "Install locations: (make install)")
    message(STATUS "  Libraries: ${CMAKE_INSTALL_PREFIX}/lib")
    message(STATUS "  Inclues:   ${CMAKE_INSTALL_PREFIX}/${INCLUDE_INSTALL_DIR}")
    message(STATUS "  Binaries:  ${CMAKE_INSTALL_PREFIX}/${BINARY_INSTALL_DIR}")
    if(ENABLE_DOC)
        message(STATUS "  Doc:       ${CMAKE_INSTALL_PREFIX}/${DOC_INSTALL_DIR}")
    else(ENABLE_DOC)
        message(STATUS "  Doc:       Disabled (-DENABLE_DOC=true to enable)")
    endif(ENABLE_DOC)
    message(STATUS "  Other:     ${CMAKE_INSTALL_PREFIX}/${MODULES_INSTALL_DIR}")
    message(STATUS "****************************************************************************")
    message(STATUS "")
endif (DEBUG_PRINT)

#mkrecv
add_executable(mkrecv mkrecv_main.cpp ${mkrecv_src})
target_link_libraries(mkrecv ${MKRECV_LIBRARIES})

add_executable(mkrecv_nt mkrecv_main_nt.cpp ${mkrecv_src_nt})
target_link_libraries(mkrecv_nt ${MKRECV_LIBRARIES})

add_executable(recv_heaps recv_heaps.cpp)

add_executable(decode_heap decode_heap.cpp)

install (TARGETS mkrecv mkrecv_nt recv_heaps decode_heap
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
#install (FILES MathFunctions.h DESTINATION include)

install(FILES ${mkrecv_inc} DESTINATION include/mkrecv)
