set (INCLUDES event.h frame.h net.h queue.h stack.h wslay.h)
set (SOURCES  event.c frame.c net.c queue.c stack.c)

if (WSLAY_SHARED MATCHES true)
    add_library (${PROJECT_NAME} SHARED ${SOURCES})
endif ()

if (WSLAY_STATIC MATCHES true)
    add_library (${PROJECT_NAME}_static STATIC ${SOURCES})
    set_target_properties (${PROJECT_NAME}_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
endif ()