set (SOURCES main.c event.c frame.c queue.c)

if (WSLAY_SHARED MATCHES true)
    add_executable (main ${SOURCES})
    target_link_libraries (main ${PROJECT_NAME} cunit)
    add_test (${PROJECT_NAME}-main main)
endif ()

if (WSLAY_STATIC MATCHES true)
add_executable (main_static ${SOURCES})
    target_link_libraries (main_static ${PROJECT_NAME}_static cunit)
    add_test (${PROJECT_NAME}-static-main main_static)
endif ()