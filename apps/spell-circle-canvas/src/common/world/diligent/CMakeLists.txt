# The diligent feature: the execution of a frame's passes on the device
# SigilGeometryDevice created, and the seam values that stand on it.
#
# The device itself is not made here. Diligent cannot attach to a device
# that already exists, so the one creation point has to sit below every
# consumer of one — which is SigilGeometryDevice, and this links it.

# The shader modules. Each one's text is compiled into the archive, and
# each is compiled by slangc when this library is built, which makes a
# mistake in one a build failure rather than a first-frame surprise.
include("${CMAKE_SOURCE_DIR}/src/common/material/cmake/Slang.cmake")
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
# Every module here imports the portable subset, and Surface.slang and
# Painter.slang the material kit's shading terms as well, so slangc is
# pointed at the two directories those files live in. At run time the
# same texts, embedded in the libraries they belong to, are loaded into
# the compiler session by name.
set(_imports ${_shaders} ${SIGIL_MATERIAL_SLANG_SHADER_DIR}
             ${SIGIL_MATERIAL_SHADER_DIR})
set(_portable ${SIGIL_MATERIAL_SLANG_SHADER_DIR}/Portable.slang)
sigil_slang_module(NAME Surface SOURCE ${_shaders}/Surface.slang
  ENTRIES vsMain fsFlat INCLUDE ${_imports}
  DEPENDS ${_portable} ${SIGIL_MATERIAL_SHADER_DIR}/Shading.slang
  PROOF_VAR SIGIL_WORLD_SURFACE_PROOF)
sigil_slang_module(NAME Painter SOURCE ${_shaders}/Painter.slang
  ENTRIES vsPaint fsPaint INCLUDE ${_imports}
  DEPENDS ${_portable} ${SIGIL_MATERIAL_SHADER_DIR}/Shading.slang
  PROOF_VAR SIGIL_WORLD_PAINTER_PROOF)
sigil_slang_module(NAME Post SOURCE ${_shaders}/Post.slang
  ENTRIES vsFullscreen fsCopy fsBlur fsLevels fsMasked INCLUDE ${_imports}
  DEPENDS ${_portable}
  PROOF_VAR SIGIL_WORLD_POST_PROOF)

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
          ${SIGIL_WORLD_SURFACE_PROOF} ${SIGIL_WORLD_PAINTER_PROOF}
          ${SIGIL_WORLD_POST_PROOF}
  HEADERS Import.h Maps.h Painter.h Runtime.h
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR}
  PUBLIC SigilGeometryDevice SigilWorldScene SigilMaterialSlang
         SigilGeometryMeshPop SigilCoreHardware SigilGeometryMeshRender
  PRIVATE SigilMaterialKit SigilSkiaGraphite Boost::container)
sigil_shader_sources(SigilWorldDiligent
  DIR ${_shaders} NAMESPACE sigil::world::diligent)

# Every case here reads this feature through its public headers alone —
# the source directory is not on the test's include path — and the seam
# values, the cameras and the readings they share sit in
# test/DeviceSeams.h, over the one device a test process brings up, which
# is SigilGeometryDevice's fixture because that is where a device can be
# created at all.
#
# HALF THIS BINARY IS A NO-OP WITHOUT A VULKAN RUNTIME, and it reports
# pass either way, because a case that cannot ask its question skips
# rather than fails. The label is what makes that visible from outside:
# `ctest -L gpu` is the run that needs a device, and a run that excludes
# it is a run nobody should read a device verdict out of.
sigil_test(world_diligent_test
  SOURCES test/PainterTest.cpp test/RuntimeTest.cpp test/StackTest.cpp
          test/SurfaceTest.cpp test/DeviceSeams.h
  SUPPORT_DIRS ${SIGIL_SHADER_GENERATED_DIR}
               ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  DEFINITIONS SIGIL_WORLD_SHADER_DIR="${_shaders}"
  LIBRARIES SigilWorldDiligent SigilMaterialKit SigilGeometryKit
  LABELS gpu)

sigil_bench(world_diligent_bench
  SOURCES bench/DeviceBench.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
  LIBRARIES SigilWorldDiligent SigilMaterialKit SigilGeometryKit SigilMeasure)
