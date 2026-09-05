# The device feature: the one GPU device 2D and 3D share, and what every
# executor standing on it holds — the shared resources, the meshes
# resident on it, and the maps.
#
# RESIDENCY IS THE DEVICE'S, not a renderer's. A mesh is host memory and
# a map is an image; a draw needs buffers and textures. The crossing has
# one vertex layout, one lifetime rule and one cache, and two executors
# sharing a device share the crossing rather than each copying every
# mesh and every map a second time. The same is true of a pipeline: what
# distinguishes two draws of one compiled program is how they blend and
# whether they depth-test, and building that twice from two renderers
# would only mean two caches. That is why the mesh currency, the
# material texture and SigilMaterial's Slang backend are linked here:
# they are what gets made resident and what a pipeline is built out of,
# and nothing here reads any of them for anything else. Nothing here
# decides a picture.
#
# THE CREATION POINT SITS BELOW EVERY CONSUMER, and it has to. Diligent
# creates the Vulkan device — this build of it cannot attach to a device
# that already exists, and has no Metal backend — so a library that wants
# a Diligent device cannot be handed one made elsewhere. That is why the
# bring-up lives here, under the point operators, the mesh painter and
# the frame runtime alike, rather than in whichever of them happened to
# need it first. SigilCoreHardware adopts what Diligent made, so there is
# one device, one queue and one handle table under both APIs, and
# Graphite records onto that same queue.
#
# VolkShim.c compiles volk with a Homebrew-aware volkInitialize, so
# /opt/homebrew MoltenVK installs work without DYLD environment surgery.
# unofficial::diligent-engine::volk is what it compiles against: volk's
# headers under the same platform definitions the engine's own copy was
# built with, without which the surface entry point the engine calls is
# missing here and that copy is linked in beside this one. It is the
# loader for Diligent alone: the hardware device resolves its own entry
# points and links nothing, and AdoptDevice.cpp passes volk's
# vkGetInstanceProcAddr on to it so both APIs dispatch through the one
# loader this process opened.
#
# Diligent is PUBLIC because Resources.h names its buffer, sampler and
# texture interfaces: an executor holding one of these is writing
# Diligent calls. SigilCoreHardware and SigilSkiaGraphite are PUBLIC
# because Device.h hands out the adopted device and the Graphite context
# standing on it. volk and the Vulkan headers are PRIVATE — the public
# headers spell no Vulkan type at all.
#
# NOTHING DOWNSTREAM NAMES DiligentCore ON ITS OWN LINE. This archive
# defines every volk symbol (VolkShim.c), and the engine's archive
# defines them too; whichever comes first on the link line wins, and only
# this one finds a Homebrew MoltenVK. Reaching Diligent through this
# target is what puts this archive first — naming the engine directly
# beside it puts the engine's own volk ahead and the device then fails to
# come up on exactly the machines the shim exists for.
enable_language(C)


sigil_library(SigilGeometryDevice
  SOURCES AdoptDevice.cpp AdoptDevice.h Device.cpp Meshes.cpp Pipelines.cpp
          Resources.cpp Textures.cpp VolkShim.c
  HEADERS Device.h Meshes.h Pipelines.h Resources.h Textures.h
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR}
  PUBLIC unofficial::diligent-engine::DiligentCore SigilCoreHardware
         SigilSkiaGraphite SigilGeometryMesh SigilMaterialTexture
         SigilMaterialSlang
  PRIVATE unofficial::diligent-engine::volk Vulkan::Headers)

# The device fixture every binary with device cases reads: one device per
# process, and the reason there is none. Named so a consumer in another
# directory can put it on its include path.
set(SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR ${CMAKE_CURRENT_LIST_DIR}/test/support
    CACHE INTERNAL "the shared Vulkan device fixture")

# LABELLED `gpu`: every case here brings a Vulkan device up and skips with
# the reason when the machine has none, so a machine without one runs
# `ctest -LE gpu` and checks nothing it cannot judge.
sigil_test(geometry_device_test
  SOURCES test/DeviceTest.cpp test/ResidencyTest.cpp
  LIBRARIES SigilGeometryDevice
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}/test/support
               ${SIGIL_SKIA_GRAPHITE_TEST_SUPPORT_DIR}
  LABELS gpu)

sigil_bench(geometry_device_bench
  SOURCES bench/DeviceBench.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
  LIBRARIES SigilGeometryDevice SigilMeasure)
