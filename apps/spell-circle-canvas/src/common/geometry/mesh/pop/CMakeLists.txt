# Point operators and the clouds they act on: cloud storage, generators,
# modifiers, instancing and the billboard rasterizer, plus the pop chain
# language's field table, its CPU reference executor and the
# mesh-forming sinks — the swept operator among them, because carrying a
# profile along a rail is GPU-focused mesh work like the rest of it.
#
# THE DEVICE EXECUTORS ARE NOT HERE. They stand in device/, as their own
# target, because this feature must stay free of a device: a consumer
# that only cooks a chain on the host links no Diligent, no Vulkan
# loader and no Graphite. That is the shape every other seam here has.

# THE KERNEL, compiled twice from one source: to the C++ this library's
# executor calls, and to the SPIR-V a runtime that owns a device
# dispatches. Neither side re-derives a formula, which is what lets two
# tiers be held to bit identity rather than to a tolerance. The C++ is
# compiled with the float model pinned by the Slang module rule.
include("${CMAKE_SOURCE_DIR}/src/common/material/cmake/Slang.cmake")
sigil_slang_module(NAME Pop
  SOURCE ${CMAKE_CURRENT_LIST_DIR}/kernels/Pop.slang
  ENTRIES sigilPopKernel
  CPP_VAR SIGIL_POP_KERNEL_CPP
  SPIRV_VAR SIGIL_POP_KERNEL_SPV_H)
sigil_slang_kernel_flags(${SIGIL_POP_KERNEL_CPP})

# THE RING KERNEL, the swept operator's own, compiled the same two ways.
sigil_slang_module(NAME Sweep
  SOURCE ${CMAKE_CURRENT_LIST_DIR}/kernels/Sweep.slang
  ENTRIES sigilSweepKernel
  CPP_VAR SIGIL_SWEEP_KERNEL_CPP
  SPIRV_VAR SIGIL_SWEEP_KERNEL_SPV_H)
sigil_slang_kernel_flags(${SIGIL_SWEEP_KERNEL_CPP})

# THE STAMPING KERNEL, the Copy operator's own, compiled the same two
# ways. It is what keeps a stamped point set from being formed by the
# host on the tier that has a device to form it on.
sigil_slang_module(NAME Stamp
  SOURCE ${CMAKE_CURRENT_LIST_DIR}/kernels/Stamp.slang
  ENTRIES sigilStampKernel
  CPP_VAR SIGIL_STAMP_KERNEL_CPP
  SPIRV_VAR SIGIL_STAMP_KERNEL_SPV_H)
sigil_slang_kernel_flags(${SIGIL_STAMP_KERNEL_CPP})

# The Runtime seams — the cook's, the sweep's and the stamping's — are
# comparable erased values, which is SigilCoreComparable's shape. The
# seeded mixers the scatter and the generators deal from are
# SigilCoreCompute's, read in the sources alone. The built-in executor
# divides its passes through SigilCoreSchedule, which is where every
# divided range in this repository is decided.
find_package(boost_container CONFIG REQUIRED)
sigil_library(SigilGeometryMeshPop
  SOURCES Billboards.cpp Cloud.cpp Cook.cpp Fields.cpp Generators.cpp
          Kernel.cpp Modifiers.cpp Sinks.cpp Stamp.cpp
          Sweep.cpp
          ${SIGIL_POP_KERNEL_CPP} ${SIGIL_POP_KERNEL_SPV_H}
          ${SIGIL_STAMP_KERNEL_CPP} ${SIGIL_STAMP_KERNEL_SPV_H}
          ${SIGIL_SWEEP_KERNEL_CPP} ${SIGIL_SWEEP_KERNEL_SPV_H}
          kernels/Pop.slang kernels/Stamp.slang kernels/Sweep.slang
  HEADERS Builder.h Kernel.h Ops.h Points.h Pop.h Runtime.h Sinks.h
          Stamp.h Sweep.h
  PUBLIC SigilGeometryMeshCurve SigilCoreComparable Boost::container
  PRIVATE SigilCoreCompute SigilCoreSchedule)
# The generated headers are PUBLIC because a runtime that dispatches the
# kernel on a device reads the words this build embedded rather than
# compiling the source a second time.
target_include_directories(SigilGeometryMeshPop PUBLIC
  ${SIGIL_SLANG_GENERATED_DIR})

# The pop tests seed chains from an imported model and save a cooked
# result, so this binary reaches into the codec as well.
sigil_test(geometry_test
  SOURCES test/PointsTest.cpp test/PopChainsTest.cpp test/PopFieldsTest.cpp
          test/PopFiltersTest.cpp test/PopLanesTest.cpp
          test/PopNeighboursTest.cpp
          test/PopSelectionTest.cpp test/PopSinksTest.cpp
          test/RuntimeTest.cpp test/SweepTest.cpp test/SweptShapesTest.cpp
  LIBRARIES SigilGeometryMeshPop SigilGeometryMeshCodec SigilGeometryKit
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}/test/support)

sigil_bench(geometry_bench
  SOURCES bench/PopBench.cpp
  LIBRARIES SigilGeometryMeshPop SigilGeometryKit)

# …and the device executors of these three seams, their own target so
# that this one keeps its promise to know nothing about a device.
add_subdirectory(device)
