# ComposeSketch — the p5-style live-coding host (cr.h/RCC++ pattern):
# sketches are real C++ over the full compose API, compiled on save
# into dylibs that resolve the framework's symbols from the host
# executable (-undefined dynamic_lookup + -export_dynamic), so a sketch
# build is one small TU with no linking against the static libraries.
#
# The kit is a proper library: its PUBLIC deps define what sketches may
# use, and SketchAnchor.cpp compiles the sketch prelude inside the real
# target graph — ExtractSketchFlags.cmake lifts that TU's command from
# compile_commands.json into sketch_flags.rsp at build time, so sketch
# compilation has no hand-maintained flag list.
add_library(SigilSketchKit STATIC
  SketchHost.cpp
  SketchHost.h
  Assets.cpp
  SketchAnchor.cpp
)
target_include_directories(SigilSketchKit PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
# PUBLIC so sketches see it too: `--target fetch_assets` writes the
# open-licensed demo faces here (cmake/FetchAssets.cmake), and the sketch
# host also accepts `--assets <dir>` to point at it directly.
target_compile_definitions(SigilSketchKit PUBLIC
  SIGIL_ASSET_DIR="${SIGIL_ASSET_DIR}")
target_link_libraries(SigilSketchKit PUBLIC
  SigilCompose SigilLoader SigilWeavePorts EnTT::EnTT)
set_target_properties(SigilSketchKit PROPERTIES
  POSITION_INDEPENDENT_CODE ON)

qt_add_executable(ComposeSketch
  sketch_main.cpp
)
qt_add_qml_module(ComposeSketch
  URI Compose.Sketch
  VERSION 1.0
  QML_FILES qml/Main.qml
  SOURCES ComposeSketchView.cpp ComposeSketchView.h
)
set_source_files_properties(qml/Main.qml
  PROPERTIES QT_RESOURCE_ALIAS Main.qml)
target_include_directories(ComposeSketch PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ComposeSketch PRIVATE
  SigilSketchKit Qt6::Gui Qt6::GuiPrivate Qt6::Quick)

# Frames render through the shared Skia Graphite context straight into the
# QQuickRhiItem texture when the QRhi backend is supported (same pattern as
# ComposeGallery); other builds keep the raster-and-upload fallback.
if(TARGET SpellCircleSkiaQt)
  target_link_libraries(ComposeSketch PRIVATE SpellCircleSkiaQt)
  target_compile_definitions(ComposeSketch PRIVATE SIGILCOMPOSE_SKETCH_GPU)
endif()
if(APPLE)
  target_link_options(ComposeSketch PRIVATE "LINKER:-export_dynamic")
  # Sketches may call ANY part of the SDK, not just what the host's own
  # TUs reference — force the full archives into the executable so
  # -undefined dynamic_lookup always resolves. (Skia is the exception:
  # vcpkg builds it hidden-visibility, so sketches link its archive
  # directly — see EXTRA below.)
  foreach(sketch_dep IN ITEMS SigilSketchKit SigilCompose SigilImage
                              SigilLoader SigilMotion SigilWeave
                              SigilWeavePorts)
    target_link_options(ComposeSketch PRIVATE
      "LINKER:-force_load,$<TARGET_FILE:${sketch_dep}>")
  endforeach()
else()
  # Non-Apple ports: mirror with --whole-archive when the sketch host
  # lands there (see the repo's other platform-draft notes).
  target_link_options(ComposeSketch PRIVATE "LINKER:--export-dynamic")
endif()

# The rsp is a tracked build output (not POST_BUILD): deleting it or
# reconfiguring (compile_commands.json rewrites at generate time)
# regenerates it, independent of whether ComposeSketch relinks.
add_custom_command(
  OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/sketch_flags.rsp
  COMMAND ${CMAKE_COMMAND}
    -DCOMPDB=${CMAKE_BINARY_DIR}/compile_commands.json
    -DANCHOR=${CMAKE_CURRENT_LIST_DIR}/SketchAnchor.cpp
    -DCONFIG=$<CONFIG>
    -DOUT=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/sketch_flags.rsp
    -DEXTRA=$<TARGET_FILE:unofficial::skia::skia>
    -P ${CMAKE_SOURCE_DIR}/cmake/ExtractSketchFlags.cmake
  DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
          ${CMAKE_SOURCE_DIR}/cmake/ExtractSketchFlags.cmake
  COMMENT "Extracting sketch compile flags from compile_commands.json"
  VERBATIM)
add_custom_target(sketch_flags
  DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/sketch_flags.rsp)
add_dependencies(ComposeSketch sketch_flags)

# End-to-end smoke test of the reload pipeline: compile the starter
# sketch through the same flags, dlopen it, render a frame.
add_test(NAME compose_sketch_smoke
  COMMAND ComposeSketch
    ${CMAKE_CURRENT_LIST_DIR}/sketches/hello.cpp
    --frame ${CMAKE_BINARY_DIR}/compose_sketch_smoke.png)

# The split-Skia guard. A sketch dylib links libskia.a directly (vcpkg
# builds Skia hidden-visibility, so its symbols cannot be re-exported from
# the host), which means SkRuntimeEffect::MakeForShader builds the SkSL AST
# in the SKETCH's Skia image while getRPProgram and the inliner run in the
# HOST's — virtual dispatch across that boundary faults on PAC. Stock SkSL
# materials must therefore keep main() monolithic and must not break a loop
# against a uniform. This sketch PAINTS one of each of them, so a shader
# that violates the rule segfaults here instead of in someone's sketch.
add_test(NAME compose_sketch_stock
  COMMAND ComposeSketch
    ${CMAKE_CURRENT_LIST_DIR}/sketches/stock_materials.cpp
    --frame ${CMAKE_BINARY_DIR}/compose_sketch_stock.png)
