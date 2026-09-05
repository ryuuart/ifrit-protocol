# SigilSketch — everything renderable in this repository is a sketch: one
# file that declares a scene, addressed by its own stem, driven by one
# registry, one live host and one headless renderer.
#
# It stands ABOVE the drawing libraries rather than beside them: a sketch
# may describe a compose Element tree, a world Frame or a pen's frame, so
# this library links all three. Nothing underneath links back — a drawn
# tree, a lit set and a pen's canvas meet here and nowhere else.
#
# One archive, in directories that are the outline: core/ (what a sketch
# is and the registry it joins), the three runtimes canvas/, set/ and
# draw/, live/ (the reload engine), plate/ (the headless renderer) and,
# where the SDK is installed, scry/ (the one web engine sketches borrow).
# Each directory keeps its own tests and benchmarks beside its code. The
# fixtures every sketch test shares are test/support/, reached as
# "support/<name>.h": the one asset store a process holds, the readings a
# test takes off a picture, and the contract every session answers
# whatever it draws through.
sigil_library_root(SigilSketch
  BRIEF "Sketches: one file per scene, one registry, one host"
  DOCS ${CMAKE_CURRENT_LIST_DIR}/kit/README.md)
# Where a sketch looks for what it did not generate, and where the live
# host finds the file behind a registry entry. Neither directory need
# exist: a missing asset yields the placeholder and heals when a file
# appears.
set(SIGIL_SKETCH_DIR ${CMAKE_CURRENT_LIST_DIR}/sketches)
set(SIGIL_SKETCH_ASSET_DIR ${CMAKE_CURRENT_LIST_DIR}/sketches/assets)

find_package(boost_container CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)

# THE ARCHIVE LINKS NO DEVICE BACKEND AND NO QT. The runtime a session
# draws through is a value the process installs — a machine with no
# device runs every set on the CPU mesh executor — and the plate's
# `--gpu` lane draws through the Graphite context the process's device
# already carries, which is why SigilGeometryDevice is here and no
# renderer is: a device of the plate's own would be a second set of
# textures neither could read out of the other. The reload engine watches,
# rebuilds and swaps without a window; book/ is the only place one
# appears.
#
# PUBLIC is what the headers spell: the resource hub, the comparable
# erasure the kind seam is built on, the instruments every runtime times
# itself with, the camera the viewpoint seam is spelled in, and the three
# runtimes' own vocabularies — compose for a canvas, world for a set, the
# pen for a draw. The two doors between runtimes (a compose tree painted
# into a texture, a world frame baked to an image) name the other side by
# forward declaration alone. What a session reads in its own source —
# the byte writer a plate is saved through, the Graphite handle, the
# decoders and encoders — is PRIVATE.
set(SIGIL_SKETCH_SOURCES
  core/Assets.cpp core/Crash.cpp core/Device.cpp core/Registry.cpp
  core/Sources.cpp
  canvas/Context.cpp canvas/Session.cpp
  set/Context.cpp set/Session.cpp
  draw/Session.cpp
  live/Host.cpp live/Residency.cpp
  plate/Compare.cpp plate/Graphite.cpp plate/Story.cpp plate/Sweep.cpp
  plate/Thumbnails.cpp)
set(SIGIL_SKETCH_HEADERS
  core/Assets.h core/CanvasSpec.h core/Crash.h core/Device.h core/Kind.h
  core/Registry.h core/Session.h core/Sources.h
  canvas/Sketch.h
  set/Set.h
  draw/Draw.h
  live/Host.h live/Residency.h
  plate/Compare.h plate/FrameStats.h plate/Graphite.h plate/Story.h
  plate/Sweep.h plate/Thumbnails.h)
