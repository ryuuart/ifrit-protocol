# SigilDraw — an immediate-mode canvas with p5's brevity: a Pen over an
# SkCanvas carrying p5's verbs with p5's names, held as one value so two
# pens draw side by side and nothing is global. README.md is the canon.
#
# The pen is one static target, with the brush library — procedural tools,
# dabs, strokes, fields and natural-media interiors — as a second target
# over it. What the verbs draw WITH is this repository's: SigilMaterial's
# Skia paint as a fill or a stroke, SigilWeave's shaping and paragraph
# engine for text, SigilCore's mixers for the seeded random stream and
# the noise field. Nothing here knows about the sketch runtime that steps
# a pen or the compose tree that may host one — both stand above this
# library and reach down to it.
sigil_library_root(SigilDraw
  BRIEF "An immediate-mode canvas with p5's verbs, over Skia, material paints and shaped text")

set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)

# The material's Skia paint is what a fill is; the weave targets are what
# text is shaped, laid out and painted by; the core mixer is what the
# random stream and the noise field are seeded through. All public: the
# pen's header spells each of their types. PIC, because the sketch host
# force-loads the pen into a hot-reloaded dylib.
find_package(boost_container_hash CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
sigil_library(SigilDraw PIC
  SOURCES Color.cpp Graphics.cpp Noise.cpp Pen.cpp Text.cpp
  HEADERS Color.h Constants.h Draw.h Graphics.h Math.h Noise.h Pen.h
          Retained.h
  PUBLIC SigilMaterialSkia SigilCoreCompute
         SigilWeaveStyle SigilWeaveParagraph SigilWeaveLayout SigilWeavePaint
         SigilWeaveFonts Boost::container_hash Boost::unordered)
sigil_shader_sources(SigilDraw DIR ${_shaders} NAMESPACE sigil::draw)

add_subdirectory(brush)

# The pen's tests link the system font manager its text is shaped
# against. The text cases stand in their own binary under the `fonts`
# label, since what they measure depends on the machine's default face.
sigil_test(draw_test
  SOURCES test/DrawTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_DRAW_SHADER_DIR="${_shaders}"
  LIBRARIES SigilDraw SigilWeavePorts)
sigil_test(draw_text_test
  SOURCES test/DrawTextTest.cpp
  LIBRARIES SigilDraw SigilWeavePorts
  LABELS fonts)

sigil_bench(draw_bench
  SOURCES bench/DrawBench.cpp
  LIBRARIES SigilDraw SigilWeavePorts)
