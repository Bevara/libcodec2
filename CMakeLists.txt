cmake_minimum_required(VERSION 3.7)
project(Filters)

include(filters.cmake)

add_definitions(-fpic)

SET(LIBCODEC2_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/dec_codec2.c
        ${CMAKE_CURRENT_SOURCE_DIR}/codec2_stubs.c
)

SET(LIBCODEC2_LIBS
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libcodec2.a
)

SET(LIBCODEC2_INC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_filter(libcodec2
        "${LIBCODEC2_SRC}"
        "${LIBCODEC2_LIBS}"
        []
        ""
        "${LIBCODEC2_INC}"
        ""
        "1")
