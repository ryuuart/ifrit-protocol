# The device feature: the one GPU device 2D and 3D share, and what every
# executor standing on it holds.
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
enable_language(C)

find_package(unofficial-diligent-engine CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG REQUIRED)

add_library(SigilGeometryDevice STATIC
  AdoptDevice.cpp
  AdoptDevice.h
  Device.cpp
  Resources.cpp
  VolkShim.c
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/device/Device.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/device/Resources.h
)
target_compile_features(SigilGeometryDevice PUBLIC cxx_std_20)
target_include_directories(SigilGeometryDevice
  PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR}
  PRIVATE ${CMAKE_CURRENT_LIST_DIR})
# Diligent is PUBLIC because Resources.h names its buffer, sampler and
# texture interfaces: an executor holding one of these is writing
# Diligent calls. SigilCoreHardware and SigilSkiaGraphite are PUBLIC
# because Device.h hands out the adopted device and the Graphite context
# standing on it. volk and the Vulkan headers are PRIVATE — the public
# headers spell no Vulkan type at all.
target_link_libraries(SigilGeometryDevice
  PUBLIC unofficial::diligent-engine::DiligentCore SigilCoreHardware
         SigilSkiaGraphite
  PRIVATE unofficial::diligent-engine::volk Vulkan::Headers)

# NOTHING DOWNSTREAM NAMES DiligentCore ON ITS OWN LINE. This archive
# defines every volk symbol (VolkShim.c), and the engine's archive
# defines them too; whichever comes first on the link line wins, and only
# this one finds a Homebrew MoltenVK. Reaching Diligent through this
# target is what puts this archive first — naming the engine directly
# beside it puts the engine's own volk ahead and the device then fails to
# come up on exactly the machines the shim exists for.
add_executable(geometry_device_test test/DeviceTest.cpp)
target_link_libraries(geometry_device_test PRIVATE SigilGeometryDevice
  GTest::gtest_main)
add_test(NAME geometry_device_test COMMAND geometry_device_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(geometry_device_bench bench/DeviceBench.cpp)
target_include_directories(geometry_device_bench PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(geometry_device_bench PRIVATE SigilGeometryDevice
  SigilMeasure benchmark::benchmark)
add_dependencies(benches geometry_device_bench)
