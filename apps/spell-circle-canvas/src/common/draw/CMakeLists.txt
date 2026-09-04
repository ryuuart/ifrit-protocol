# SigilDraw — an immediate-mode canvas with p5's brevity: a Pen over an
# SkCanvas carrying p5's verbs with p5's names, held as one value so two
# pens draw side by side and nothing is global. README.md is the canon.
#
# The pen is one static target, with a kit of stock procedural tools over it.
# What the verbs draw WITH is this repository's:
# SigilMaterial's Skia paint as a fill or a stroke, SigilWeave's shaping
# and paragraph engine for text, SigilCore's mixers for the seeded random
# stream and the noise field. Nothing here knows about the sketch runtime
# that steps a pen or the compose tree that may host one — both stand
# above this library and reach down to it.
set(SIGIL_DRAW_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)

add_library(SigilDraw STATIC
  Color.cpp
  Graphics.cpp
  Noise.cpp
  Pen.cpp
  Text.cpp
  ${_shaders}/Subtract.sksl
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/Color.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/Constants.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/Draw.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/Graphics.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/Math.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/Noise.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/Pen.h
  ${SIGIL_DRAW_INCLUDE_DIR}/sigildraw/Retained.h
)
target_compile_features(SigilDraw PUBLIC cxx_std_20)
target_include_directories(SigilDraw PUBLIC ${SIGIL_DRAW_INCLUDE_DIR})
target_compile_definitions(SigilDraw
  PRIVATE SIGIL_DRAW_SHADER_DIR="${_shaders}")
# The material's Skia paint is what a fill is; the weave targets are what
# text is shaped, laid out and painted by; the core mixer is what the
# random stream and the noise field are seeded through. All public: the
# pen's header spells each of their types.
target_link_libraries(SigilDraw PUBLIC
  SigilMaterialSkia SigilCoreCompute
  SigilWeaveStyle SigilWeaveParagraph SigilWeaveLayout SigilWeavePaint
  SigilWeaveFonts)
target_link_libraries(SigilDraw PRIVATE SigilIOHub)
set_target_properties(SigilDraw PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Stock brushes, pressure-bearing paths and direction fields over the pen.
add_subdirectory(kit)

# Links the library and the system font manager its text is shaped
# against, and nothing above either.
add_executable(draw_test test/DrawTest.cpp)
target_link_libraries(draw_test PRIVATE SigilDraw SigilWeavePorts
  GTest::gtest_main)
add_test(NAME draw_test COMMAND draw_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(draw_bench bench/DrawBench.cpp)
target_link_libraries(draw_bench PRIVATE SigilDraw SigilWeavePorts
  benchmark::benchmark)
add_dependencies(benches draw_bench)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilDraw
  BRIEF "An immediate-mode canvas with p5's verbs, over Skia, material paints and shaped text"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
