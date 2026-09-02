# Point operators and the clouds they act on: cloud storage, generators,
# modifiers, instancing and the billboard rasterizer, plus the pop chain
# language's field table, its CPU reference executor and the
# mesh-forming sinks — the swept operator among them, because carrying a
# profile along a rail is GPU-focused mesh work like the rest of it.
#
# AND THE DEVICE EXECUTORS BESIDE THEM, in device/: the chain cooked by
# dispatching its kernel, and the sweep's rings formed by dispatching
# theirs. A device executor stands beside the CPU executor of the seam it
# serves — the ABI the chain declares is honoured in the same library
# that declares it — and both are compiled only where a device exists to
# run on.

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

# THE RING KERNEL, the swept operator's own, compiled the same two ways.
sigil_slang_module(NAME Sweep
  SOURCE ${CMAKE_CURRENT_LIST_DIR}/kernels/Sweep.slang
  ENTRIES sigilSweepKernel
  OUT_VAR SIGIL_SWEEP_KERNEL_H
  CPP_VAR SIGIL_SWEEP_KERNEL_CPP
  SPIRV_VAR SIGIL_SWEEP_KERNEL_SPV_H)
sigil_slang_kernel_flags(${SIGIL_SWEEP_KERNEL_CPP})

# THE STAMPING KERNEL, the Copy operator's own, compiled the same two
# ways. It is what keeps a stamped point set from being formed by the
# host on the tier that has a device to form it on.
sigil_slang_module(NAME Stamp
  SOURCE ${CMAKE_CURRENT_LIST_DIR}/kernels/Stamp.slang
  ENTRIES sigilStampKernel
  OUT_VAR SIGIL_STAMP_KERNEL_H
  CPP_VAR SIGIL_STAMP_KERNEL_CPP
  SPIRV_VAR SIGIL_STAMP_KERNEL_SPV_H)
sigil_slang_kernel_flags(${SIGIL_STAMP_KERNEL_CPP})

# The device half, present only where a device feature was built.
if(TARGET SigilGeometryDevice)
  set(SIGIL_POP_DEVICE_SOURCES
      device/Cook.cpp
      device/Stamp.cpp
      device/Sweep.cpp)
else()
  set(SIGIL_POP_DEVICE_SOURCES)
endif()

add_library(SigilGeometryMeshPop STATIC
  ${SIGIL_POP_DEVICE_SOURCES}
  Billboards.cpp
  Cloud.cpp
  Cook.cpp
  Fields.cpp
  Generators.cpp
  Kernel.cpp
  Modifiers.cpp
  Sinks.cpp
  Spirv.cpp
  Stamp.cpp
  Sweep.cpp
  ${SIGIL_POP_KERNEL_CPP}
  ${SIGIL_POP_KERNEL_H}
  ${SIGIL_POP_KERNEL_SPV_H}
  ${SIGIL_STAMP_KERNEL_CPP}
  ${SIGIL_STAMP_KERNEL_H}
  ${SIGIL_STAMP_KERNEL_SPV_H}
  ${SIGIL_SWEEP_KERNEL_CPP}
  ${SIGIL_SWEEP_KERNEL_H}
  ${SIGIL_SWEEP_KERNEL_SPV_H}
  ${CMAKE_CURRENT_LIST_DIR}/kernels/Pop.slang
  ${CMAKE_CURRENT_LIST_DIR}/kernels/Stamp.slang
  ${CMAKE_CURRENT_LIST_DIR}/kernels/Sweep.slang
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/pop/Kernel.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/pop/Points.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/pop/Pop.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/pop/Spirv.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/pop/Stamp.h
  ${SIGIL_GEOMETRY_INCLUDE_DIR}/sigilgeometry/mesh/pop/Sweep.h
)
target_compile_features(SigilGeometryMeshPop PUBLIC cxx_std_20)
# The generated headers are PUBLIC because a runtime that dispatches the
# kernel on a device reads the words this build embedded rather than
# compiling the source a second time.
target_include_directories(SigilGeometryMeshPop
  PUBLIC ${SIGIL_GEOMETRY_INCLUDE_DIR} ${SIGIL_SLANG_GENERATED_DIR})
# The Runtime seams — the cook's and the sweep's — are comparable erased
# values, which is SigilCoreComparable's shape.
target_link_libraries(SigilGeometryMeshPop PUBLIC SigilGeometryMeshCurve
  SigilCoreComparable)
# The device executors dispatch the kernels this feature compiled, on the
# device SigilGeometryDevice created. Diligent arrives through that
# target and is never named beside it — see its CMakeLists for why the
# link order matters.
if(TARGET SigilGeometryDevice)
  target_link_libraries(SigilGeometryMeshPop PUBLIC SigilGeometryDevice)
endif()

# The pop tests seed chains from an imported model and save a cooked
# result, so this binary reaches into the codec as well.
if(TARGET SigilGeometryDevice)
  set(SIGIL_POP_DEVICE_TESTS test/DeviceCookTest.cpp test/DeviceStampTest.cpp
      test/DeviceSweepTest.cpp)
else()
  set(SIGIL_POP_DEVICE_TESTS)
endif()

add_executable(geometry_mesh_pop_test
  ${SIGIL_POP_DEVICE_TESTS}
  test/PointsTest.cpp
  test/PopTest.cpp
  test/RuntimeTest.cpp
  test/SweepTest.cpp
)
target_include_directories(geometry_mesh_pop_test PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../test)
target_link_libraries(geometry_mesh_pop_test PRIVATE SigilGeometryMeshPop
  SigilGeometryMeshCodec SigilGeometryKit GTest::gtest_main)
add_test(NAME geometry_mesh_pop_test COMMAND geometry_mesh_pop_test)

add_executable(geometry_mesh_pop_bench bench/PopBench.cpp)
target_link_libraries(geometry_mesh_pop_bench PRIVATE SigilGeometryMeshPop
  SigilGeometryKit benchmark::benchmark)
add_dependencies(benches geometry_mesh_pop_bench)
