# The mesh painter's device executor, beside the CPU one in the feature
# above: the same MeshStyle, the same geometry and the same canvas,
# rasterised on a device instead of sorted on the host.
#
# It is its own target and not part of the render feature, because the
# feature above must stay free of a device — a consumer that only draws
# meshes on the CPU links no renderer's dependencies — and because that
# is the shape every other seam here has: a device executor stands beside
# its CPU twin without pulling a device into it.
include("${CMAKE_SOURCE_DIR}/src/common/material/cmake/Slang.cmake")
set(_shaders ${CMAKE_CURRENT_LIST_DIR}/shaders)
# The painter imports the portable subset and the material kit's shading
# terms, so slangc is pointed at the two directories those live in. At
# run time the same text, embedded in this library, is loaded into the
# compiler session by name — and compiling it here as well makes a
# mistake in it a build failure rather than a first-frame surprise.
sigil_slang_module(NAME Painter SOURCE ${_shaders}/Painter.slang
  ENTRIES vsPaint fsPaint
  INCLUDE ${_shaders} ${SIGIL_MATERIAL_SLANG_SHADER_DIR}
          ${SIGIL_MATERIAL_TERMS_DIR}
  DEPENDS ${SIGIL_MATERIAL_SLANG_SHADER_DIR}/Portable.slang
          ${SIGIL_MATERIAL_TERMS_DIR}/Shading.slang
  PROOF_VAR SIGIL_GEOMETRY_PAINTER_PROOF)

# SigilGeometryDevice is PUBLIC because the entry point takes the device
# it creates, and SigilGeometryMeshRender because the value handed back
# is that seam's Runtime. SigilMaterialSlang carries the compiled program
# and the uniforms written into it, both named in the source alone;
# Diligent arrives through SigilGeometryDevice and is never named beside
# it, because that target's archive must come first on the link line.
#
# The draw itself holds the device's own resources and what is resident
# on it — the mesh's buffers, the map's texture, the pipeline built from
# the program — and those are spelled in the device feature's and the
# residency's own headers rather than in the library's public ones. Both
# directories are therefore on this target's PRIVATE include path and
# Painter.cpp includes them by their bare names; SigilGeometryDeviceResidency
# is PRIVATE for the same reason, since nothing of it reaches Painter.h.
sigil_library(SigilGeometryMeshRenderDevice
  SOURCES Painter.cpp ${SIGIL_GEOMETRY_PAINTER_PROOF}
  HEADERS Painter.h
  INCLUDE_PRIVATE ${SIGIL_GEOMETRY_DEVICE_PRIVATE_DIR}
                  ${SIGIL_GEOMETRY_DEVICE_RESIDENCY_PRIVATE_DIR}
  PUBLIC SigilGeometryDevice SigilGeometryMeshRender
  PRIVATE SigilGeometryDeviceResidency SigilMaterialSlang)
sigil_shader_sources(SigilGeometryMeshRenderDevice
  DIR ${_shaders} NAMESPACE sigil::geometry::mesh::render)

# LABELLED `gpu`: every case brings a Vulkan device up and skips with the
# reason when the machine has none. The device itself is
# SigilGeometryDevice's, whose test/support/OnDevice.h brings up ONE for
# the process, because that is where a device can be created at all.
sigil_test(geometry_test
  SOURCES test/PainterTest.cpp
  LIBRARIES SigilGeometryMeshRenderDevice SigilGeometryKit
  SUPPORT_DIRS ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  SUITES Painter LABELS gpu)
