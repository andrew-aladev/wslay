set (SOURCES main.c event.c frame.c queue.c)

if (WSLAY_SHARED MATCHES true)
    add_executable (main ${SOURCES})
    target_link_libraries (main ${WSLAY_TARGET} cunit)
    add_test (${WSLAY_TARGET}-main main)
endif ()

if (WSLAY_STATIC MATCHES true)
add_executable (main_static ${SOURCES})
    target_link_libraries (main_static ${WSLAY_TARGET}_static cunit)
    add_test (${WSLAY_TARGET}-static-main main_static)
endif ()