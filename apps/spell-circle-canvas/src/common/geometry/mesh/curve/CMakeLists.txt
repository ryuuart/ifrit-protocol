# Splines, their parallel-transport frames and the geometry swept along
# them; projection to a 2D path needs the camera.

# THE RING KERNEL, compiled twice from one source: to the C++ the
# built-in executor calls, and to the SPIR-V a runtime that owns a device
# dispatches. Neither side re-derives a formula, which is what lets two
# tiers be held to bit identity rather than to a tolerance. The C++ is
# compiled with the float model pinned — see cmake/Slang.cmake.
include(Slang)
sigil_slang_module(NAME Sweep
  SOURCE ${CMAKE_CURRENT_LIST_DIR}/kernels/Sweep.slang
  ENTRIES sigilSweepKernel
  OUT_VAR SIGIL_SWEEP_KERNEL_H
  CPP_VAR SIGIL_SWEEP_KERNEL_CPP
  SPIRV_VAR SIGIL_SWEEP_KERNEL_SPV_H)
sigil_slang_kernel_flags(${SIGIL_SWEEP_KERNEL_CPP})

add_library(SigilGeometryMeshCurve STATIC
  Curve.cpp
  Pose.cpp
  Sweep.cpp
  ${SIGIL_SWEEP_KERNEL_CPP}
  ${SIGIL_SWEEP_KERNEL_H}
  ${SIGIL_SWEEP_KERNEL_SPV_H}
  ${CMAKE_CURRENT_LIST_DIR}/kernels/Sweep.slang
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/curve/Curve.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/curve/Frame.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/curve/Pose.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/curve/Sweep.h
)
target_compile_features(SigilGeometryMeshCurve PUBLIC cxx_std_20)
# The generated headers are PUBLIC because a runtime that dispatches the
# kernel on a device reads the words this build embedded rather than
# compiling the source a second time.
target_include_directories(SigilGeometryMeshCurve
  PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR} ${SIGIL_SLANG_GENERATED_DIR})
# The SweepRuntime seam is a comparable erased value, which is
# SigilCoreComparable's shape.
target_link_libraries(SigilGeometryMeshCurve PUBLIC SigilGeometryMesh
  SigilGeometryMeshCamera SigilCoreComparable)

add_executable(geometry_mesh_curve_test test/CurveTest.cpp test/SweepTest.cpp)
target_link_libraries(geometry_mesh_curve_test PRIVATE SigilGeometryMeshCurve
  GTest::gtest_main)
add_test(NAME geometry_mesh_curve_test COMMAND geometry_mesh_curve_test)

add_executable(geometry_mesh_curve_bench bench/CurveBench.cpp)
target_link_libraries(geometry_mesh_curve_bench PRIVATE SigilGeometryMeshCurve
  benchmark::benchmark)
add_dependencies(benches geometry_mesh_curve_bench)
