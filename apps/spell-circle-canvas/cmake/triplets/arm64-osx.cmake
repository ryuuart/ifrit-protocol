# workaround: the vcpkg usd port links a static TBB into each of its
# dylibs.
#
# The stock arm64-osx triplet (static libraries), with ONE exception:
# oneTBB builds as a shared library. OpenUSD ships as many dylibs and
# links TBB into each; a static TBB gives every dylib its own runtime,
# and a task enqueued in one runtime waits forever for workers idling in
# another — the first UsdStage::Open hangs. A shared TBB is the single
# runtime they all expect.
#
# One port's settings are varied by testing PORT, which is where vcpkg
# means per-port customization to live.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

if(PORT STREQUAL "tbb")
  set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
