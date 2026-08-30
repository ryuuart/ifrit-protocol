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
  SketchCrash.cpp
  SketchHost.h
  Assets.cpp
  SketchAnchor.cpp
  StaticSketches.cpp
)
target_include_directories(SigilSketchKit PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
# PUBLIC so sketches see it too: `--target fetch_assets` writes the
# open-licensed demo faces here (cmake/FetchAssets.cmake), and the sketch
# host also accepts `--assets <dir>` to point at it directly.
target_compile_definitions(SigilSketchKit PUBLIC
  SIGIL_ASSET_DIR="${SIGIL_ASSET_DIR}")
# SigilComposeTesting is here for the --verify path: a sketch that reports
# its checks reads <sigilcompose/testing/Checks.h>, and what a sketch may
# include is exactly this target's PUBLIC dependencies. It is header-only,
# so it adds nothing to the force_load list below.
target_link_libraries(SigilSketchKit PUBLIC
  SigilCompose SigilComposeTesting SigilLoader SigilGeometry SigilWeavePorts
  EnTT::EnTT)
set_target_properties(SigilSketchKit PROPERTIES
  POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# SigilSketchStudies — the same sketch files, compiled INTO a host.
#
# Each study is one TU with SIGIL_SKETCH_STATIC set to its stem, which
# switches SIGIL_SKETCH (Sketch.h) from exporting dylib entry points to
# registering a factory. ComposeGallery links this and gets every study as a
# scene; nothing about the sketch files changes, so the file someone
# hot-reloads and the scene the gallery shows are the same file, always.
#
# An OBJECT library, not STATIC: registration happens in a namespace-scope
# initializer with nothing referring to it, which an archive would drop.
# Object libraries link every object by construction — no -force_load, and
# the gallery and its test share one compile of the set.
set(SIGIL_SKETCH_STUDIES
  # Game UI
  ds2_bench fallout2_charsheet ksp_mapview psx_doom_fire xcom_battlescape
  thaumonomicon vagrant_story_target astral_tome bg3_dice_roll
  # Screens and software
  cde_motif spacejam_1996 twoadvanced_equipment twoadvanced_v3 twoadvanced_v4
  winamp_base
  # Science and data
  chaucer_astrolabe chevreul_circle chladni_tab1 minard_1869
  nightingale_coxcomb
  # Pattern and tradition
  black_watch kumiko_asanoha penrose_paving sigillum_aemeth
  thunder_fulu dunhuang_star_chart
  # Motion and film
  genesis_fire hitman_verlet slitscan_2001 vertigo_titles
  eva_magi_defense lain_navi eva_magi_interior
  # Type in motion
  shipping_forecast karaoke_wipe axis_ripple elastic_type matrix_rain
  rota_convocationis
  # The kit itself
  hello stock_materials stroke_atlas shapeworks_lab easel_playground
  # One sketch per API — legible over pretty. Each is a place to stand:
  # small, single-purpose, and captioned with the call that made it.
  travel_path wiggle_shake env_theme material_child matte_luma pop_lanes
  pop_deform geo_groups blur_falloff ember_decode
  # §38's measured fixture as a living scene: one never-moving bound fill
  # over 512 cells — the reproduction of the 19.6× and the demonstration
  # that the settle/release extension retires it.
  still_accent
)
set(sigil_study_sources "")
foreach(study IN LISTS SIGIL_SKETCH_STUDIES)
  set(study_source ${CMAKE_CURRENT_LIST_DIR}/sketches/${study}.cpp)
  set_source_files_properties(${study_source} PROPERTIES
    COMPILE_DEFINITIONS "SIGIL_SKETCH_STATIC=\"${study}\"")
  list(APPEND sigil_study_sources ${study_source})
endforeach()
add_library(SigilSketchStudies OBJECT ${sigil_study_sources})
target_link_libraries(SigilSketchStudies PUBLIC SigilSketchKit)
set_target_properties(SigilSketchStudies PROPERTIES
  POSITION_INDEPENDENT_CODE ON)
# Where a study looks for anything it does not generate. It need not exist:
# Assets serves the magenta placeholder and heals when a file appears.
target_compile_definitions(SigilSketchStudies PUBLIC
  SIGIL_SKETCH_ASSET_DIR="${CMAKE_CURRENT_LIST_DIR}/sketches/assets")

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
  # -undefined dynamic_lookup always resolves. Skia is in that list now
  # too: the registry port's `default-visibility` feature (requested in
  # vcpkg.json) drops Skia's -fvisibility=hidden, which is what makes the
  # host able to re-export it. Until then a sketch had to link libskia.a
  # itself, leaving two Skia images in the process whose objects could not
  # be exchanged — see the split-Skia note on compose_sketch_stock below.
  # SigilGeometry was missing from this list until 2026-07-29, and the symptom
  # is the "two registries of one fact" shape again: the kit's PUBLIC
  # dependencies decide what a sketch may #include (via sketch_flags.rsp),
  # this list decides what a sketch may LINK, and nothing checked that they
  # agree. So shapeworks_lab, easel_playground and pop_lanes all compiled,
  # all ran as compiled-in gallery scenes, and all failed to dlopen —
  # `symbol not found in flat namespace ..._ZN5sigil5shape...`. Anything
  # added to SigilSketchKit's PUBLIC libraries belongs here too — as the
  # ARCHIVES it resolves to: SigilCompose is an interface over the four
  # compose archives and SigilGeometry one over its feature archives, and
  # an interface target has no file to force-load.
  foreach(sketch_dep IN ITEMS SigilSketchKit SigilComposeCore
                              SigilComposeShape SigilComposeBrush
                              SigilComposePaint SigilImageAsset
                              SigilImageDecode SigilLoader SigilMotionClock
                              SigilMotionBind SigilGeometryPath
                              SigilGeometryBlend SigilGeometryMesh
                              SigilGeometrySpace SigilGeometryCurves
                              SigilGeometryPoints SigilGeometryPop
                              SigilGeometryCodec SigilGeometryMaterial
                              SigilWeave SigilWeavePorts
                              unofficial::skia::skia)
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
    # No EXTRA: the host force_loads Skia and re-exports it, so a sketch
    # resolves Skia through -undefined dynamic_lookup like the rest of the
    # SDK. Linking the archive here is what used to split Skia in two.
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

# Paints one of every stock SkSL material, so a material that cannot compile
# or draw fails here rather than in someone's sketch.
#
# This began life as the split-Skia guard, from when a sketch dylib linked
# libskia.a itself: SkRuntimeEffect::MakeForShader then built the SkSL AST in
# the SKETCH's Skia image while getRPProgram and the inliner ran in the HOST's,
# and virtual dispatch across that boundary faulted on PAC — so stock materials
# had to keep main() monolithic and not break a loop against a uniform. That
# boundary is gone: skia[default-visibility] lets the host re-export Skia, the
# sketch resolves it through -undefined dynamic_lookup like every other SDK
# library, and only one Skia image is loaded. The rule it enforced no longer
# applies; the coverage is still worth keeping.
add_test(NAME compose_sketch_stock
  COMMAND ComposeSketch
    ${CMAKE_CURRENT_LIST_DIR}/sketches/stock_materials.cpp
    --frame ${CMAKE_BINARY_DIR}/compose_sketch_stock.png)

# THE LINK-SURFACE GUARD, and the reason it is a third test rather than a
# case in either of the two above: what it watches is not a compile and not
# a pixel, it is the -force_load list.
#
# SigilGeometry was missing from that list from the day sketches could use it
# until 2026-07-29. Every shape-using sketch compiled (the kit's PUBLIC
# dependencies decide what a sketch may #include, via sketch_flags.rsp) and
# every one ran fine as a compiled-in gallery scene (SigilSketchStudies is
# an object library linked into ComposeGallery, which resolves nothing
# dynamically) — and all three failed at dlopen with `symbol not found in
# flat namespace ..._ZN5sigil5shape...`, because the HOST never re-exported
# the archive. Two registries of one fact, and nothing compared them.
#
# So this test must go through the DYNAMIC path, which `--frame` does: it
# compiles the file with sketch_flags.rsp, dlopens the dylib, runs it, and
# exits nonzero when the load fails. shapeworks_lab.cpp is the widest shape
# surface of the three (Blend, Materials, Mesh, Space), so it is the one
# most likely to catch a PARTIAL regression too.
#
# hello.cpp and stock_materials.cpp cannot cover this: neither names a
# geometry:: symbol, so both load happily with SigilGeometry absent.
add_test(NAME compose_sketch_shape
  COMMAND ComposeSketch
    ${CMAKE_CURRENT_LIST_DIR}/sketches/shapeworks_lab.cpp
    --frame ${CMAKE_BINARY_DIR}/compose_sketch_shape.png)
