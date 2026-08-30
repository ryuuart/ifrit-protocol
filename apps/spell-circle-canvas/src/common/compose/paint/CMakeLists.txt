# SigilComposePaint — what fills a region.
#
# Pattern tiles and the stock generators (`patterns::`), SDF materials
# (`sdf::`), layer-style presets (`styles::`) and — when OpenColorIO is in
# the dependency closure — output view transforms (paint/Ocio.h). Links
# Brush publicly: the layer-style presets are spelled in keylines and
# shadows.
add_library(SigilComposePaint STATIC
  Patterns.cpp
  Sdf.cpp
  LayerStyles.cpp
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/paint/LayerStyles.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/paint/Paint.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/paint/Pattern.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/paint/Patterns.h
  ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/paint/Sdf.h
)
target_compile_features(SigilComposePaint PUBLIC cxx_std_20)
target_link_libraries(SigilComposePaint PUBLIC SigilComposeBrush)

# OpenColorIO is already in the dependency closure (transitively via
# OpenImageIO), so this adds no new vcpkg dependency — the extension simply
# disables with a warning if the package is absent. The definition is PUBLIC
# so a consumer can `#ifdef SIGILCOMPOSE_ENABLE_OCIO` around its use.
find_package(OpenColorIO CONFIG QUIET)
if(TARGET OpenColorIO::OpenColorIO)
  target_sources(SigilComposePaint PRIVATE Ocio.cpp
    ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/paint/Ocio.h)
  target_link_libraries(SigilComposePaint PRIVATE OpenColorIO::OpenColorIO)
  target_compile_definitions(SigilComposePaint PUBLIC SIGILCOMPOSE_ENABLE_OCIO)
else()
  message(WARNING "OpenColorIO not found — <sigilcompose/paint/Ocio.h> disabled")
endif()

# Patterns, SDF materials, layer styles, the pattern pan and colour
# management.
_compose_test_binary(compose_paint_test
  LIBRARIES SigilComposePaint
  SOURCES
    test/ComposeTestBrushesPaint.cpp
    test/ComposeTestContentPaint.cpp
    test/ComposeTestEngineDocsPaint.cpp
    test/ComposeTestGrammarPaint.cpp
    test/ComposeTestKernelPaint.cpp
    test/ComposeTestLinesPaint.cpp
    test/ComposeTestMaskPaint.cpp
    test/ComposeTestTextDataPaint.cpp
)

# Pattern bakes and fills, SDF materials against paths, layer-style shadows.
_compose_bench(compose_paint_bench
  LIBRARIES SigilComposePaint
  SOURCES bench/ComposePaintBench.cpp)
