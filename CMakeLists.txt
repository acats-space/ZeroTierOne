# CMake build script for libzerotiercore.a

cmake_minimum_required (VERSION 2.8)
project (zerotiercore)

set (PROJ_DIR ${PROJECT_SOURCE_DIR})
set (ZT_DEFS -std=c++11)

file(GLOB core_src_glob ${PROJ_DIR}/node/*.cpp)
add_library(zerotiercore STATIC ${core_src_glob})

target_compile_options(zerotiercore PRIVATE ${ZT_DEFS})

# Build the Rust embedding tool
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/ci/scripts/embed_json
    COMMAND rustc ${CMAKE_CURRENT_SOURCE_DIR}/ci/scripts/embed_json.rs -o ${CMAKE_CURRENT_SOURCE_DIR}/ci/scripts/embed_json
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ci/scripts/embed_json.rs
    COMMENT "Building Rust JSON embedding tool"
)

# Embed diagnostic_schema.json as a C string
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/diagnostic/diagnostic_schema_embed.c
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/diagnostic
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ci/scripts/embed_json ${CMAKE_CURRENT_SOURCE_DIR}/../diagnostic/diagnostic_schema.json ${CMAKE_CURRENT_SOURCE_DIR}/diagnostic/diagnostic_schema_embed.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../diagnostic/diagnostic_schema.json ${CMAKE_CURRENT_SOURCE_DIR}/ci/scripts/embed_json
    COMMENT "Embedding diagnostic_schema.json as C string"
)

set(DIAGNOSTIC_SCHEMA_EMBED_SRC
    diagnostic/diagnostic_schema_embed.c
    diagnostic/diagnostic_schema_embed.h
)

# Add the generated source to your main target (replace <your_target> with actual target name)
target_sources(zerotiercore PRIVATE
    ${DIAGNOSTIC_SCHEMA_EMBED_SRC}
)
