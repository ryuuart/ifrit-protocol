# SigilCore — the kernels a retained runtime is built on and the leaves
# under them, as one feature library per directory. README.md is the
# canon.
#
#   SigilCoreComparable  comparable/  what a value needs before anything
#                                     can decide it did not change:
#                                     comparable type erasure and the
#                                     field pin
#   SigilCoreCompute     compute/     the arithmetic several libraries
#                                     agree on to the bit: the seeded
#                                     mixers and the identifying folds
#   SigilCoreSchedule    schedule/    where independent work runs: one
#                                     parallel for over the task runtime,
#                                     and a fan-out of its own threads for
#                                     calls that block on a disk or a
#                                     server
#   SigilCoreReconcile   reconcile/   descriptions reconciled onto a host's
#                                     retained tree: keyed matching, the
#                                     memo, the identity prune, the
#                                     inherited-value channel, the animation
#                                     lanes and the phase runner
#   SigilCoreCache       cache/       what a host may keep between frames:
#                                     the cache policy, the settled-subtree
#                                     proof, the stability release and the
#                                     bake seam
#   SigilCoreHardware    hardware/    the GPU device itself: owned or
#                                     adopted, its textures and fences
#                                     named by generation-checked handles,
#                                     destruction deferred past the frames
#                                     in flight
#
# The schedule feature is the one every library that divides work over
# threads links, so that a task runtime is named in one file of this
# repository and no header of it spells one.
#
# The two leaves depend on the standard library alone (Boost.PFR aside)
# and are linked from libraries far outside this one, which is what makes
# them leaves rather than parts of the kernels above them. The hardware
# feature is the one exception to std-only: it is the graphics API and
# nothing else — no Skia, no Diligent, no Qt — so a drawing library stands
# on the device it hands out rather than owning one, and two of them stand
# on the same one. Only a consumer that needs a device links it.
sigil_library_root(SigilCore
  BRIEF "The kernels a retained runtime is built on — the reconciler (keyed matching, memo, the identity prune, inherited values, animation lanes and phases) and the caching proof (policy, settled subtrees, the stability release and the bake seam) — over three leaves: comparable type erasure with the field pin, the mixers and folds several libraries agree on to the bit, and the GPU device with its handles and fences"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/CACHE.md
       ${CMAKE_CURRENT_SOURCE_DIR}/COMPARABLE.md
       ${CMAKE_CURRENT_SOURCE_DIR}/COMPUTE.md
       ${CMAKE_CURRENT_SOURCE_DIR}/HARDWARE.md
       ${CMAKE_CURRENT_SOURCE_DIR}/RECONCILE.md
       ${CMAKE_CURRENT_SOURCE_DIR}/SCHEDULE.md)

add_subdirectory(comparable)
add_subdirectory(compute)
add_subdirectory(schedule)
add_subdirectory(reconcile)
add_subdirectory(cache)
add_subdirectory(hardware)

# Every API name the prose spells, compiled against the headers that own
# it — README.md and all six chapters, because a chapter is the same canon
# as the page that links it.
#
# Two include roots outside this library are scanned rather than linked:
# SigilMeasure, for the counters the reconcile stats publish into, and the
# tree-wide test support, whose own namespace is what tells the extractor
# that a fake host's `sigil::core::test` is a namespace and not a type. A
# name this library's documents spell that belongs to a library ABOVE it —
# the value noise SigilGeometry's path leaf agrees with, SigilDraw's field
# — is written unqualified, since resolving it would mean a test binary at
# the bottom of the tree linking most of the tree.
sigil_doc_probes(core
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/CACHE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/COMPARABLE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/COMPUTE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/HARDWARE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/RECONCILE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/SCHEDULE.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../measure/include
           ${SIGIL_TEST_SUPPORT_DIR}
  FLOORS 50,3,3,0,0
  LIBRARIES SigilCoreCache SigilCoreComparable SigilCoreCompute
            SigilCoreHardware SigilCoreReconcile SigilCoreSchedule)
