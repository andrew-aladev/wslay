set (INCLUDES event.h frame.h queue.h wslay.h context.h utf8.h)
set (SOURCES  event.c frame.c queue.c context.c utf8.c)

if (WSLAY_SHARED MATCHES true)
    add_library (${WSLAY_TARGET} SHARED ${SOURCES})
    target_link_libraries (${WSLAY_TARGET} ${TALLOC_TARGET})
endif ()

if (WSLAY_STATIC MATCHES true)
    add_library (${WSLAY_TARGET}_static STATIC ${SOURCES})
    target_link_libraries (${WSLAY_TARGET}_static ${TALLOC_TARGET}_static)
    set_target_properties (${WSLAY_TARGET}_static PROPERTIES OUTPUT_NAME ${WSLAY_TARGET})
endif ()