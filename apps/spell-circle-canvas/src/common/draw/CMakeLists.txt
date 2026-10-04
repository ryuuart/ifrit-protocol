sigil_library_root(SigilDraw
  BRIEF "An immediate-mode canvas with p5's verbs, over Skia, material paints and shaped text"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/docs/Groups.dox
       ${CMAKE_CURRENT_SOURCE_DIR}/reference/GRAPHICS.md)

set(draw_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)

# Public links supply the types named in the pen's headers.
find_package(boost_container_hash CONFIG REQUIRED)
find_package(boost_unordered CONFIG REQUIRED)
sigil_library(SigilDraw
  SOURCES Color.cpp Graphics.cpp Noise.cpp Pen.cpp PenClip.cpp
          PenImage.cpp PenInternal.cpp PenInternal.h PenShapes.cpp
          PenTransform.cpp Text.cpp
  HEADERS Color.h Constants.h Graphics.h Math.h Noise.h Pen.h
          PenTypes.h Retained.h
  PUBLIC SigilMaterialSkia SigilMediaCore SigilCoreCompute SigilGeometryKit
         SigilGeometryPath SigilWeaveStyle SigilWeaveParagraph SigilWeaveLayout SigilWeavePaint
         SigilWeaveFonts Boost::container_hash Boost::unordered)
sigil_shader_sources(SigilDraw DIR ${draw_shaders} NAMESPACE sigil::draw)

add_subdirectory(brush)

sigil_test(draw_test
  SOURCES test/PenShapeTest.cpp test/PenStyleTest.cpp
          test/PenTransformTest.cpp test/PenImageTest.cpp
          test/PenValuesTest.cpp test/GraphicsTest.cpp
          test/DrawTextTest.cpp test/PenInheritTest.cpp
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
  DEFINITIONS SIGIL_DRAW_SHADER_DIR="${draw_shaders}"
  # The surface recipe is a test fixture supplied through the paint seam.
  LIBRARIES SigilDraw SigilWeavePorts SigilMaterialSurface)
# Default-family lookup needs machine fonts; instrument-face cases do not.
sigil_test(draw_test
  SUITES PenMachineFace
  LABELS fonts)

# The device case builds wherever Graphite does and skips where the
# machine offers no device of its own.
sigil_test(draw_test
  SOURCES test/GraphicsHeightGpuTest.cpp
  LIBRARIES SigilSkiaGraphite
  SUITES GraphicsHeightGpu LABELS gpu integration)

sigil_bench(draw_bench
  SOURCES bench/DrawBench.cpp
  LIBRARIES SigilDraw SigilWeavePorts)

# Probe borrowed names against their originating headers. Names belonging
# to consumers above this library are excluded to keep dependency direction.
sigil_doc_probes(draw
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/brush/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/GRAPHICS.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Pen.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../geometry/include/sigilgeometry/path
           ${CMAKE_CURRENT_SOURCE_DIR}/../geometry/include/sigilgeometry/kit
           ${CMAKE_CURRENT_SOURCE_DIR}/../material/include/sigilmaterial/skia
           ${CMAKE_CURRENT_SOURCE_DIR}/../media/include/sigilmedia/core
           ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../../sigilweave/include
  PRELUDES "<sigilgeometry/path/Polyline.h>"
           "<sigilgeometry/path/Lattice.h>"
           "<sigilgeometry/path/Noise.h>"
           "<sigilweave/paragraph/Paragraph.h>"
  EXCLUDE "sigil::sketch=SigilSketch stands on this library, so its names are named and not probed"
          "sigil::compose=SigilCompose stands on this library, so its names are named and not probed"
          "sketch::SetSketch=SigilSketch stands on this library, so its names are named and not probed"
          "sketch::SketchContext=SigilSketch stands on this library, so its names are named and not probed"
          "sigil::compose::Element=SigilCompose is a consumer of this library, and the guest helper names its public type"
          "std::source_location::current=The standard library is outside the scanner's include roots"
          "compose::pen=SigilCompose stands on this library, so its names are named and not probed"
          "compose::graphics=SigilCompose stands on this library, so its names are named and not probed"
  FLOORS 28,10,40,0,6
  LIBRARIES SigilDraw SigilDrawBrush SigilIOSource SigilWeavePorts)
