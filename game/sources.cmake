# Source lists shared by the Android build and the desktop test builds.
# Expects LIERO_SRC to point at game/src.
set(LIERO_CORE_SOURCES
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
set(LIERO_UI_SOURCES
  game/gfx.cpp game/keys.cpp game/main.cpp game/replay.cpp game/sfx.cpp
  game/weapsel.cpp game/controller/commonController.cpp
  game/controller/localController.cpp game/controller/replayController.cpp
  game/controller/stats_presenter.cpp game/menu/booleanSwitchBehavior.cpp
  game/menu/enumBehavior.cpp game/menu/hiddenMenu.cpp
  game/menu/integerBehavior.cpp game/menu/itemBehavior.cpp
  game/menu/mainMenu.cpp game/menu/menu.cpp game/menu/menuItem.cpp
  game/menu/timeBehavior.cpp
  game/mobile/touch.cpp game/mobile/platform.cpp game/mobile/ui.cpp
  game/mobile/view.cpp game/mobile/menu.cpp game/mobile/config.cpp game/mobile/hd.cpp)
set(LIERO_MOBILE_SOURCES ${LIERO_CORE_SOURCES} ${LIERO_UI_SOURCES})
list(TRANSFORM LIERO_MOBILE_SOURCES PREPEND "${LIERO_SRC}/")
