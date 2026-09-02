# The diligent feature: the execution of a frame's passes on the device
# SigilGeometryDevice created, and the seam values that stand on it.
#
# The device itself is not made here. Diligent cannot attach to a device
# that already exists, so the one creation point has to sit below every
# consumer of one — which is SigilGeometryDevice, and this links it.
find_package(unofficial-diligent-engine CONFIG REQUIRED)

# The shader modules. Each is compiled by slangc when this library is
# built — which is what makes a mistake in one a build failure — and its
# text is generated into a header, because the source a material's body
# is appended to cannot be finished until the material exists.
include(Slang)
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
# Every module here imports the portable subset, and Surface.slang the
# material kit's shading terms as well, so slangc is pointed at the two
# directories those files live in. At run time the same texts are loaded
# into the compiler session by name, from the headers their own builds
# generated.
set(_imports ${_shaders} ${SIGIL_MATERIAL_SLANG_SHADER_DIR}
             ${SIGIL_MATERIAL_SHADER_DIR})
set(_portable ${SIGIL_MATERIAL_SLANG_SHADER_DIR}/Portable.slang)
sigil_slang_module(NAME Surface SOURCE ${_shaders}/Surface.slang
  ENTRIES vsMain fsFlat INCLUDE ${_imports}
  DEPENDS ${_portable} ${SIGIL_MATERIAL_SHADER_DIR}/Shading.slang
  OUT_VAR SIGIL_WORLD_SURFACE_H)
sigil_slang_module(NAME Painter SOURCE ${_shaders}/Painter.slang
  ENTRIES vsPaint fsPaint INCLUDE ${_imports}
  DEPENDS ${_portable}
  OUT_VAR SIGIL_WORLD_PAINTER_H)
sigil_slang_module(NAME Post SOURCE ${_shaders}/Post.slang
  ENTRIES vsFullscreen fsCopy fsBlur fsLevels fsMasked INCLUDE ${_imports}
  DEPENDS ${_portable}
  OUT_VAR SIGIL_WORLD_POST_H)

add_library(SigilWorldDiligent STATIC
  Geometry.cpp
  Gpu.cpp
  Gpu.h
  Import.cpp
  PainterGpu.cpp
  PopGpu.cpp
  Post.cpp
  Programs.cpp
  Programs.h
  Runtime.cpp
  SweepGpu.cpp
  ${SIGIL_WORLD_SURFACE_H}
  ${SIGIL_WORLD_PAINTER_H}
  ${SIGIL_WORLD_POST_H}
  ${_shaders}/Surface.slang
  ${_shaders}/Painter.slang
  ${_shaders}/Post.slang
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

# SigilGeometryDevice is PUBLIC because every entry point here takes the
# device it creates, and Gpu.h holds the resources it made; SigilWorldScene is PUBLIC because the runtime
# value this hands back is executed by a Scene. Diligent and Slang are
# PRIVATE: the public headers name Diligent's two interfaces by forward
# declaration only and Slang not at all.
# SigilMaterialSlang is PUBLIC because a compiled program and the
# uniforms written into it are what Gpu.h hands around; Slang itself is
# reached only through it, so nothing here links the compiler.
# SigilMaterialKit is PRIVATE and is read for one thing: whether a
# material's sampled slot holds a map a caller placed or the neutral
# dressing every surface is built with, which is what decides whether a
# slot is bound or left reading one white texel.
# Diligent arrives through SigilGeometryDevice, which links it PUBLIC,
# and is never named again here — see that feature's CMakeLists for why
# the order matters.
target_link_libraries(SigilWorldDiligent
  PUBLIC SigilGeometryDevice SigilWorldScene SigilMaterialSlang
  PRIVATE SigilMaterialKit)

add_executable(world_diligent_test test/PainterTest.cpp
  test/PopTest.cpp test/RuntimeTest.cpp test/StackTest.cpp
  test/SurfaceTest.cpp test/SweepTest.cpp)
# The runtime test reads the compiled program the compiler registration
# produced, which is this feature's own header rather than a public one.
target_include_directories(world_diligent_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(world_diligent_test PRIVATE SigilWorldDiligent
  SigilMaterialKit SigilGeometryKit GTest::gtest_main)
add_test(NAME world_diligent_test COMMAND world_diligent_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(world_diligent_bench bench/DeviceBench.cpp)
target_include_directories(world_diligent_bench PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(world_diligent_bench PRIVATE SigilWorldDiligent
  SigilMaterialKit SigilGeometryKit SigilMeasure benchmark::benchmark)
add_dependencies(benches world_diligent_bench)
