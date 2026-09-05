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
sigil_library_root(SigilCore
  BRIEF "The kernels a retained runtime is built on — the reconciler (keyed matching, memo, the identity prune, inherited values, animation lanes and phases) and the caching proof (policy, settled subtrees, the stability release and the bake seam) — over three leaves: comparable type erasure with the field pin, the mixers and folds several libraries agree on to the bit, and the GPU device with its handles and fences")

add_subdirectory(comparable)
add_subdirectory(compute)
add_subdirectory(reconcile)
add_subdirectory(cache)
add_subdirectory(hardware)
