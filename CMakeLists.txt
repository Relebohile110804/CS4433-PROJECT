cmake_minimum_required(VERSION 3.16)
project(ModeratedLMS VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LMS_BUILD_TESTS "Build the unit tests" ON)
option(LMS_COVERAGE "Build with gcov coverage flags (GCC/Clang only)" OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(LMS_COVERAGE)
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()
endif()

# ---- External libraries ------------------------------------------------------
find_package(SQLite3 REQUIRED)
# CMake 4.3 renamed the imported target; use the new name when it exists.
if(TARGET SQLite3::SQLite3)
    set(LMS_SQLITE SQLite3::SQLite3)
else()
    set(LMS_SQLITE SQLite::SQLite3)
endif()

# libsodium ships a pkg-config file on Linux; on Windows (vcpkg) fall back to a plain search.
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SODIUM QUIET IMPORTED_TARGET libsodium)
endif()
if(TARGET PkgConfig::SODIUM)
    add_library(sodium ALIAS PkgConfig::SODIUM)
else()
    find_path(SODIUM_INCLUDE_DIR sodium.h REQUIRED)
    find_library(SODIUM_LIBRARY NAMES sodium libsodium REQUIRED)
    add_library(sodium UNKNOWN IMPORTED)
    set_target_properties(sodium PROPERTIES
        IMPORTED_LOCATION "${SODIUM_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SODIUM_INCLUDE_DIR}")
endif()

# ---- Layers ------------------------------------------------------------------
# One library per layer. A layer may only link the layer below it, so a wrong
# #include across layers fails to link instead of slipping through.
# The Data Access layer links Business Logic because it implements the
# repository and adapter interfaces declared there (dependency inversion).
function(lms_layer name dir)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.cpp")
    if(sources)
        add_library(${name} STATIC ${sources})
        target_include_directories(${name} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
    else()
        # Header-only until the first .cpp file is added.
        add_library(${name} INTERFACE)
        target_include_directories(${name} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    endif()
endfunction()

function(lms_link name)
    get_target_property(kind ${name} TYPE)
    if(kind STREQUAL "INTERFACE_LIBRARY")
        target_link_libraries(${name} INTERFACE ${ARGN})
    else()
        target_link_libraries(${name} PUBLIC ${ARGN})
    endif()
endfunction()

lms_layer(lms_business     src/business)
lms_layer(lms_data         src/data)
lms_layer(lms_presentation src/presentation)

lms_link(lms_data         lms_business ${LMS_SQLITE} sodium)

# schema.sql is built into the program, so a new lms.db can be created without any extra files.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/database/schema.sql" LMS_SCHEMA_TEXT)
configure_file(cmake/SchemaSql.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/SchemaSql.h" @ONLY)
target_include_directories(lms_data PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
lms_link(lms_presentation lms_business)

# main.cpp is the composition root: it creates the concrete classes and wires them together.
add_executable(lms src/main.cpp)
target_link_libraries(lms PRIVATE lms_presentation lms_data)


# ---- Tests -------------------------------------------------------------------
if(LMS_BUILD_TESTS)
    enable_testing()
    # Catch2 v3 (single-file amalgamated build) is kept in third_party, so nothing has to be installed.
    # It is compiled once as its own library, without our warning flags, and provides main().
    add_library(catch2 STATIC third_party/catch2/catch_amalgamated.cpp)
    target_include_directories(catch2 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
    target_compile_options(catch2 PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>)

    file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
    add_executable(lms_tests ${test_sources})
    target_link_libraries(lms_tests PRIVATE catch2 lms_presentation lms_business lms_data)
    target_link_libraries(lms_tests PRIVATE ${LMS_SQLITE})
    target_compile_definitions(lms_tests PRIVATE LMS_SCHEMA_PATH="${CMAKE_CURRENT_SOURCE_DIR}/database/schema.sql")

    # One CTest entry per V-Model level, selected by the tags on each TEST_CASE.
    add_test(NAME unit_tests COMMAND lms_tests "[unit]")
    add_test(NAME integration_tests COMMAND lms_tests "[integration]")
    add_test(NAME system_tests COMMAND lms_tests "[system]")
endif()
