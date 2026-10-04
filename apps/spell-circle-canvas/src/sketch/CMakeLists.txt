# SigilSketch — everything renderable in this repository is a sketch: one
# file that declares a scene, addressed by its own stem, driven by one
# registry, one live host and one headless renderer.
#
# It stands ABOVE the drawing libraries rather than beside them: a sketch
# may describe a compose Element tree or a world Frame, and a pen paints
# through a node of the first, so this library links all three. Nothing
# underneath links back — a drawn tree, a lit set and a pen's canvas meet
# here and nowhere else.
#
# One archive, in directories that are the outline: core/ (what a sketch
# is and the registry it joins), the two runtimes canvas/ and set/,
# live/ (the reload engine), plate/ (the headless renderer) and,
# where the SDK is installed, scry/ (the one web engine sketches borrow).
# Each directory keeps its own tests and benchmarks beside its code. The
# fixtures every sketch test shares are test/support/, reached as
# "support/<name>.h": the one asset store a process holds, the readings a
# test takes off a picture, and the contract every session answers
# whatever it draws through.
sigil_library_root(SigilSketch
  BRIEF "Sketches: one file per scene, one registry, one host"
  DOCS ${CMAKE_CURRENT_LIST_DIR}/RUNNING.md
       ${CMAKE_CURRENT_LIST_DIR}/REGISTRY.md
       ${CMAKE_CURRENT_LIST_DIR}/HOST.md
       ${CMAKE_CURRENT_LIST_DIR}/TESTING.md
       ${CMAKE_CURRENT_LIST_DIR}/kit/README.md
       ${CMAKE_CURRENT_LIST_DIR}/PROTOCOL.md)
# Where the live host finds the file behind a registry entry, and what
# mounts at res://: the demo assets a machine fetched (fonts, models,
# textures), while a sketch's own files stand in its directory and reach
# it through sketch://. Neither directory need exist: a missing asset
# yields the placeholder and heals when a file appears.
set(SIGIL_SKETCH_DIR ${CMAKE_CURRENT_LIST_DIR}/sketches)
set(SIGIL_SKETCH_ASSET_DIR ${SIGIL_ASSET_DIR})

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
# PUBLIC supplies the types and complete runtime values the library's own
# headers name. Authoring kits, image codecs and device executors are
# consumers' own dependencies. The two doors between runtimes name the other
# side by forward declaration; their texture and scene executors are PRIVATE,
# beside the codecs and Graphite handles the implementation uses.
set(SIGIL_SKETCH_SOURCES
  core/Assets.cpp core/Catalog.cpp core/Crash.cpp core/Device.cpp
  core/Registry.cpp core/Sources.cpp core/State.cpp
  canvas/Context.cpp canvas/Guest.cpp canvas/Session.cpp
  set/Context.cpp set/Session.cpp
  live/BuildDirectory.cpp live/BuildCache.cpp live/Host.cpp live/Residency.cpp live/SkewGuard.cpp
  plate/Compare.cpp plate/Graphite.cpp plate/Story.cpp plate/Sweep.cpp
  plate/ThumbnailQueue.cpp plate/Thumbnails.cpp plate/ThumbnailWriter.cpp)
set(SIGIL_SKETCH_HEADERS
  core/Assets.h core/CanvasSpecification.h core/Catalog.h core/Crash.h
  core/Device.h
  core/Kind.h core/Members.h core/Placement.h core/Registry.h core/Session.h
  core/Sources.h core/State.h
  canvas/Guest.h canvas/Sketch.h
  set/Set.h
  live/Host.h live/Residency.h
  plate/Compare.h plate/Graphite.h plate/Story.h
  plate/Sweep.h plate/ThumbnailQueue.h plate/Thumbnails.h
  plate/ThumbnailWriter.h)
set(SIGIL_SKETCH_PUBLIC
  SigilCoreCallable SigilCoreComparable
  SigilGeometryMeshCamera SigilGeometryMeshRender SigilMediaCore
  SigilIOHub SigilIOFrames SigilMeasure SigilMaterialColor SigilMaterialTexture
  SigilMotionClock
  SigilComposeCore SigilWorldFrame SigilWorldElement)
set(SIGIL_SKETCH_PRIVATE
  # The door a frame arrives by. A canvas can wear another application's
  # picture, so the archive that draws reaches the subscription — the
  # publisher beside it is still nobody's but the host's, and neither
  # protocol reaches a header.
  SigilComposeTexture SigilWorldScene SigilMaterialSkia SigilWeavePaint
  SigilDataDecode
  SigilDataQuery
  SigilDataRead
  SigilIOSource SigilCoreSchedule SigilMedia SigilMediaImageDecode
  # A sketch opens network feeds through its assets, so the host is the
  # consumer that links the transports.
  SigilIOTransport
  SigilGeometryDevice SigilSkiaGraphite SigilMediaVideoDecode
  SigilMediaImageEncode SigilMediaVideoEncode SigilWeaveFonts)
# The one WebEngine Ultralight permits in a process, lent to every sketch
# that asks for it. SigilScry itself keeps explicit caller ownership as
# its ordinary path; a host that configures the shared engine opts in.
# Where the SDK is not installed there is no engine to share and no
# header to spell.
if(TARGET SigilScry)
  list(APPEND SIGIL_SKETCH_SOURCES scry/SharedEngine.cpp scry/Settling.cpp)
  list(APPEND SIGIL_SKETCH_HEADERS scry/SettledPage.h scry/SharedEngine.h
       scry/Settling.h)
  list(APPEND SIGIL_SKETCH_PUBLIC SigilScry)
