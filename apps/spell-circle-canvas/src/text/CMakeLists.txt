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

target_link_libraries(TextFlow
  PUBLIC
    unofficial::skia::skia
    absl::inlined_vector  # Word::segments inline storage (public header)
    absl::flat_hash_map   # ExclusionFlow caches (public header)
    absl::node_hash_map   # stable Paragraph& cache entries across inserts
  PRIVATE
    absl::strings          # allocation-aware composite cache-key construction
    harfbuzz::harfbuzz
    ICU::i18n
    ICU::uc
    ICU::data
)

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

add_executable(textflow_test
  test/textflow_test.cpp
)
target_link_libraries(textflow_test PRIVATE TextFlow GTest::gtest_main
  absl::flat_hash_set)
add_test(NAME textflow_test COMMAND textflow_test)

add_executable(textflow_bench
  bench/textflow_bench.cpp
)
target_link_libraries(textflow_bench PRIVATE TextFlow benchmark::benchmark)

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
)
target_link_libraries(textflow_demo PRIVATE TextFlow)
