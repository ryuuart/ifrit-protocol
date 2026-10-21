# The frame feature: a frame as its declared things — the scene, the
# environment, the camera it names, the passes and the readbacks — the
# resources those passes write, the seam a texture pass paints through,
# and the
# Runtime/Executor seam a pass is performed through. The executor that
# draws is the device's, supplied as a value by the feature that owns a
# device, so nothing here links one and every dependency points down.
# SigilMaterialTexture because a body's view carries the map its surface
# is dressed with; SigilCoreReconcile because a pass body and a generator
# are its erased value. A texture is read into a picture through the Skia
# executor, privately.
find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilWorldFrame
  SOURCES Frame.cpp Pass.cpp Targets.cpp TextureProducer.cpp View.cpp
  HEADERS Environment.h Frame.h Pass.h ../advanced/Runtime.h
          ../advanced/TextureProducer.h ../advanced/View.h ../advanced/Skia.h
  PUBLIC SigilWorldNode SigilMaterialTexture SigilGeometryMeshCamera
         SigilMediaCore
         SigilCoreCallable SigilCoreReconcile Boost::container
  PRIVATE SigilMaterialSkia)

# Links only the library it exercises, so a link edge that pulled the
# retained side or a device in would show up as a build failure here.
sigil_test(world_test
  SOURCES test/FrameTest.cpp test/SamplingTest.cpp
  LIBRARIES SigilWorldFrame)

sigil_bench(world_bench
  SOURCES bench/FrameBench.cpp
  LIBRARIES SigilWorldFrame)
