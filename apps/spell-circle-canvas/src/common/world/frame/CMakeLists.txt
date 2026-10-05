# The frame feature: a frame as three declared things — the scene, the
# passes and the readbacks — the resources those passes write, and the
# Runtime/Executor seam a pass is performed through. The executor that
# draws is the device's, supplied as a value by the feature that owns a
# device, so nothing here links one and every dependency points down.
# SigilMaterialTexture because a body's view carries the map its surface
# is dressed with; SigilCoreReconcile because a pass body and a generator
# are its erased value. A texture is read into a picture through the Skia
# executor, privately.
find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilWorldFrame
  SOURCES Frame.cpp Pass.cpp Targets.cpp View.cpp
  HEADERS Frame.h Pass.h Runtime.h View.h ../advanced/Skia.h
  PUBLIC SigilWorldElement SigilMaterialTexture SigilGeometryMeshCamera
         SigilCoreCallable SigilCoreReconcile Boost::container
  PRIVATE SigilMaterialSkia)

# Links only the library it exercises, so a link edge that pulled the
# retained side or a device in would show up as a build failure here.
sigil_test(world_test
  SOURCES test/FrameTest.cpp
  LIBRARIES SigilWorldFrame)

sigil_bench(world_bench
  SOURCES bench/FrameBench.cpp
  LIBRARIES SigilWorldFrame)
