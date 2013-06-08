set (SOURCES main.c event.c frame.c queue.c)

if (WSLAY_SHARED MATCHES true)
    add_executable (${WSLAY_TARGET}-main ${SOURCES})
    target_link_libraries (${WSLAY_TARGET}-main ${WSLAY_TARGET} cunit)
    add_test (${WSLAY_TARGET}-main ${WSLAY_TARGET}-main)
endif ()

if (WSLAY_STATIC MATCHES true)
add_executable (${WSLAY_TARGET}-static-main ${SOURCES})
    target_link_libraries (${WSLAY_TARGET}-static-main ${WSLAY_TARGET}_static cunit)
    add_test (${WSLAY_TARGET}-static-main ${WSLAY_TARGET}-static-main)
endif ()