# The device executors of the three seams in the feature above: the
# chain cooked by dispatching its kernel, the sweep's rings formed by
# dispatching theirs, and a stamping's vertices formed by dispatching the
# third. A device executor stands beside the CPU executor of the seam it
# serves — the ABI each seam declares is honoured in the library that
# declares it.
#
# It is its own target and not part of the feature above, because that
# feature must stay free of a device — a consumer that only cooks a
# chain on the host links no Diligent, no Vulkan loader and no
# Graphite — and because that is the shape every other seam here has.
#
# SigilGeometryDevice is PUBLIC because each entry point takes the
# device it creates, and SigilGeometryMeshPop because the values handed
# back are those seams' Runtimes. The kernels themselves were compiled by
# the feature above and reach the dispatches through its target, so
# nothing is compiled twice; Diligent arrives through
# SigilGeometryDevice and is never named beside it, because that target's
# archive must come first on the link line.
sigil_library(SigilGeometryMeshPopDevice
  SOURCES Cook.cpp Stamp.cpp Sweep.cpp
  HEADERS Cook.h Stamp.h Sweep.h
  PUBLIC SigilGeometryDevice SigilGeometryMeshPop)

# THE DEVICE TIER, labelled `gpu`: every case here brings a Vulkan device
# up and skips with the reason when the machine has none, and what they
# judge — that two executors of one kernel agree bit for bit, and that
# the backend says nothing while they do — is not what the host cases
# judge. A machine with no device runs `ctest -LE gpu` and checks the
# host tier alone.
sigil_test(geometry_test
  SOURCES test/DeviceCookTest.cpp test/DeviceStampTest.cpp
          test/DeviceSweepTest.cpp
  LIBRARIES SigilGeometryMeshPopDevice SigilGeometryMeshCodec
            SigilGeometryKit
  SUPPORT_DIRS ${SIGIL_GEOMETRY_DEVICE_TEST_SUPPORT_DIR}
  SUITES DevicePop DeviceStamp DeviceSweep LABELS gpu)