set(SIGIL_SKETCH_PUBLIC
  SigilCoreComparable SigilGeometryMeshCamera SigilImageAsset SigilIO
  SigilVideoDecode SigilVideoCore SigilImageEncode SigilMeasure
  SigilMotionClock
  SigilComposeCore SigilComposeKit SigilComposeBrush SigilComposeTypography
  SigilComposeTexture SigilComposeTesting SigilGeometry SigilMaterial
  SigilMaterialKit SigilWeaveKit SigilWeavePorts
  SigilWorldScene SigilWorldKit SigilWorldFrame SigilWorldElement
  SigilWorldLight
  SigilDraw EnTT::EnTT Boost::container)
set(SIGIL_SKETCH_PRIVATE
  SigilIOSource SigilCoreSchedule SigilImage SigilImageDecode
  SigilGeometryDevice SigilSkiaGraphite SigilVideoEncode SigilWeaveFonts)
# The one WebEngine Ultralight permits in a process, lent to every sketch
# that asks for it. SigilScry itself keeps explicit caller ownership as
# its ordinary path; a host that configures the shared engine opts in.
# Where the SDK is not installed there is no engine to share and no
# header to spell.
if(TARGET SigilScry)
  list(APPEND SIGIL_SKETCH_SOURCES scry/SharedEngine.cpp)
  list(APPEND SIGIL_SKETCH_HEADERS scry/SettledPage.h scry/SharedEngine.h)
  list(APPEND SIGIL_SKETCH_PUBLIC SigilScry)
endif()
sigil_library(SigilSketch
  SOURCES ${SIGIL_SKETCH_SOURCES}
  HEADERS ${SIGIL_SKETCH_HEADERS}
  PUBLIC ${SIGIL_SKETCH_PUBLIC}
  PRIVATE ${SIGIL_SKETCH_PRIVATE})

# Each directory's cases and benchmarks, declared beside the code they
# cover, in the library's one test binary and one bench binary.
add_subdirectory(core)
add_subdirectory(canvas)
add_subdirectory(set)
add_subdirectory(draw)
add_subdirectory(live)
add_subdirectory(plate)
if(TARGET SigilScry)
  add_subdirectory(scry)
endif()

# The sheet a sketch stands on: one theme value bound for a scope, and
# the furniture — the stage, the page, the well, the caption — every
# specimen sheet would otherwise retype. It decides no look: the theme is
# a seam and the house one is a stock value over it.
add_subdirectory(kit)

# The sketches themselves, compiled into whichever host links them.
add_subdirectory(sketches)

# Sketchbook: the one live application.
add_subdirectory(book)

# Every sketch stem a document names, resolved against the registry.
# A README points its reader at the sketches that draw the feature beside
# it and spells their stems by hand, so a rename in the registry leaves
# prose pointing at a file that is not there with nothing to notice. This
# is the registry's check — it is the registry the stems are resolved
# against — so it lives here and reads every document in the tree.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
file(GLOB_RECURSE SIGIL_SKETCH_STEM_DOCS CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_LIST_DIR}/../README.md
     ${CMAKE_CURRENT_LIST_DIR}/../FEATURES.md
     ${CMAKE_CURRENT_LIST_DIR}/../TYPOGRAPHY.md)
set(SIGIL_SKETCH_STEM_DOC_ARGS)
foreach(doc IN LISTS SIGIL_SKETCH_STEM_DOCS)
  list(APPEND SIGIL_SKETCH_STEM_DOC_ARGS --doc ${doc})
endforeach()
add_test(NAME sketch_readme_stems
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_LIST_DIR}/test/readme_sketch_stems.py
          --sketch-dir ${SIGIL_SKETCH_DIR}
          ${SIGIL_SKETCH_STEM_DOC_ARGS})

# The checker's own fixtures: a stem must resolve, a misspelt one must
# fail the run, a token outside a sketch paragraph must not be read as a
# stem, and a count of a list must be refused. One Python invocation — and
# the only check that notices the EXTRACTOR narrowing, since a checker that
# silently resolves fewer stems still passes over the corpus.
add_test(NAME sketch_readme_stems_self_test
  COMMAND ${Python3_EXECUTABLE}
          ${CMAKE_CURRENT_LIST_DIR}/test/readme_sketch_stems.py --self-test)
