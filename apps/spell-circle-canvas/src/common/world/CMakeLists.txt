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
  VolkShim.c
  World.cpp
  include/sigilworld/World.h
)

target_compile_features(SigilWorld PUBLIC cxx_std_20)

target_include_directories(SigilWorld
  PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}
)

find_package(unofficial-diligent-engine CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)

# The engine's Vulkan surface code references the Metal/MoltenVK WSI
# entry points, which volk only emits when the platform extensions are
# visible.
if(APPLE)
  set_source_files_properties(VolkShim.c PROPERTIES COMPILE_DEFINITIONS
    "VK_USE_PLATFORM_METAL_EXT=1;VK_USE_PLATFORM_MACOS_MVK=1")
endif()

target_link_libraries(SigilWorld
  PUBLIC SigilShape unofficial::skia::skia
  PRIVATE unofficial::diligent-engine::DiligentCore Vulkan::Headers
)

add_executable(world_test
  test/WorldTest.cpp
)

target_link_libraries(world_test PRIVATE SigilWorld GTest::gtest_main)

add_test(NAME world_test COMMAND world_test)

# Headless PNG shots of the diegetic-panel scene:
#   world_demo [outdir]            (default world_demo_out)
add_executable(world_demo
  examples/world_demo.cpp
)

# SigilLoader feeds the demo's fetched-asset poster (SVG -> texture).
target_link_libraries(world_demo PRIVATE SigilWorld SigilLoader)