endif()
sigil_library(SigilSketch
  SOURCES ${SIGIL_SKETCH_SOURCES}
  HEADERS ${SIGIL_SKETCH_HEADERS}
  PUBLIC ${SIGIL_SKETCH_PUBLIC}
  PRIVATE ${SIGIL_SKETCH_PRIVATE})

if(SIGIL_BUILD_PYTHON)
  add_subdirectory(python)
endif()

# Each directory's cases and benchmarks, declared beside the code they
# cover, in the library's one test binary and one bench binary.
add_subdirectory(core)
add_subdirectory(canvas)
add_subdirectory(set)
add_subdirectory(live)
add_subdirectory(testing)
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
if(SIGIL_BUILD_APPS)
  add_subdirectory(sketches)

# Sketchbook: the one live application.
  add_subdirectory(book)
endif()

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
     ${CMAKE_CURRENT_LIST_DIR}/../TYPOGRAPHY.md
     ${CMAKE_CURRENT_LIST_DIR}/RUNNING.md
     ${CMAKE_CURRENT_LIST_DIR}/REGISTRY.md
     ${CMAKE_CURRENT_LIST_DIR}/HOST.md
     ${CMAKE_CURRENT_LIST_DIR}/TESTING.md)
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

# The web panel's page names the runtime probe SigilScry answers, which
# is only there where the SDK was found.
set(SIGIL_SKETCH_DOC_SCRY_PRELUDE)
set(SIGIL_SKETCH_DOC_SCRY_ALIASES)
set(SIGIL_SKETCH_DOC_SKIP_HEADERS)
set(SIGIL_SKETCH_DOC_EXCLUDE)
if(TARGET SigilScry)
  set(SIGIL_SKETCH_DOC_SCRY_PRELUDE "<sigilscry/platform/Runtime.h>")
  set(SIGIL_SKETCH_DOC_SCRY_ALIASES scry=::sigil::scry)
else()
  set(SIGIL_SKETCH_DOC_SKIP_HEADERS SettledPage.h Settling.h SharedEngine.h)
  set(SIGIL_SKETCH_DOC_EXCLUDE
    "scry::available=the optional web SDK is not configured"
    "sigil::scry::WebEngine=the optional web SDK is not configured"
    "sketch::scry::sharedEngine=the optional web SDK is not configured"
    "sketch::scry::Sequence=the optional web SDK is not configured"
    "sketch::scry::settle=the optional web SDK is not configured")
endif()

# Every API name the prose spells, compiled against the headers that own
# it — README.md and its chapters, the protocol's page, the kit's page and
# the studies' page, because each is canon for what it covers. This library composes every other, so every
# one it names is scanned and linked: a sketch's prose is written in the
# whole vocabulary a sketch is written in.
sigil_doc_probes(sketch
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/RUNNING.md
          ${CMAKE_CURRENT_SOURCE_DIR}/REGISTRY.md
          ${CMAKE_CURRENT_SOURCE_DIR}/HOST.md
          ${CMAKE_CURRENT_SOURCE_DIR}/TESTING.md
          ${CMAKE_CURRENT_SOURCE_DIR}/PROTOCOL.md
          ${CMAKE_CURRENT_SOURCE_DIR}/kit/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/sketches/README.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../common/core/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/data/include/sigildata
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/io/include/sigilio
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/motion/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/compose/include/sigilcompose
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/geometry/include/sigilgeometry
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/world/include/sigilworld
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/material/include/sigilmaterial
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/media/include/sigilmedia
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/measure/include/sigilmeasure
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/scry/include/sigilscry
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/draw/include/sigildraw
           ${CMAKE_CURRENT_SOURCE_DIR}/../sigilweave/include/sigilweave
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/protocol/include/sigilprotocol
           ${CMAKE_BINARY_DIR}/generated/protocol/sigilprotocol
  PRELUDES "<sigilio/advanced/Places.h>" "<sigilio/advanced/Time.h>"
           ${SIGIL_SKETCH_DOC_SCRY_PRELUDE}
           "<sigilio/transport/Transport.h>"
           "<sigilcompose/Compose.h>"
           "<sigilcompose/draw/Draw.h>"
           "<sigilcompose/kit/Marquee.h>"
           "<sigilcompose/kit/Plate.h>"
           "<sigilcompose/typography/TextEffect.h>"
           "<sigilcompose/typography/Presets.h>"
           "<sigilgeometry/kit/Generators.h>"
           "<sigilworld/diligent/Import.h>"
           "<sigilweave/ports/SystemFontManager.h>"
           "<sigilmedia/video/Video.h>"
           "<sigilmaterial/substance/Substance.h>"
           "<sigilmaterial/program/Shader.h>"
           "<sigildata/query/Database.h>"
  ALIASES ${SIGIL_SKETCH_DOC_SCRY_ALIASES}
  SKIP_HEADERS ${SIGIL_SKETCH_DOC_SKIP_HEADERS}
  EXCLUDE ${SIGIL_SKETCH_DOC_EXCLUDE}
  FLOORS 50,10,10,0,0
  LIBRARIES SigilSketch SigilIOFrames SigilDataDecode SigilDataScale
            SigilSketchSessionAgent SigilSketchTestingHarness
            SigilMaterialSubstance SigilComposeKit SigilComposeBrush
            SigilComposeTypography SigilComposeDraw SigilComposeTesting
            SigilGeometry SigilWorldDiligent SigilWeaveKit SigilWeavePorts)
