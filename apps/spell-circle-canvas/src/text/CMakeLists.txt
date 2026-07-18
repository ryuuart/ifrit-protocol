# TextFlow — a flexible Skia text layout engine (LayoutNG / Pretext inspired).
#
# Deliberately Qt-free: it consumes UTF-8/UTF-16 text plus SkTypefaces and
# produces positioned SkTextBlobs, so it can be reused from the renderer, the
# benchmarks, and standalone tools alike. See README.md in this directory for
# the architecture (itemization → word shape cache → flow placement).

# Library version, independent of the host application: bump when the API
# changes so an eventual vcpkg registry port can pin releases.
set(TEXTFLOW_VERSION 0.1.0)
option(TEXTFLOW_ENABLE_INSTALL
  "Generate install/export rules so find_package(TextFlow) works from an install tree"
  ON)

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
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  $<INSTALL_INTERFACE:include>
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
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  $<INSTALL_INTERFACE:include>
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
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(TextFlowPorts PUBLIC unofficial::skia::skia)

# Companion utilities distilled from the gallery, demo, and app consumers:
# keyed rebuild guards, paragraph-aware layout memoization, glyph bucketing,
# label/style shorthand, and shared sample content. The point is to make the
# invisible parts of fast TextFlow usage — cache invalidation keys, draw-call
# batching — explicit and reusable (see kit/README.md). Qt-free; a separate
# target so the core engine carries no convenience layer, and the eventual
# vcpkg port's optional "Kit" component.
add_library(TextFlowKit STATIC
  kit/Labels.cpp
  kit/SampleText.cpp

  kit/include/textflowkit/TextFlowKit.h
  kit/include/textflowkit/RebuildGuard.h
  kit/include/textflowkit/LayoutGuard.h
  kit/include/textflowkit/GlyphBuckets.h
  kit/include/textflowkit/Labels.h
  kit/include/textflowkit/SampleText.h
  kit/include/textflowkit/Timing.h
)
target_compile_features(TextFlowKit PUBLIC cxx_std_20)
target_include_directories(TextFlowKit PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/kit/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(TextFlowKit PUBLIC TextFlow unofficial::skia::skia)

# Qt ↔ Skia bridging helpers (header-only). Separate target so the core
# TextFlow library stays Qt-free; only Qt consumers link this.
add_library(TextFlowQt INTERFACE
  qt/include/textflowqt/TextFlowQt.h
)
target_include_directories(TextFlowQt INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/qt/include>
  $<INSTALL_INTERFACE:include>
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
  test/KitTest.cpp
)
target_link_libraries(textflow_test PRIVATE TextFlow TextFlowShaders
  TextFlowPorts TextFlowKit GTest::gtest_main absl::flat_hash_set)
add_test(NAME textflow_test COMMAND textflow_test)

add_executable(textflow_bench
  bench/textflow_bench.cpp
)
target_link_libraries(textflow_bench PRIVATE TextFlow TextFlowShaders
  TextFlowPorts benchmark::benchmark)

# Headless PNG panels for library-only surfaces; the interactive/animated
# showcases live in TextFlowGallery (the former duplicated scenes were
# consolidated there).
add_executable(textflow_demo
  demo/textflow_demo.cpp
  demo/DemoSupport.cpp demo/DemoSupport.h
  demo/DemoScenes.h
  demo/SceneExtreme.cpp
  demo/SceneTypography.cpp
  demo/SceneBabel.cpp
  demo/SceneFeatures.cpp
  demo/SceneCjk.cpp
  demo/SceneShapes.cpp
  demo/SceneFallback.cpp
  demo/SceneNewFeatures.cpp
)
target_link_libraries(textflow_demo PRIVATE TextFlow TextFlowShaders
                                            TextFlowPorts TextFlowKit)

# ── Install / export (find_package(TextFlow) from an install tree) ───────
# The eventual private vcpkg registry port drives exactly these rules; the
# generated TextFlowConfig.cmake re-finds the dependencies (the libraries
# are static archives, so even PRIVATE deps must be resolvable at consumer
# link time) and exposes the Qt bridge behind an optional "Qt" component:
#   find_package(TextFlow CONFIG REQUIRED)                # core, Qt-free
#   find_package(TextFlow CONFIG REQUIRED COMPONENTS Kit) # + textflow::TextFlowKit
#   find_package(TextFlow CONFIG REQUIRED COMPONENTS Qt)  # + textflow::TextFlowQt
if(TEXTFLOW_ENABLE_INSTALL)
  include(CMakePackageConfigHelpers)

  install(TARGETS TextFlow TextFlowShaders TextFlowPorts TextFlowKit TextFlowQt
    EXPORT TextFlowTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
  install(DIRECTORY include/textflow DESTINATION include)
  install(DIRECTORY kit/include/textflowkit DESTINATION include)
  install(DIRECTORY qt/include/textflowqt DESTINATION include)

  install(EXPORT TextFlowTargets
    NAMESPACE textflow::
    DESTINATION share/textflow
  )

  configure_package_config_file(
    cmake/TextFlowConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/TextFlowConfig.cmake
    INSTALL_DESTINATION share/textflow
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/TextFlowConfigVersion.cmake
    VERSION ${TEXTFLOW_VERSION}
    COMPATIBILITY SameMinorVersion
  )
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/TextFlowConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/TextFlowConfigVersion.cmake
    DESTINATION share/textflow
  )
endif()
