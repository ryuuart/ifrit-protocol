# SigilWeave — a flexible Skia text layout engine (LayoutNG / Pretext inspired).
#
# Deliberately Qt-free: it consumes UTF-8/UTF-16 text plus SkTypefaces and
# produces positioned SkTextBlobs, so it can be reused from the renderer, the
# benchmarks, and standalone tools alike. See README.md in this directory for
# the architecture (itemization → word shape cache → flow placement).

# Library version, independent of the host application: bump when the API
# changes so an eventual vcpkg registry port can pin releases.
set(SIGILWEAVE_VERSION 0.1.0)
option(TEXTFLOW_ENABLE_INSTALL
  "Generate install/export rules so find_package(SigilWeave) works from an install tree"
  ON)

add_library(SigilWeave STATIC
  FontContext.cpp
  Shaper.cpp
  Paragraph.cpp
  Flow.cpp
  ParagraphLayout.cpp
  KnuthPlass.cpp
  Query.cpp
  SingleLineParagraphCache.cpp

  include/sigilweave/SigilWeave.h
  include/sigilweave/Features.h
  include/sigilweave/InlineVector.h
  include/sigilweave/Style.h
  include/sigilweave/FontContext.h
  include/sigilweave/Shaper.h
  include/sigilweave/Paragraph.h
  include/sigilweave/Flow.h
  include/sigilweave/ParagraphLayout.h
  include/sigilweave/Query.h
  include/sigilweave/SingleLineParagraphCache.h
  include/sigilweave/Choreograph.h
)

target_compile_features(SigilWeave PUBLIC cxx_std_20)

target_include_directories(SigilWeave PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# Abseil is an implementation detail: no absl type appears in any public
# header (Word::segments uses sigil::weave::InlineVector; the ExclusionFlow and
# SingleLineParagraphCache maps hide behind pimpls), so consumers of an
# installed SigilWeave never include abseil headers — the archives still need
# linking, which the future package config expresses via find_dependency.
target_link_libraries(SigilWeave
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
# the header keeps its <sigilweave/PaintShaders.h> spelling.
add_library(SigilWeaveShaders STATIC
  shaders/PaintShaders.cpp
  include/sigilweave/PaintShaders.h
)
target_compile_features(SigilWeaveShaders PUBLIC cxx_std_20)
target_include_directories(SigilWeaveShaders PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveShaders PUBLIC unofficial::skia::skia)

# Platform glue kept out of the core: the system-font-manager factory
# (CoreText today; DirectWrite/Fontconfig slot into SystemFontManager.cpp
# later). Core SigilWeave accepts any SkFontMgr and never links this; only
# tools and apps that want "the OS fonts" do.
add_library(SigilWeavePorts STATIC
  ports/SystemFontManager.cpp
  include/sigilweave/ports/SystemFontManager.h
)
target_compile_features(SigilWeavePorts PUBLIC cxx_std_20)
target_include_directories(SigilWeavePorts PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeavePorts PUBLIC unofficial::skia::skia)

# Companion utilities distilled from the gallery, demo, and app consumers:
# keyed rebuild guards, paragraph-aware layout memoization, glyph bucketing,
# label/style shorthand, and shared sample content. The point is to make the
# invisible parts of fast SigilWeave usage — cache invalidation keys, draw-call
# batching — explicit and reusable (see kit/README.md). Qt-free; a separate
# target so the core engine carries no convenience layer, and the eventual
# vcpkg port's optional "Kit" component.
add_library(SigilWeaveKit STATIC
  kit/Labels.cpp
  kit/SampleText.cpp

  kit/include/sigilweavekit/SigilWeaveKit.h
  kit/include/sigilweavekit/RebuildGuard.h
  kit/include/sigilweavekit/LayoutGuard.h
  kit/include/sigilweavekit/GlyphBuckets.h
  kit/include/sigilweavekit/Labels.h
  kit/include/sigilweavekit/SampleText.h
  kit/include/sigilweavekit/Timing.h
)
target_compile_features(SigilWeaveKit PUBLIC cxx_std_20)
target_include_directories(SigilWeaveKit PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/kit/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveKit PUBLIC SigilWeave unofficial::skia::skia)

# Qt ↔ Skia bridging helpers (header-only). Separate target so the core
# SigilWeave library stays Qt-free; only Qt consumers link this.
add_library(SigilWeaveQt INTERFACE
  qt/include/sigilweaveqt/SigilWeaveQt.h
)
target_include_directories(SigilWeaveQt INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/qt/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveQt INTERFACE
  Qt6::Gui
  unofficial::skia::skia
  SigilWeave # bridges Paragraph/Query types directly
)

# One TU per module (mirroring the library's own structure) linked into a
# single test binary; shared fixtures live in TestSupport.
add_executable(weave_test
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
target_link_libraries(weave_test PRIVATE SigilWeave SigilWeaveShaders
  SigilWeavePorts SigilWeaveKit GTest::gtest_main absl::flat_hash_set)
add_test(NAME weave_test COMMAND weave_test)

add_executable(weave_bench
  bench/weave_bench.cpp
)
target_link_libraries(weave_bench PRIVATE SigilWeave SigilWeaveShaders
  SigilWeavePorts benchmark::benchmark)

# Example programs: the headless PNG demo and the interactive Qt gallery.
add_subdirectory(examples/demo)
add_subdirectory(examples/gallery)

# ── Install / export (find_package(SigilWeave) from an install tree) ───────
# The eventual private vcpkg registry port drives exactly these rules; the
# generated SigilWeaveConfig.cmake re-finds the dependencies (the libraries
# are static archives, so even PRIVATE deps must be resolvable at consumer
# link time) and exposes the Qt bridge behind an optional "Qt" component:
#   find_package(SigilWeave CONFIG REQUIRED)                # core, Qt-free
#   find_package(SigilWeave CONFIG REQUIRED COMPONENTS Kit) # + sigil::weave::SigilWeaveKit
#   find_package(SigilWeave CONFIG REQUIRED COMPONENTS Qt)  # + sigil::weave::SigilWeaveQt
if(TEXTFLOW_ENABLE_INSTALL)
  include(CMakePackageConfigHelpers)

  install(TARGETS SigilWeave SigilWeaveShaders SigilWeavePorts SigilWeaveKit SigilWeaveQt
    EXPORT SigilWeaveTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
  install(DIRECTORY include/sigilweave DESTINATION include)
  install(DIRECTORY kit/include/sigilweavekit DESTINATION include)
  install(DIRECTORY qt/include/sigilweaveqt DESTINATION include)

  install(EXPORT SigilWeaveTargets
    NAMESPACE sigil::weave::
    DESTINATION share/sigilweave
  )

  configure_package_config_file(
    cmake/SigilWeaveConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SigilWeaveConfig.cmake
    INSTALL_DESTINATION share/sigilweave
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SigilWeaveConfigVersion.cmake
    VERSION ${SIGILWEAVE_VERSION}
    COMPATIBILITY SameMinorVersion
  )
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SigilWeaveConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SigilWeaveConfigVersion.cmake
    DESTINATION share/sigilweave
  )
endif()
