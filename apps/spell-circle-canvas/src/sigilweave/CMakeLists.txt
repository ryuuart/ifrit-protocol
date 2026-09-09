# SigilWeave — a Skia text layout engine on HarfBuzz and ICU, as one
# feature library per directory. Each feature is its own static archive
# with its own tests and benchmarks, and links only the features beneath
# it, so a consumer that needs one tier links that tier alone.
# `SigilWeave` is the umbrella: an interface over every engine feature,
# for a consumer of the whole engine. Deliberately Qt-free below the qt
# feature: the engine consumes UTF-8/UTF-16 text plus SkTypefaces and
# produces positioned SkTextBlobs.
#
# Test fixtures every binary shares sit in test/support/ and the bench
# corpus in bench/support/; a feature's test and bench binaries include
# "support/<Name>.h". The font context is the whole test tree's rather than
# this library's, so a binary reaches it through <Fonts.h>. The constructed
# faces under test/assets/ reach a binary through SIGIL_TEST_ASSET_DIR.
sigil_library_root(SigilWeave
  BRIEF "Styled Unicode text shaped into positioned glyph runs ready for a Skia canvas")

# Dependencies point down this list.
add_subdirectory(unicode)
add_subdirectory(style)
add_subdirectory(fonts)
add_subdirectory(paragraph)
add_subdirectory(layout)
add_subdirectory(decoration)
add_subdirectory(paint)
add_subdirectory(choreograph)
add_subdirectory(query)
add_subdirectory(cache)
add_subdirectory(ports)
add_subdirectory(kit)
add_subdirectory(qt)

# The engine: every feature a consumer of <sigilweave/SigilWeave.h>
# reaches. Shaders, ports, the kit and the Qt bridge stay separate
# targets, as content, platform glue, consumer discipline and a bridge
# rather than engine.
sigil_library(SigilWeave
  INTERFACE SigilWeaveUnicode SigilWeaveStyle SigilWeaveFonts
            SigilWeaveParagraph SigilWeaveLayout SigilWeaveDecoration
            SigilWeavePaint SigilWeaveChoreograph SigilWeaveQuery
            SigilWeaveCache)

# Every API name the prose spells, compiled against the headers that own
# it — README.md, the control-by-control catalogue and the kit's page,
# because each is canon for what it covers. The libraries this one stands
# on are scanned for the contour a path run rides, the distance field a
# glyph atlas is baked into and the caching values the kit's guards are,
# and the tree-wide test support for the instrument faces the testing
# section names.
#
# The names it borrows from ABOVE are exempted instead: the settling
# passage names SigilCompose's worked example, and SigilCompose stands on
# this library.
sigil_doc_probes(weave
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/FEATURES.md
          ${CMAKE_CURRENT_SOURCE_DIR}/kit/README.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../common/geometry/include/sigilgeometry/path
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/image/include/sigilimage
           ${CMAKE_CURRENT_SOURCE_DIR}/../common/core/include
           ${SIGIL_TEST_SUPPORT_DIR}
  PRELUDES "<sigilgeometry/path/Contour.h>"
           "<sigilimage/field/DistanceField.h>"
           "<sigilcore/cache/Rebuild.h>"
           "\"Fonts.h\""
  SKIP_HEADERS SigilWeaveQt.h
  EXCLUDE "sigil::weave::SigilWeaveKit=the exported CMake target's namespaced name, not a C++ name"
          "SkCanvas::saveLayer=Skia's own overloaded member: no requires-spelling can name an overload set, and this generator indexes only the libraries whose include roots it is given"
          "Element::live=SigilCompose stands on this library, so its worked example is named and not probed"
          "Composer::settling=SigilCompose stands on this library, so its worked example is named and not probed"
  FLOORS 35,10,10,0,0
  LIBRARIES SigilWeave SigilGeometryPath SigilImageField SigilCoreCache)

# Example programs: the headless PNG demo and the interactive Qt gallery.
add_subdirectory(examples/demo)
add_subdirectory(examples/gallery)
