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
    ${Boost_LIBRARIES}
    ${IBVERBS_LIBRARIES}
    ${CUDA_LIBRARIES}
)
