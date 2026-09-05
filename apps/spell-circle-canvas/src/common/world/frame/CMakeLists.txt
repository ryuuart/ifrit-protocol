# The frame feature: a frame as three declared things — the scene, the
# passes and the readbacks — the resources those passes write, and the
# Runtime/Executor seam a pass is performed through. The built-in
# executor is the CPU one; a feature that owns a device supplies its own
# as a value, so nothing here links a device and every dependency points
# down. SigilMaterialTexture because a body's view carries the map its
# surface is dressed with, which every executor samples;
# SigilCoreReconcile because a pass body and a generator are its erased
# value. The CPU executor cooks a body's chain itself, so the point
# operators are named privately.
find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilWorldFrame
  SOURCES Cpu.cpp CpuGeometry.cpp CpuPost.cpp Frame.cpp Pass.cpp Targets.cpp
          View.cpp Cpu.h
  HEADERS Frame.h Pass.h Runtime.h Targets.h View.h
  PUBLIC SigilWorldElement SigilMaterialTexture SigilGeometryMeshRender
         SigilCoreReconcile Boost::container
  PRIVATE SigilGeometryMeshPop)

# Links only the library it exercises, so a link edge that pulled the
# retained side or a device in would show up as a build failure here.
sigil_test(world_frame_test
  SOURCES test/FrameTest.cpp
  LIBRARIES SigilWorldFrame)

sigil_bench(world_frame_bench
  SOURCES bench/FrameBench.cpp
  LIBRARIES SigilWorldFrame)
