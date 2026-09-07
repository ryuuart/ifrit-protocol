# Splines, their parallel-transport frames, and the pose read off one by
# distance; projection to a 2D path needs the camera. What a rail
# CARRIES is a point operator and lives with them, in mesh/pop.
sigil_library(SigilGeometryMeshCurve
  SOURCES Curve.cpp Pose.cpp
  HEADERS Curve.h Frame.h Pose.h
  PUBLIC SigilGeometryMesh SigilGeometryMeshCamera)

sigil_test(geometry_test
  SOURCES test/CurveTest.cpp
  LIBRARIES SigilGeometryMeshCurve)

sigil_bench(geometry_bench
  SOURCES bench/CurveBench.cpp
  LIBRARIES SigilGeometryMeshCurve)
