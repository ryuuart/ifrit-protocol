# SigilDraw — an immediate-mode canvas with p5's brevity: a Pen over an
# SkCanvas carrying p5's verbs with p5's names, held as one value so two
# pens draw side by side and nothing is global.
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
# pen's header spells each of their types.
find_package(boost_container_hash CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
sigil_library(SigilDraw
  SOURCES Color.cpp Graphics.cpp Noise.cpp Pen.cpp PenClip.cpp
          PenImage.cpp PenInternal.cpp PenInternal.h PenShapes.cpp
          PenTransform.cpp Text.cpp
  HEADERS Color.h Constants.h Draw.h Graphics.h Math.h Noise.h Pen.h
          PenTypes.h Retained.h
  PUBLIC SigilMaterialSkia SigilCoreCompute
         SigilWeaveStyle SigilWeaveParagraph SigilWeaveLayout SigilWeavePaint
         SigilWeaveFonts Boost::container_hash Boost::unordered)
sigil_shader_sources(SigilDraw DIR ${_shaders} NAMESPACE sigil::draw)

add_subdirectory(brush)

# The pen's tests link the system font manager, which is both what loads
# the instrument face every text case sets in and what resolves a family
# when no face is named. Only the case that names none reads the
# machine's own families, so it alone carries the `fonts` label.
sigil_test(draw_test
  SOURCES test/PenShapeTest.cpp test/PenStyleTest.cpp
          test/PenTransformTest.cpp test/PenImageTest.cpp
          test/PenValuesTest.cpp test/GraphicsTest.cpp
          test/DrawTextTest.cpp test/PenInheritTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_DRAW_SHADER_DIR="${_shaders}"
  LIBRARIES SigilDraw SigilWeavePorts)
sigil_test(draw_test
  SUITES PenMachineFace
  LABELS fonts)

sigil_bench(draw_bench
  SOURCES bench/DrawBench.cpp
  LIBRARIES SigilDraw SigilWeavePorts)

# Every API name the prose spells, compiled against the headers that own
# it — README.md and the brush chapter beside it. The libraries this one
# stands on are scanned so a name it borrows resolves where it lives: the
# paint value, the noise and chance streams, the polyline vocabulary and
# the paragraph.
#
# The names it borrows from ABOVE are exempted instead. Resolving one
# would mean this library's test binary linking the libraries that stand
# on it, which is the dependency the tiering exists to prevent.
sigil_doc_probes(draw
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/brush/README.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../geometry/include/sigilgeometry/path
           ${CMAKE_CURRENT_SOURCE_DIR}/../material/include/sigilmaterial/skia
           ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../../sigilweave/include
  PRELUDES "<sigilgeometry/path/Polyline.h>"
           "<sigilgeometry/path/Lattice.h>"
           "<sigilweave/paragraph/Paragraph.h>"
  EXCLUDE "sigil::sketch=SigilSketch stands on this library, so its names are named and not probed"
          "sketch::DrawSketch=SigilSketch stands on this library, so its names are named and not probed"
          "sketch::DrawContext=SigilSketch stands on this library, so its names are named and not probed"
          "sketch::Set=SigilSketch stands on this library, so its names are named and not probed"
          "compose::pen=SigilCompose stands on this library, so its names are named and not probed"
  FLOORS 20,3,1,0,5
  LIBRARIES SigilDraw SigilDrawBrush SigilIOSource SigilWeavePorts)
