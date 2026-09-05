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
# that declares it.

# THE KERNEL, compiled twice from one source: to the C++ this library's
# executor calls, and to the SPIR-V a runtime that owns a device
# dispatches. Neither side re-derives a formula, which is what lets two
# tiers be held to bit identity rather than to a tolerance. The C++ is
# compiled with the float model pinned by the Slang module rule.
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

# The Runtime seams — the cook's and the sweep's — are comparable erased
# values, which is SigilCoreComparable's shape. The seeded mixers the
# scatter and the generators deal from are SigilCoreCompute's, read in
# the sources alone. The device executors dispatch the kernels this
# feature compiled, on the device SigilGeometryDevice created; Diligent
# arrives through that target and is never named beside it, because that
# target's archive must come first on the link line.
find_package(TBB CONFIG REQUIRED)
sigil_library(SigilGeometryMeshPop
  SOURCES device/Cook.cpp device/Stamp.cpp device/Sweep.cpp
          Billboards.cpp Cloud.cpp Cook.cpp Fields.cpp Generators.cpp
          Kernel.cpp Modifiers.cpp Parallel.h Sinks.cpp Spirv.cpp Stamp.cpp
          Sweep.cpp
          ${SIGIL_POP_KERNEL_CPP} ${SIGIL_POP_KERNEL_H}
          ${SIGIL_POP_KERNEL_SPV_H}
          ${SIGIL_STAMP_KERNEL_CPP} ${SIGIL_STAMP_KERNEL_H}
          ${SIGIL_STAMP_KERNEL_SPV_H}
          ${SIGIL_SWEEP_KERNEL_CPP} ${SIGIL_SWEEP_KERNEL_H}
          ${SIGIL_SWEEP_KERNEL_SPV_H}
          kernels/Pop.slang kernels/Stamp.slang kernels/Sweep.slang
  HEADERS Kernel.h Points.h Pop.h Spirv.h Stamp.h Sweep.h
  PUBLIC SigilGeometryMeshCurve SigilGeometryDevice SigilCoreComparable
         Boost::container
  PRIVATE SigilCoreCompute TBB::tbb)
# The generated headers are PUBLIC because a runtime that dispatches the
# kernel on a device reads the words this build embedded rather than
# compiling the source a second time.
target_include_directories(SigilGeometryMeshPop PUBLIC
  ${SIGIL_SLANG_GENERATED_DIR})

# The pop tests seed chains from an imported model and save a cooked
# result, so this binary reaches into the codec as well.
sigil_test(geometry_mesh_pop_test
  SOURCES test/PointsTest.cpp test/PopTest.cpp test/RuntimeTest.cpp
          test/SweepTest.cpp
  LIBRARIES SigilGeometryMeshPop SigilGeometryMeshCodec SigilGeometryKit)

# THE DEVICE TIER, its own binary and labelled `gpu`: every case here
# brings a Vulkan device up and skips with the reason when the machine
# has none, and what they judge — that two executors of one kernel agree
# bit for bit, and that the backend says nothing while they do — is not
# what the cases above judge. A machine with no device runs
# `ctest -LE gpu` and checks the host tier alone.
sigil_test(geometry_mesh_pop_device_test
  SOURCES test/DeviceCookTest.cpp test/DeviceStampTest.cpp
          test/DeviceSweepTest.cpp
  LIBRARIES SigilGeometryMeshPop SigilGeometryMeshCodec SigilGeometryKit
  LABELS gpu)

sigil_bench(geometry_mesh_pop_bench
  SOURCES bench/PopBench.cpp
  LIBRARIES SigilGeometryMeshPop SigilGeometryKit)
