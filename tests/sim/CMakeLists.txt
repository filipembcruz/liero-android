# Builds the headless simulation harness against a given OpenLiero source tree.
#   cmake -S tests/sim -B build-x -DLIERO_SRC=<path to .../src>
cmake_minimum_required(VERSION 3.16)
project(liero_sim CXX C)

if(NOT LIERO_SRC)
  message(FATAL_ERROR "Set -DLIERO_SRC=<liero src dir>")
endif()
# Only SDL *headers* are needed (the core simulation never calls SDL).
if(NOT SDL2_INCLUDE_DIR)
  find_package(SDL2 REQUIRED)
  get_target_property(SDL2_INCLUDE_DIR SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -w")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2 -w")

set(CORE
  game/bobject.cpp game/bonus.cpp game/common.cpp game/console.cpp
  game/constants.cpp game/filesystem.cpp game/game.cpp game/level.cpp
  game/math.cpp game/ninjarope.cpp game/nobject.cpp game/rand.cpp
  game/settings.cpp game/sobject.cpp game/spectatorviewport.cpp
  game/stats_recorder.cpp game/sys.cpp game/text.cpp game/viewport.cpp
  game/weapon.cpp game/worm.cpp game/ai/dijkstra.cpp game/ai/predictive_ai.cpp
  game/gfx/blit.cpp game/gfx/font.cpp game/gfx/palette.cpp
  game/gfx/renderer.cpp game/gfx/sprite.cpp game/mixer/mixer.c
  game/mixer/player.cpp gvl/zlib/miniz.c gvl/system/system.c
  gvl/support/bits.c gvl/support/debug.cpp gvl/support/platform.cpp
  gvl/support/profile.cpp gvl/io2/stream.cpp gvl/io2/convert.cpp
  gvl/containers/list.cpp)
list(TRANSFORM CORE PREPEND "${LIERO_SRC}/")

add_executable(liero_sim sim_main.cpp ${CORE})
target_include_directories(liero_sim PRIVATE ${LIERO_SRC} ${LIERO_SRC}/game ${SDL2_INCLUDE_DIR})
if(LIERO_STATIC)
  target_link_options(liero_sim PRIVATE -static)
endif()
