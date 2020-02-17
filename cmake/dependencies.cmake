include(compiler_settings)
include(cmake/boost.cmake)
include(cmake/spead2.cmake)
include(cmake/psrdada.cmake)
include(cmake/psrdada_cpp.cmake)
include(cmake/ibverbs.cmake)
include(cmake/cuda.cmake)
include_directories(SYSTEM ${CUDA_INCLUDE_DIR} ${IBVERBS_INCLUDE_DIR} ${SPEAD2_INCLUDE_DIR} ${PSRDADA_CPP_INCLUDE_DIR} ${PSRDADA_INCLUDE_DIR} ${Boost_INCLUDE_DIR})

set(DEPENDENCY_LIBRARIES
    ${PSRDADA_CPP_LIBRARIES}
    ${PSRDADA_LIBRARIES}
    ${SPEAD2_LIBRARIES}
    ${IBVERBS_LIBRARIES}
    ${CUDA_LIBRARIES}
    ${Boost_LIBRARIES}
)
set(DEPENDENCY_LIBRARIES_RNT
    ${PSRDADA_LIBRARIES}
    ${SPEAD2_LIBRARIES}
    ${IBVERBS_LIBRARIES}
    ${CUDA_LIBRARIES}
    ${Boost_LIBRARIES}
)

find_package(OpenMP REQUIRED)
if(OPENMP_FOUND)
    message(STATUS "Found OpenMP" )
    set(DEPENDENCY_LIBRARIES ${DEPENDENCY_LIBRARIES} ${OpenMP_EXE_LINKER_FLAGS})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

