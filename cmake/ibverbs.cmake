IF (IBVERBS_INCLUDE_DIR)
    SET(IBVERBS_INC_DIR ${IBVERBS_INCLUDE_DIR})
    UNSET(IBVERBS_INCLUDE_DIR)
ENDIF (IBVERBS_INCLUDE_DIR)

FIND_PATH(IBVERBS_INCLUDE_DIR infiniband/verbs.h
    PATHS ${IBVERBS_INC_DIR}
    ${IBVERBS_INSTALL_DIR}/include
    /usr/local/include
    /usr/include )
message("Found ${IBVERBS_INCLUDE_DIR} : ${IBVERBS_INSTALL_DIR}")

SET(IBVERBS_NAMES ibverbs rdmacm)
FOREACH( lib ${IBVERBS_NAMES} )
    FIND_LIBRARY(IBVERBS_LIBRARY_${lib}
        NAMES ${lib}
        PATHS ${IBVERBS_LIBRARY_DIR} ${IBVERBS_INSTALL_DIR} ${IBVERBS_INSTALL_DIR}/lib /usr/local/lib /usr/lib
        )
    LIST(APPEND IBVERBS_LIBRARIES ${IBVERBS_LIBRARY_${lib}})
ENDFOREACH(lib)

    # handle the QUIETLY and REQUIRED arguments and set IBVERBS_FOUND to TRUE if.
    # all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(IBVERBS DEFAULT_MSG IBVERBS_LIBRARIES IBVERBS_INCLUDE_DIR)

IF(NOT IBVERBS_FOUND)
    SET( IBVERBS_LIBRARIES )
    SET( IBVERBS_TEST_LIBRARIES )
ELSE(NOT IBVERBS_FOUND)
    # -- add dependecies
    LIST(APPEND IBVERBS_LIBRARIES ${CUDA_curand_LIBRARY})
ENDIF(NOT IBVERBS_FOUND)

LIST(APPEND IBVERBS_INCLUDE_DIR "${IBVERBS_INCLUDE_DIR}")

MARK_AS_ADVANCED(IBVERBS_LIBRARIES IBVERBS_TEST_LIBRARIES IBVERBS_INCLUDE_DIR)
