# Source tree layout:
#
#   common/      libraries shared across products, in the order they are
#                added below: core/ (SigilCore), skia/ (SigilSkia,
#                Graphite bring-up), qt/ (Ifrit.Qt desktop components and services),
#                media/ (SigilMedia), io/ (SigilIO),
#                data/ (SigilData), protocol/ (SigilProtocol), scry/
#                (SigilScry), measure/
#                (SigilMeasure), motion/ (SigilMotion), geometry/
#                (SigilGeometry), material/ (SigilMaterial), draw/
#                (SigilDraw), world/ (SigilWorldElement, SigilWorldFrame,
#                SigilWorldGraph, SigilWorldScene, SigilWorldLight,
#                SigilWorldKit, SigilWorldDiligent) and usd/ (SigilUsd),
#                which leaves itself out where its SDK is not installed, and
#                compose/ (SigilCompose) last. Each library carries its
#                own features as its own archives, and a build module
#                only one library needs stands in that library's cmake/.
#   sketch/      SigilSketch: every renderable thing in this repository as
#                one sketch file, with the registry, the three runtimes a
#                sketch may draw through, the live host and the headless
#                renderer — and Sketchbook, the one live application
#   sigilweave/  the Qt-free SigilWeave text layout engine, its companion
#                libraries, tests, benchmarks, and examples/ (gallery, demo)
#   spellcircle/ the SpellCircle receiver product: shared/ core embedded by
#                both the qt/ and mac/ executables
# Test support belonging to no library: a scratch directory on disk is
# the standard library and nothing else, and a font context is one line
# every test binary that draws text would otherwise write for itself.
# Beside them, test/assets/ holds the instrument faces — generated fonts
# carrying one property each, which a case shapes when its answer must
# not depend on which fonts the machine has. They belong here rather than
# to a library because more than one library reads them; a fixture only
# one library needs stays in that library's own test/assets/. Every test
# binary sees this directory, spells "ScratchDir.h" or "Fonts.h", and
# reaches the faces through SIGIL_TEST_INSTRUMENT_DIR.
set(SIGIL_TEST_SUPPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/test)

# Core first: its hardware feature is the device every drawing library
# below stands on.
add_subdirectory(common/core)
add_subdirectory(common/skia)
if(SIGIL_BUILD_APPS)
  add_subdirectory(common/qt)
endif()
add_subdirectory(common/media)
add_subdirectory(common/io)
# Tabular data and the mappings read off it. Over io's byte seam only:
# a decoder speaks that vocabulary, and registers itself on whatever
# hub a host holds, so nothing here inherits the hub or a codec.
add_subdirectory(common/data)
# The inspection and automation protocol: its definition is a FlatBuffers
# schema read through data's value generator, so it stands on data alone;
# the libraries whose domains it describes never see it.
add_subdirectory(common/protocol)
add_subdirectory(common/scry)
add_subdirectory(common/measure)
add_subdirectory(common/motion)
add_subdirectory(common/geometry)
add_subdirectory(common/material)
# The immediate-mode pen: over material's paint, weave's text and core's
# mixers, and beneath compose, which hosts a pen program in a node.
add_subdirectory(common/draw)
add_subdirectory(common/world)
add_subdirectory(common/usd)
add_subdirectory(common/compose)
if(SIGIL_BUILD_PYTHON)
  add_subdirectory(common/python)
endif()
# Above both drawing libraries, and the only place they meet: a sketch may
# describe a compose Element tree or a world Frame, so this links both.
add_subdirectory(sketch)
add_subdirectory(sigilweave)
if(SIGIL_BUILD_APPS)
  add_subdirectory(spellcircle)
# Seer hosts wire inspection and the shared receiver frontend.
  add_subdirectory(seer)
endif()
