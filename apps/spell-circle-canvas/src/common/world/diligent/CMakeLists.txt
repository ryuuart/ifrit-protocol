# The diligent feature: the execution of a frame's passes on the device
# SigilGeometryDevice created, and the seam values that stand on it.
#
# The device itself is not made here. Diligent cannot attach to a device
# that already exists, so the one creation point has to sit below every
# consumer of one — which is SigilGeometryDevice, and this links it.
find_package(unofficial-diligent-engine CONFIG REQUIRED)

# The shader modules. Each is compiled by slangc when this library is built,
# which makes a mistake in one a build failure. At run time SigilIO loads the
# same authored files before a material body is appended; the generated headers
# remain the build's validation artifacts.
include("${CMAKE_SOURCE_DIR}/src/common/material/cmake/Slang.cmake")
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
# Every module here imports the portable subset, and Surface.slang and
# Painter.slang the material kit's shading terms as well, so slangc is
# pointed at the two directories those files live in. At run time the same
# texts are loaded into the compiler session by name through SigilIO.
set(_imports ${_shaders} ${SIGIL_MATERIAL_SLANG_SHADER_DIR}
             ${SIGIL_MATERIAL_SHADER_DIR})
set(_portable ${SIGIL_MATERIAL_SLANG_SHADER_DIR}/Portable.slang)
sigil_slang_module(NAME Surface SOURCE ${_shaders}/Surface.slang
  ENTRIES vsMain fsFlat INCLUDE ${_imports}
  DEPENDS ${_portable} ${SIGIL_MATERIAL_SHADER_DIR}/Shading.slang
  OUT_VAR SIGIL_WORLD_SURFACE_H)
sigil_slang_module(NAME Painter SOURCE ${_shaders}/Painter.slang
  ENTRIES vsPaint fsPaint INCLUDE ${_imports}
  DEPENDS ${_portable} ${SIGIL_MATERIAL_SHADER_DIR}/Shading.slang
  OUT_VAR SIGIL_WORLD_PAINTER_H)
sigil_slang_module(NAME Post SOURCE ${_shaders}/Post.slang
  ENTRIES vsFullscreen fsCopy fsBlur fsLevels fsMasked INCLUDE ${_imports}
  DEPENDS ${_portable}
  OUT_VAR SIGIL_WORLD_POST_H)

# SigilGeometryDevice is PUBLIC because every entry point here takes the
# device it creates, and Gpu.h holds the resources it made;
# SigilWorldScene is PUBLIC because the runtime value this hands back is
# executed by a Scene. SigilMaterialSlang is PUBLIC because a compiled
# program and the uniforms written into it are what Gpu.h hands around;
# Slang itself is reached only through it, so nothing here links the
# compiler. SigilMaterialKit is PRIVATE and is read for one thing:
# whether a material's sampled slot holds a map a caller placed or the
# neutral dressing every surface is built with, which is what decides
# whether a slot is bound or left reading one white texel. Diligent
# arrives through SigilGeometryDevice, which links it PUBLIC, and is
# never named again here: that target's archive must come first on the
# link line. The hardware device a native import names and the
# mesh-renderer seam the painter stands in are PUBLIC because the headers
# spell them; the Graphite pixel reads an environment upload takes are
# PRIVATE.
sigil_library(SigilWorldDiligent
  SOURCES Geometry.cpp Gpu.cpp Gpu.h Import.cpp PainterGpu.cpp Post.cpp
          Programs.cpp Programs.h Runtime.cpp
          ${SIGIL_WORLD_SURFACE_H} ${SIGIL_WORLD_PAINTER_H}
          ${SIGIL_WORLD_POST_H}
          ${_shaders}/Surface.slang ${_shaders}/Painter.slang
          ${_shaders}/Post.slang ${_shaders}/MaterialEntry.slang
  HEADERS Import.h Painter.h Runtime.h
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${SIGIL_SLANG_GENERATED_DIR}
  PUBLIC SigilGeometryDevice SigilWorldScene SigilMaterialSlang
         SigilGeometryMeshPop SigilCoreHardware SigilGeometryMeshRender
  PRIVATE SigilMaterialKit SigilSkiaGraphite SigilIOHub Boost::container)
target_compile_definitions(SigilWorldDiligent
  PRIVATE SIGIL_WORLD_SHADER_DIR="${_shaders}")

# The runtime test reads the compiled program the compiler registration
# produced, which is this feature's own header rather than a public one.
# HALF THIS BINARY IS A NO-OP WITHOUT A VULKAN RUNTIME, and it reports
# pass either way, because a case that cannot ask its question skips
# rather than fails. The label is what makes that visible from outside:
# `ctest -L gpu` is the run that needs a device, and a run that excludes
# it is a run nobody should read a device verdict out of.
sigil_test(world_diligent_test
  SOURCES test/PainterTest.cpp test/RuntimeTest.cpp test/StackTest.cpp
          test/SurfaceTest.cpp test/Distance.h test/OnDevice.h
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
  LIBRARIES SigilWorldDiligent SigilMaterialKit SigilGeometryKit
  LABELS gpu)

sigil_bench(world_diligent_bench
  SOURCES bench/DeviceBench.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
  LIBRARIES SigilWorldDiligent SigilMaterialKit SigilGeometryKit SigilMeasure)
