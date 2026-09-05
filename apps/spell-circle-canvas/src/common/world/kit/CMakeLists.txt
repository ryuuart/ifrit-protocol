# The kit feature: presets that compose elements — a three-point rig, a
# turntable, and the set both make over a ground plane. Everything here
# returns an ordinary Element built out of the verbs a tree is already
# written in, so a caller reads, replaces or ignores any part of one.
#
# It links the description and SigilMaterial's kit, because a ground
# plane given no surface has to be made of something; it defines no
# shading model, no catalog and no material of its own. The turntable's
# phase along its rail is a motion value, read in the source alone.
# Nothing puts a world source directory on this target's include path,
# so the retained side's own headers are unreachable from kit code.
sigil_library(SigilWorldKit
  SOURCES Kit.cpp
  HEADERS Kit.h
  PUBLIC SigilWorldElement SigilMaterialKit SigilGeometryKit
  PRIVATE SigilMotionValues)

# Links only the library it exercises, so a link edge that pulled the
# retained side or a device in would show up as a build failure here.
sigil_test(world_test
  SOURCES test/KitTest.cpp
  LIBRARIES SigilWorldKit)

sigil_bench(world_bench
  SOURCES bench/KitBench.cpp
  LIBRARIES SigilWorldKit)
