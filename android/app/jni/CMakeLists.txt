cmake_minimum_required(VERSION 3.18)
project(LieroAndroid C CXX)

# SDL2 built from source (checked out to jni/SDL by CI / scripts/fetch-sdl.sh)
add_subdirectory(SDL)

set(LIERO_SRC ${CMAKE_CURRENT_LIST_DIR}/../../../game/src)
include(${CMAKE_CURRENT_LIST_DIR}/../../../game/sources.cmake)

add_library(main SHARED ${LIERO_MOBILE_SOURCES} ${LIERO_SRC}/game/mobile/android_main.cpp)
target_compile_definitions(main PRIVATE LIERO_MOBILE=1)
target_include_directories(main PRIVATE ${LIERO_SRC} ${LIERO_SRC}/game)
target_compile_options(main PRIVATE -O2 -w -fno-strict-aliasing)
target_link_libraries(main SDL2 log android)
