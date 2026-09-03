# Source tree layout:
#
#   common/      libraries shared across products: skia/ (SigilSkia,
#                Graphite bring-up), ui/ (Ifrit.Ui Qt Quick controls), image/
#                (SigilImage), loader/ (SigilLoader), scry/ (SigilScry),
#                measure/ (SigilMeasure), motion/ (SigilMotion), core/ (SigilCore),
#                geometry/ (SigilGeometry), world/ (SigilWorldElement,
#                SigilWorldFrame, SigilWorldGraph, SigilWorldScene,
#                SigilWorldLight, SigilWorldKit, SigilWorldDiligent),
#                material/ (SigilMaterial), draw/ (SigilDraw), compose/
#                (SigilCompose)
#   sketch/      SigilSketch: every renderable thing in this repository as
#                one sketch file, with the registry, the three runtimes a
#                sketch may draw through, the live host and the headless
#                renderer — and Sketchbook, the one live application
#   sigilweave/  the Qt-free SigilWeave text layout engine, its companion
#                libraries, tests, benchmarks, and examples/ (gallery, demo)
#   spellcircle/ the SpellCircle receiver product: shared/ core embedded by
#                both the qt/ and mac/ executables
# Test support belonging to no library: a scratch directory on disk is
# the standard library and nothing else, and every library's tests want
# the same one. A test target that includes it adds this directory and
# spells "ScratchDir.h".
set(SIGIL_TEST_SUPPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/test)

# Core first: its hardware feature is the device every drawing library
# below stands on.
add_subdirectory(common/core)
add_subdirectory(common/skia)
add_subdirectory(common/ui)
add_subdirectory(common/image)
add_subdirectory(common/loader)
if(SPELLCIRCLE_ENABLE_ULTRALIGHT)
  add_subdirectory(common/scry)
endif()
add_subdirectory(common/measure)
add_subdirectory(common/motion)
add_subdirectory(common/geometry)
add_subdirectory(common/material)
# The immediate-mode pen: over material's paint, weave's text and core's
# mixers, and beneath compose, which hosts a pen program in a node.
add_subdirectory(common/draw)
add_subdirectory(common/world)
if(SPELLCIRCLE_ENABLE_SUBSTANCE)
  add_subdirectory(common/substance)
endif()
if(SPELLCIRCLE_ENABLE_USD)
  add_subdirectory(common/usd)
endif()
add_subdirectory(common/compose)
# Above both drawing libraries, and the only place they meet: a sketch may
# describe a compose Element tree or a world Frame, so this links both.
add_subdirectory(sketch)
add_subdirectory(sigilweave)
add_subdirectory(spellcircle)
