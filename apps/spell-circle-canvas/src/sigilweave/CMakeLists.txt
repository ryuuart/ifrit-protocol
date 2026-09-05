# SigilWeave — a Skia text layout engine on HarfBuzz and ICU, as one
# feature library per directory. Each feature is its own static archive
# with its own tests and benchmarks, and links only the features beneath
# it, so a consumer that needs one tier links that tier alone.
# `SigilWeave` is the umbrella: an interface over every engine feature,
# for a consumer of the whole engine. Deliberately Qt-free below the qt
# feature: the engine consumes UTF-8/UTF-16 text plus SkTypefaces and
# produces positioned SkTextBlobs. README.md in this directory is the
# canon.
#
# Test fixtures every binary shares sit in test/support/ and the bench
# corpus in bench/support/; a feature's test and bench binaries include
# "support/<Name>.h". The constructed faces under test/assets/ reach a
# binary through SIGIL_TEST_ASSET_DIR.
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

# Example programs: the headless PNG demo and the interactive Qt gallery.
add_subdirectory(examples/demo)
add_subdirectory(examples/gallery)
