# The diligent feature: the one GPU device 2D and 3D share, and the
# execution of a frame's passes on it.
#
# Diligent creates the Vulkan device — this build of it cannot attach to
# a device that already exists, and has no Metal backend — and SigilSkia
# adopts the device, queue and loader entry points it made, so there is
# one device, one queue and one handle table under both APIs.
#
# workaround: the diligent-engine port expects a volk loader it does not
# provide.
#
# VolkShim.c supplies the volk loader the diligent-engine port expects
# (vendored under thirdparty/volk, MIT), with a Homebrew-aware
# volkInitialize so /opt/homebrew MoltenVK installs work without DYLD
# environment surgery. It is the loader for Diligent alone: SigilSkia
# resolves its own entry points and links nothing, and AdoptDevice.cpp
# passes volk's vkGetInstanceProcAddr on to it so both APIs dispatch
# through the one loader this process opened.
enable_language(C)

find_package(unofficial-diligent-engine CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)

# The shader modules. Each is compiled by slangc when this library is
# built — which is what makes a mistake in one a build failure — and its
# text is generated into a header, because the source a material's body
# is appended to cannot be finished until the material exists.
include(Slang)
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
sigil_slang_module(NAME Portable SOURCE ${_shaders}/Portable.slang
  OUT_VAR SIGIL_WORLD_PORTABLE_H)
sigil_slang_module(NAME Surface SOURCE ${_shaders}/Surface.slang
  ENTRIES vsMain fsFlat INCLUDE ${_shaders}
  DEPENDS ${_shaders}/Portable.slang
  OUT_VAR SIGIL_WORLD_SURFACE_H)
sigil_slang_module(NAME Painter SOURCE ${_shaders}/Painter.slang
  ENTRIES vsPaint fsPaint INCLUDE ${_shaders}
  DEPENDS ${_shaders}/Portable.slang
  OUT_VAR SIGIL_WORLD_PAINTER_H)
sigil_slang_module(NAME Post SOURCE ${_shaders}/Post.slang
  ENTRIES vsFullscreen fsCopy fsBlur fsLevels fsMasked INCLUDE ${_shaders}
  DEPENDS ${_shaders}/Portable.slang
  OUT_VAR SIGIL_WORLD_POST_H)

add_library(SigilWorldDiligent STATIC
  AdoptDevice.cpp
  AdoptDevice.h
  Device.cpp
  Geometry.cpp
  Gpu.cpp
  Gpu.h
  Import.cpp
  PainterGpu.cpp
  PopGpu.cpp
  Post.cpp
  Runtime.cpp
  SweepGpu.cpp
  Compile.cpp
  Compile.h
  VolkShim.c
  ${SIGIL_WORLD_PORTABLE_H}
  ${SIGIL_WORLD_SURFACE_H}
  ${SIGIL_WORLD_PAINTER_H}
  ${SIGIL_WORLD_POST_H}
  ${_shaders}/Portable.slang
  ${_shaders}/Surface.slang
  ${_shaders}/Painter.slang
  ${_shaders}/Post.slang
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/diligent/Device.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/diligent/Import.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/diligent/Painter.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/diligent/Pop.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/diligent/Runtime.h
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/diligent/Sweep.h
)
target_compile_features(SigilWorldDiligent PUBLIC cxx_std_20)
target_include_directories(SigilWorldDiligent
  PUBLIC ${SIGIL_WORLD_INCLUDE_DIR}
  PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${SIGIL_SLANG_GENERATED_DIR})

# SigilSkiaDevice is PUBLIC because Device.h hands out the GpuDevice and
# the Graphite context standing on it, and a caller that uses either
# needs their headers; SigilWorldScene is PUBLIC because the runtime
# value this hands back is executed by a Scene. Diligent and Slang are
# PRIVATE: the public headers name Diligent's two interfaces by forward
# declaration only and Slang not at all.
# SigilMaterialKit is PRIVATE and is read for one thing: whether a
# material's sampled slot holds a map a caller placed or the neutral
# dressing every surface is built with, which is what decides whether a
# slot is bound or left reading one white texel.
target_link_libraries(SigilWorldDiligent
  PUBLIC SigilSkiaDevice SigilWorldScene
  PRIVATE unofficial::diligent-engine::DiligentCore Vulkan::Headers
          slang::slang SigilMaterialKit)

# workaround: volk emits the Metal/MoltenVK WSI entry points only under
# the platform defines.
#
# The engine's Vulkan surface code references the Metal/MoltenVK WSI
# entry points, which volk only emits when the platform extensions are
# visible.
if(APPLE)
  set_source_files_properties(VolkShim.c PROPERTIES COMPILE_DEFINITIONS
    "VK_USE_PLATFORM_METAL_EXT=1;VK_USE_PLATFORM_MACOS_MVK=1")
endif()

# Links Diligent directly: proving that Diligent still drives the device
# after Graphite has submitted on it means issuing a Diligent command.
add_executable(world_diligent_test test/DeviceTest.cpp test/PainterTest.cpp
  test/PopTest.cpp test/RuntimeTest.cpp test/StackTest.cpp
  test/SurfaceTest.cpp test/SweepTest.cpp)
# The runtime test reads the compiled program the compiler registration
# produced, which is this feature's own header rather than a public one.
target_include_directories(world_diligent_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(world_diligent_test PRIVATE SigilWorldDiligent
  SigilMaterialKit unofficial::diligent-engine::DiligentCore GTest::gtest_main)
add_test(NAME world_diligent_test COMMAND world_diligent_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(world_diligent_bench bench/DeviceBench.cpp)
target_include_directories(world_diligent_bench PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(world_diligent_bench PRIVATE SigilWorldDiligent
  SigilMaterialKit benchmark::benchmark)
add_dependencies(benches world_diligent_bench)
