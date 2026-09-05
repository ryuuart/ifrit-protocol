# What STANDS ON THE DEVICE between one draw and the next: a mesh as
# buffers, a map as a texture, and a compiled program as a pipeline.
#
# RESIDENCY IS THE DEVICE'S, not a renderer's. A mesh is host memory and
# a map is an image; a draw needs buffers and textures. The crossing has
# one vertex layout, one lifetime rule and one cache, and two executors
# sharing a device share the crossing rather than each copying every mesh
# and every map a second time. The same is true of a pipeline: what
# distinguishes two draws of one compiled program is how they blend and
# whether they depth-test, and building that twice from two renderers
# would only mean two caches. Nothing here decides a picture.
#
# IT IS A SIBLING OF THE DEVICE AND NOT PART OF IT, because of what it
# has to link. A mesh is SigilGeometryMesh's currency, a map is
# SigilMaterial's texture and a pipeline is built out of SigilMaterial's
# Slang backend — and the feature that CREATES the device sits under
# every consumer of one, the point operators' device executors among
# them. A point operator dispatches a kernel and forms no picture, so its
# link line has no business reaching a material. Standing residency
# beside the device instead of inside it is what keeps that true: a
# consumer takes the device, or the device and what is resident on it.
#
# Diligent arrives through SigilGeometryDevice and is never named beside
# it, because that target's archive must come first on the link line. The
# device feature's own headers are on the private include path for
# Resources.h, which a binding is written through. The Vulkan headers are
# here because a map whose pixels already stand on this device is wrapped
# through the engine's Vulkan interface, which names Vulkan's handle
# types without declaring them.
#
# THE HEADERS ARE THIS FEATURE'S OWN, not the library's: each names the
# engine's buffer, texture, pipeline or binding interfaces, so a consumer
# holding one of those objects reaches them the same way the device
# feature's are reached — by putting the directory below on its own
# PRIVATE include path.
find_package(VulkanHeaders CONFIG REQUIRED)
sigil_library(SigilGeometryDeviceResidency
  SOURCES Meshes.cpp Meshes.h Pipelines.cpp Pipelines.h Textures.cpp
          Textures.h
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${SIGIL_GEOMETRY_DEVICE_PRIVATE_DIR}
  PUBLIC SigilGeometryDevice SigilGeometryMesh SigilMaterialTexture
         SigilMaterialSlang
  PRIVATE Vulkan::Headers)

set(SIGIL_GEOMETRY_DEVICE_RESIDENCY_PRIVATE_DIR ${CMAKE_CURRENT_LIST_DIR}
    CACHE INTERNAL "the residency feature's own headers, which spell Diligent")

# LABELLED `gpu`: every case here brings a Vulkan device up and skips with
# the reason when the machine has none, so a machine without one runs
# `ctest -LE gpu` and checks nothing it cannot judge.
sigil_test(geometry_test
  SOURCES test/ResidencyTest.cpp
  LIBRARIES SigilGeometryDeviceResidency
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR} ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  SUITES MapUpload MeshResidency TextureResidency LABELS gpu)
