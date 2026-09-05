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
# ONE PUBLIC HEADER, AND IT NAMES NO DILIGENT TYPE IT DOES NOT DECLARE.
# Device.h forward-declares the render device and the context it hands
# out, and includes nothing of the engine's. The four beside it —
# Resources.h, Meshes.h, Pipelines.h, Textures.h — spell the engine's
# buffer, sampler, texture, pipeline and binding interfaces, so they are
# this feature's own headers and not the library's: they sit in this
# directory, and a consumer holding one of these is writing Diligent
# calls and says so by putting this directory on its own PRIVATE include
# path. SIGIL_GEOMETRY_DEVICE_PRIVATE_DIR below is that one door; the
# world's frame executor is what walks through it.
#
# Diligent is PUBLIC all the same, because Device.h's two doors hand out
# objects the caller then makes engine calls on, and because of the link
# order below. SigilCoreHardware and SigilSkiaGraphite are PUBLIC because
# Device.h hands out the adopted device and the Graphite context standing
# on it. volk and the Vulkan headers are PRIVATE — no header here spells
# a Vulkan type at all.
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
  SOURCES AdoptDevice.cpp AdoptDevice.h Device.cpp Meshes.cpp Meshes.h
          Pipelines.cpp Pipelines.h Resources.cpp Resources.h Textures.cpp
          Textures.h VolkShim.c
  HEADERS Device.h
  INCLUDE_PRIVATE ${CMAKE_CURRENT_LIST_DIR}
  PUBLIC unofficial::diligent-engine::DiligentCore SigilCoreHardware
         SigilSkiaGraphite SigilGeometryMesh SigilMaterialTexture
         SigilMaterialSlang
  PRIVATE unofficial::diligent-engine::volk Vulkan::Headers)

# THE ONE DOOR ONTO THE ENGINE'S INTERFACES: the directory holding the
# headers that spell them. A consumer that genuinely holds a Diligent
# buffer, texture or pipeline — the world's frame executor does, and the
# mesh painter's device executor does — puts this on its own PRIVATE
# include path and includes the header by its bare name. Anything that
# only wants a device includes <sigilgeometry/device/Device.h> and needs
# none of this.
set(SIGIL_GEOMETRY_DEVICE_PRIVATE_DIR ${CMAKE_CURRENT_LIST_DIR}
    CACHE INTERNAL "the device feature's own headers, which spell Diligent")

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
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/test/support
               ${SIGIL_SKIA_GRAPHITE_TEST_SUPPORT_DIR}
  LABELS gpu)

sigil_bench(geometry_device_bench
  SOURCES bench/DeviceBench.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
  LIBRARIES SigilGeometryDevice SigilMeasure)
