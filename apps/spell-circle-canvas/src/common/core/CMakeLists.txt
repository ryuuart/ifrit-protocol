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
# The two leaves depend on the standard library alone (Boost.PFR aside)
# and are linked from libraries far outside this one, which is what makes
# them leaves rather than parts of the kernels above them. The hardware
# feature is the one exception to std-only: it is the graphics API and
# nothing else — no Skia, no Diligent, no Qt — so a drawing library stands
# on the device it hands out rather than owning one, and two of them stand
# on the same one. Only a consumer that needs a device links it.
#
# Every public header lives under one include root, include/sigilcore/
# <feature>/, so a feature adds the root and spells its own directory.

set(SIGIL_CORE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

add_subdirectory(comparable)
add_subdirectory(compute)
add_subdirectory(reconcile)
add_subdirectory(cache)
add_subdirectory(hardware)

# The header tree installs as one unit beside the exported leaves.
install(DIRECTORY include/sigilcore DESTINATION include)

# API documentation (docs/README.md).
sigil_add_docs(
  NAME SigilCore
  BRIEF "The kernels a retained runtime is built on — the reconciler (keyed matching, memo, the identity prune, inherited values, animation lanes and phases) and the caching proof (policy, settled subtrees, the stability release and the bake seam) — over three leaves: comparable type erasure with the field pin, the mixers and folds several libraries agree on to the bit, and the GPU device with its handles and fences"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
