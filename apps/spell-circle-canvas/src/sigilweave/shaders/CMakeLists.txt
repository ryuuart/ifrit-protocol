# Preset SkSL shaders (water, mesh gradient, sparkle, …) for PaintStyle
# layers. A separate library so no engine feature carries SkRuntimeEffect
# code: presets are content, not engine, and consumers that never use them
# need not link them.
add_library(SigilWeaveShaders STATIC
  PaintShaders.cpp
  ${SIGIL_WEAVE_INCLUDE_DIR}/sigilweave/shaders/PaintShaders.h
)
target_compile_features(SigilWeaveShaders PUBLIC cxx_std_20)
target_include_directories(SigilWeaveShaders PUBLIC
  $<BUILD_INTERFACE:${SIGIL_WEAVE_INCLUDE_DIR}>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(SigilWeaveShaders
  PUBLIC
    unofficial::skia::skia
  PRIVATE
    SigilMaterialKit
    SigilMaterialSkia
)
