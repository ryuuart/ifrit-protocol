# SigilWorld — diegetic surfaces in real 3D over Diligent Engine
# (Vulkan/MoltenVK on macOS; the Diligent layer keeps D3D/GL open for
# the Windows/Linux ports). Geometry arrives as sigil::shape::Mesh,
# panel content as SkImage textures, frames leave as raster SkImages —
# the Skia world and the GPU world meet at those three seams.
#
# VolkShim.c supplies the volk loader the diligent-engine port expects
# (vendored under thirdparty/volk, MIT), with a Homebrew-aware
# volkInitialize so /opt/homebrew MoltenVK installs work without
# DYLD environment surgery.
enable_language(C)

add_library(SigilWorld STATIC
  Scene.cpp
  TextureSet.cpp
  VolkShim.c
  World.cpp
  include/sigilworld/Animation.h
  include/sigilworld/Components.h
  include/sigilworld/Easel.h
  include/sigilworld/Scene.h
  include/sigilworld/TextureSet.h
  include/sigilworld/World.h
)

target_compile_features(SigilWorld PUBLIC cxx_std_20)

target_include_directories(SigilWorld
  PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}
)

find_package(EnTT CONFIG REQUIRED)
find_package(unofficial-diligent-engine CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)

# The engine's Vulkan surface code references the Metal/MoltenVK WSI
# entry points, which volk only emits when the platform extensions are
# visible.
if(APPLE)
  set_source_files_properties(VolkShim.c PROPERTIES COMPILE_DEFINITIONS
    "VK_USE_PLATFORM_METAL_EXT=1;VK_USE_PLATFORM_MACOS_MVK=1")
endif()

# SigilMotion is PUBLIC because Animation.h puts motion::Animatable in
# the public component surface. It links choreograph and nothing else —
# no Skia, no Yoga, no kernel — which is exactly why the animation
# VALUES could move out of SigilCompose (2026-07-29) and land here.
# SigilWorld stays compose-free; world_test links SigilCompose
# TEST-ONLY, below.
target_link_libraries(SigilWorld
  PUBLIC SigilShape unofficial::skia::skia EnTT::EnTT SigilMotion
  PRIVATE unofficial::diligent-engine::DiligentCore Vulkan::Headers
)

add_executable(world_test
  test/WorldTest.cpp
)

# SigilCompose is a TEST-ONLY dependency here (the library above stays
# compose-free): world_demo's marquee slices its strip through compose's
# tiles:: door, and WorldMarqueeSlice pins the transforms that slice
# depends on so a change over there cannot silently move the band.
target_link_libraries(world_test PRIVATE SigilWorld SigilCompose
  GTest::gtest_main)

add_test(NAME world_test COMMAND world_test)

# Headless PNG shots of the diegetic-panel scene:
#   world_demo [outdir]            (default world_demo_out)
add_executable(world_demo
  examples/world_demo.cpp
)

# SigilLoader feeds the demo's fetched-asset poster (SVG -> texture).
# SigilCompose authors the marquee textures (element trees baked via
# snapshot()); the weave ports/kit supply fonts and styles. SigilWorld
# itself stays text- and compose-free.
target_link_libraries(world_demo PRIVATE SigilWorld SigilLoader
  SigilCompose SigilWeave SigilWeaveShaders SigilWeavePorts
  SigilWeaveKit)

# The material lab shot dresses its props with a fetched texture set and,
# when the Substance SDK was found, with .sbsar materials rendered by
# SigilSubstance from the SDK's own sample archives. Without the SDK the
# lab still renders; the Substance props are simply absent.
if(SPELLCIRCLE_ENABLE_USD)
  # The lit material lab is also written out as a USD crate.
  target_link_libraries(world_demo PRIVATE SigilUsd)
  target_compile_definitions(world_demo PRIVATE SIGIL_WORLD_DEMO_USD)
endif()
if(SPELLCIRCLE_ENABLE_SUBSTANCE)
  target_link_libraries(world_demo PRIVATE SigilSubstance)
  target_compile_definitions(world_demo PRIVATE
    SIGIL_WORLD_DEMO_SUBSTANCE_ASSETS="${SUBSTANCE_SDK_DIR}/assets")
endif()

# The GPU pop executor under load (Google Benchmark; needs a Vulkan
# runtime, skips without one; use a Release build):
#   ./build/bin/Release/world_bench
add_executable(world_bench bench/WorldBench.cpp)
target_link_libraries(world_bench PRIVATE SigilWorld benchmark::benchmark)

# --- Slang kernels ---------------------------------------------------------
# The pop kernel is authored ONCE in Slang (shaders/pop.slang over the
# shaders/sigilspline.slang module), compiled to SPIRV at build time by
# the manifest's slangc, and embedded — Diligent consumes the bytecode
# directly, no glslang in the loop. Without slangc the build still
# configures and surface rendering still works (the graphics
# vertex/pixel shader ships as an HLSL source string in World.cpp), but
# the compute generators have no fallback: placeSweep/placeChain/
# placeChainOn return 0 and log a one-time diagnostic.
find_program(SLANGC_EXECUTABLE slangc
  PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/shader-slang")
if(SLANGC_EXECUTABLE)
  set(_slang_dir ${CMAKE_CURRENT_LIST_DIR}/shaders)
  set(_spirv_dir ${CMAKE_CURRENT_BINARY_DIR}/spirv)
  set(_pop_entries CSSplineScatter CSJitter CSNoise CSRamp CSVary
      CSLookAt CSMath CSRelax CSFill CSAtlas CSLookup CSSelect CSAffine
      CSPeak CSDeform CSMix CSPointSet CSCopyBack CSPopPack)
  set(_spv_files)
  macro(_sigil_slang _file _entry)
    set(_spv ${_spirv_dir}/${_file}_${_entry}.spv)
    add_custom_command(OUTPUT ${_spv}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${_spirv_dir}
      COMMAND ${SLANGC_EXECUTABLE} ${_slang_dir}/${_file}.slang
              -I ${_slang_dir} -target spirv -entry ${_entry} -o ${_spv}
      DEPENDS ${_slang_dir}/${_file}.slang
              ${_slang_dir}/sigilspline.slang
              ${_slang_dir}/WorldShaderParams.h
      COMMENT "slangc ${_entry}" VERBATIM)
    list(APPEND _spv_files ${_spv})
  endmacro()
  foreach(_entry IN LISTS _pop_entries)
    _sigil_slang(pop ${_entry})
  endforeach()
  _sigil_slang(sweep CSSweep)
  add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/world_pop_spirv.h
    COMMAND ${CMAKE_COMMAND} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/world_pop_spirv.h
            "-DSPV_FILES=${_spv_files}"
            -P ${CMAKE_CURRENT_LIST_DIR}/EmbedSpirv.cmake
    DEPENDS ${_spv_files} ${CMAKE_CURRENT_LIST_DIR}/EmbedSpirv.cmake
    COMMENT "embed pop SPIRV" VERBATIM)
  target_sources(SigilWorld PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/world_pop_spirv.h)
  target_include_directories(SigilWorld PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  target_compile_definitions(SigilWorld PRIVATE SIGILWORLD_POP_SPIRV)
endif()

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilWorld
  BRIEF "Windowless 3D scenes drawn on a real GPU and handed back as raster images"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
