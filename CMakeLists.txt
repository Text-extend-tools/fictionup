project(fictionup C)
cmake_minimum_required(VERSION 2.8.11)

include(GNUInstallDirs)

find_library(YAML yaml)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(minizip)

# Workaround for Gentoo patched zlib
include(CheckSymbolExists)
check_symbol_exists(OF zlib.h ZLIB_HAS_OF)
if(NOT ZLIB_HAS_OF)
    check_symbol_exists(_Z_OF zlib.h ZLIB_HAS_ZOF)
    if(ZLIB_HAS_ZOF)
        add_definitions(-DOF=_Z_OF)
    else()
        add_definitions("'-DOF(args)=args'")
    endif()
endif()

add_library(minizip STATIC
    minizip/zip.c
    minizip/ioapi.c
    )
target_compile_definitions(minizip PRIVATE NOCRYPT)

include_directories(hoedown/src)

add_library(hoedown STATIC
    hoedown/src/buffer.c
    hoedown/src/document.c
    hoedown/src/stack.c
    hoedown/src/autolink.c
    hoedown/src/html_blocks.c
    )

include_directories(libb64/include)

add_library(b64 STATIC
    libb64/src/cencode.c
    )

include_directories(uthash/src)

add_executable(${PROJECT_NAME}
    src/main.c
    src/fb2.c
    src/meta_subst.c
    src/meta.c
    src/parse_yaml.c
    src/options.c
    src/utils.c
    src/parse_meta_lang.c
    src/write_zip.c
    )

target_link_libraries(${PROJECT_NAME} hoedown b64 ${YAML} minizip ${ZLIB_LIBRARIES})

if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic -Wall -Wextra")
endif(CMAKE_COMPILER_IS_GNUCC)

add_custom_command(
    OUTPUT ${PROJECT_NAME}.1.gz
    COMMAND gzip -c -9 "${CMAKE_SOURCE_DIR}/doc/${PROJECT_NAME}.1" > ${PROJECT_NAME}.1.gz
    DEPENDS "${CMAKE_SOURCE_DIR}/doc/${PROJECT_NAME}.1"
    )

add_custom_target(man ALL DEPENDS ${PROJECT_NAME}.1.gz)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.1.gz" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
install(FILES
    "${CMAKE_SOURCE_DIR}/doc/fictionup-example-en.md"
    "${CMAKE_SOURCE_DIR}/doc/fictionup-example-ru.md"
    "${CMAKE_SOURCE_DIR}/doc/fictionup.png"
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    )
install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
    )
