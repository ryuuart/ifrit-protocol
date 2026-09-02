# Point operators and the clouds they act on: cloud storage, generators,
# modifiers, instancing and the billboard rasterizer, plus the pop chain
# language's field table, its CPU reference executor and the
# mesh-forming sinks.

# THE KERNEL, compiled twice from one source: to the C++ this library's
# executor calls, and to the SPIR-V a runtime that owns a device
# dispatches. Neither side re-derives a formula, which is what lets two
# tiers be held to bit identity rather than to a tolerance. The C++ is
# compiled with the float model pinned — see cmake/Slang.cmake.
include(Slang)
sigil_slang_module(NAME Pop
  SOURCE ${CMAKE_CURRENT_LIST_DIR}/kernels/Pop.slang
  ENTRIES sigilPopKernel
  OUT_VAR SIGIL_POP_KERNEL_H
  CPP_VAR SIGIL_POP_KERNEL_CPP
  SPIRV_VAR SIGIL_POP_KERNEL_SPV_H)
sigil_slang_kernel_flags(${SIGIL_POP_KERNEL_CPP})

add_library(SigilGeometryMeshPop STATIC
  Billboards.cpp
  Cloud.cpp
  Cook.cpp
  Fields.cpp
  Generators.cpp
  Kernel.cpp
  Modifiers.cpp
  Sinks.cpp
  Spirv.cpp
  ${SIGIL_POP_KERNEL_CPP}
  ${SIGIL_POP_KERNEL_H}
  ${SIGIL_POP_KERNEL_SPV_H}
  ${CMAKE_CURRENT_LIST_DIR}/kernels/Pop.slang
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/pop/Kernel.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/pop/Points.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/pop/Pop.h
)
target_compile_features(SigilGeometryMeshPop PUBLIC cxx_std_20)
# The generated headers are PUBLIC because a runtime that dispatches the
# kernel on a device reads the words this build embedded rather than
# compiling the source a second time.
target_include_directories(SigilGeometryMeshPop
  PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR} ${SIGIL_SLANG_GENERATED_DIR})
# The Runtime seam is a comparable erased value, which is
# SigilCoreComparable's shape.
target_link_libraries(SigilGeometryMeshPop PUBLIC SigilGeometryMeshCurve
  SigilCoreComparable)

# The pop tests seed chains from an imported model and save a cooked
# result, so this binary reaches into the codec as well.
add_executable(geometry_mesh_pop_test
  test/PointsTest.cpp
  test/PopTest.cpp
  test/RuntimeTest.cpp
)
target_include_directories(geometry_mesh_pop_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../test)
target_link_libraries(geometry_mesh_pop_test PRIVATE SigilGeometryMeshPop
  SigilGeometryMeshCodec SigilGeometryKit GTest::gtest_main)
add_test(NAME geometry_mesh_pop_test COMMAND geometry_mesh_pop_test)

add_executable(geometry_mesh_pop_bench bench/PopBench.cpp)
target_link_libraries(geometry_mesh_pop_bench PRIVATE SigilGeometryMeshPop
  SigilGeometryKit benchmark::benchmark)
add_dependencies(benches geometry_mesh_pop_bench)
