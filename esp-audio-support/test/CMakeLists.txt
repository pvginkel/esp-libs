# The host tests: esp-audio-support's pure core, built natively against GoogleTest. esp-libs has no
# gates of its own, so ToiletAutomation's `kc project test` runs them:
#
#   cmake -S test -B build-test -G Ninja && cmake --build build-test && ctest --test-dir build-test --output-on-failure

cmake_minimum_required(VERSION 3.20)

project(esp_audio_support_test CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

get_filename_component(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src/core ABSOLUTE)

# The pure core: src/core/. The ESP-IDF side of the component goes in src/, outside it.
file(GLOB_RECURSE PURE_FILES CONFIGURE_DEPENDS ${CORE_DIR}/*.h ${CORE_DIR}/*.cpp)

# An include is checked when the build configures, so an edit to a pure file configures again.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PURE_FILES})

# The rule that makes this build possible: the pure core includes the C++ standard library and
# itself, and nothing else, ESP-IDF least of all. An angle include must be a standard C++ header,
# which has no extension. A quoted include must be a pure file, by its path from the including file
# or from src/core/.
foreach(file ${PURE_FILES})
    get_filename_component(dir ${file} DIRECTORY)
    file(STRINGS ${file} include_lines REGEX "^[ \t]*#[ \t]*include")

    foreach(line ${include_lines})
        if(line MATCHES "#[ \t]*include[ \t]*<([^>]+)>")
            set(included ${CMAKE_MATCH_1})
            if(included MATCHES "\\.")
                message(FATAL_ERROR "${file}: the pure core includes the C++ standard library only, not <${included}>")
            endif()
        elseif(line MATCHES "#[ \t]*include[ \t]*\"([^\"]+)\"")
            set(included ${CMAKE_MATCH_1})
            get_filename_component(from_file ${included} ABSOLUTE BASE_DIR ${dir})
            get_filename_component(from_core ${included} ABSOLUTE BASE_DIR ${CORE_DIR})
            if(NOT from_file IN_LIST PURE_FILES AND NOT from_core IN_LIST PURE_FILES)
                message(FATAL_ERROR "${file}: the pure core includes the C++ standard library only, not \"${included}\"")
            endif()
        endif()
    endforeach()
endforeach()

# GoogleTest, without gmock.
include(FetchContent)

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

# Nothing of ESP-IDF is on this build's include path: src/core/ is, for the pure files' paths.
set(PURE_SOURCES ${PURE_FILES})
list(FILTER PURE_SOURCES INCLUDE REGEX "\\.cpp$")

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(esp_audio_support_tests ${TEST_SOURCES} ${PURE_SOURCES})
target_include_directories(esp_audio_support_tests PRIVATE ${CORE_DIR})
target_link_libraries(esp_audio_support_tests PRIVATE GTest::gtest_main)
target_compile_options(esp_audio_support_tests PRIVATE -Wall -Wextra -Werror)

# The component builds with -Wconversion on the device, so the core does here too.
set_source_files_properties(${PURE_SOURCES} PROPERTIES COMPILE_OPTIONS -Wconversion)

enable_testing()

include(GoogleTest)
gtest_discover_tests(esp_audio_support_tests)
