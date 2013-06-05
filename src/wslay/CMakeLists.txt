set (INCLUDES event.h frame.h queue.h wslay.h context.h utf8.h)
set (SOURCES  event.c frame.c queue.c context.c utf8.c)

if (WSLAY_SHARED MATCHES true)
    add_library (${PROJECT_NAME} SHARED ${SOURCES})
    target_link_libraries (${PROJECT_NAME} talloc2)
endif ()

if (WSLAY_STATIC MATCHES true)
    add_library (${PROJECT_NAME}_static STATIC ${SOURCES})
    target_link_libraries (${PROJECT_NAME}_static talloc2_static)
    set_target_properties (${PROJECT_NAME}_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
endif ()