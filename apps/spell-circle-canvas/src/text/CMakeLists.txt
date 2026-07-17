# TextFlow — a flexible Skia text layout engine (LayoutNG / Pretext inspired).
#
# Deliberately Qt-free: it consumes UTF-8/UTF-16 text plus SkTypefaces and
# produces positioned SkTextBlobs, so it can be reused from the renderer, the
# benchmarks, and standalone tools alike. See README.md in this directory for
# the architecture (itemization → word shape cache → flow placement).
add_library(TextFlow STATIC
  FontContext.cpp
  Shaper.cpp
  Paragraph.cpp
  Flow.cpp
  ParagraphLayout.cpp
  KnuthPlass.cpp
  Query.cpp
  SingleLineParagraphCache.cpp

  include/textflow/TextFlow.h
  include/textflow/Features.h
  include/textflow/InlineVector.h
  include/textflow/Style.h
  include/textflow/FontContext.h
  include/textflow/Shaper.h
  include/textflow/Paragraph.h
  include/textflow/Flow.h
  include/textflow/ParagraphLayout.h
  include/textflow/Query.h
  include/textflow/SingleLineParagraphCache.h
  include/textflow/Choreograph.h
)

target_compile_features(TextFlow PUBLIC cxx_std_20)

target_include_directories(TextFlow PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

# Abseil is an implementation detail: no absl type appears in any public
# header (Word::segments uses textflow::InlineVector; the ExclusionFlow and
# SingleLineParagraphCache maps hide behind pimpls), so consumers of an
# installed TextFlow never include abseil headers — the archives still need
# linking, which the future package config expresses via find_dependency.
target_link_libraries(TextFlow
  PUBLIC
    unofficial::skia::skia
  PRIVATE
    absl::flat_hash_map   # FontContextImpl + ExclusionFlow::PathCache
    absl::node_hash_map   # stable Paragraph& cache entries across inserts
    absl::strings         # allocation-aware composite cache-key construction
    harfbuzz::harfbuzz
    ICU::i18n
    ICU::uc
    ICU::data
)

# Preset SkSL shaders (water, mesh gradient, sparkle, …) for PaintStyle
# layers. A separate library so the core engine carries no
# SkRuntimeEffect/SkSL code: presets are content, not engine, and consumers
# that never use them shouldn't link them. Shares the core include dir so
# the header keeps its <textflow/PaintShaders.h> spelling.
add_library(TextFlowShaders STATIC
  shaders/PaintShaders.cpp
  include/textflow/PaintShaders.h
)
target_compile_features(TextFlowShaders PUBLIC cxx_std_20)
target_include_directories(TextFlowShaders PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(TextFlowShaders PUBLIC unofficial::skia::skia)

# Platform glue kept out of the core: the system-font-manager factory
# (CoreText today; DirectWrite/Fontconfig slot into SystemFontManager.cpp
# later). Core TextFlow accepts any SkFontMgr and never links this; only
# tools and apps that want "the OS fonts" do.
add_library(TextFlowPorts STATIC
  ports/SystemFontManager.cpp
  include/textflow/ports/SystemFontManager.h
)
target_compile_features(TextFlowPorts PUBLIC cxx_std_20)
target_include_directories(TextFlowPorts PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(TextFlowPorts PUBLIC unofficial::skia::skia)

# Qt ↔ Skia bridging helpers (header-only). Separate target so the core
# TextFlow library stays Qt-free; only Qt consumers link this.
add_library(TextFlowQt INTERFACE
  qt/include/textflowqt/TextFlowQt.h
)
target_include_directories(TextFlowQt INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/qt/include
)
target_link_libraries(TextFlowQt INTERFACE
  Qt6::Gui
  unofficial::skia::skia
  TextFlow # bridges Paragraph/Query types directly
)

# One TU per module (mirroring the library's own structure) linked into a
# single test binary; shared fixtures live in TestSupport.
add_executable(textflow_test
  test/TestSupport.cpp test/TestSupport.h
  test/ShaperTest.cpp
  test/ParagraphTest.cpp
  test/FlowTest.cpp
  test/LayoutTest.cpp
  test/KnuthPlassTest.cpp
  test/TypographyTest.cpp
  test/OverflowTest.cpp
  test/QueryTest.cpp
  test/VerticalTest.cpp
  test/StressTest.cpp
  test/InlineVectorTest.cpp
)
target_link_libraries(textflow_test PRIVATE TextFlow TextFlowShaders
  TextFlowPorts GTest::gtest_main absl::flat_hash_set)
add_test(NAME textflow_test COMMAND textflow_test)

add_executable(textflow_bench
  bench/textflow_bench.cpp
)
target_link_libraries(textflow_bench PRIVATE TextFlow TextFlowShaders
  TextFlowPorts benchmark::benchmark)

add_executable(textflow_demo
  demo/textflow_demo.cpp
  demo/DemoSupport.cpp demo/DemoSupport.h
  demo/DemoScenes.h
  demo/SceneExclusions.cpp
  demo/SceneKnuthPlass.cpp
  demo/SceneFreeform.cpp
  demo/SceneExtreme.cpp
  demo/SceneTypography.cpp
  demo/ScenePretext.cpp
  demo/SceneRain.cpp
  demo/SceneRipple.cpp
  demo/SceneLoop.cpp
  demo/SceneBabel.cpp
  demo/SceneFeatures.cpp
  demo/SceneCjk.cpp
  demo/SceneShapes.cpp
  demo/SceneFallback.cpp
  demo/SceneHyperScripts.cpp
  demo/ScenePaintEffects.cpp
)
target_link_libraries(textflow_demo PRIVATE TextFlow TextFlowShaders
  TextFlowPorts)
