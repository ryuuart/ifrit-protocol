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
#
# The two leaves depend on the standard library alone (Boost.PFR aside)
# and are linked from libraries far outside this one, which is what makes
# them leaves rather than parts of the kernels above them.
#
# Every public header lives under one include root, include/sigilcore/
# <feature>/, so a feature adds the root and spells its own directory.

set(SIGIL_CORE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

add_subdirectory(comparable)
add_subdirectory(compute)
add_subdirectory(reconcile)
add_subdirectory(cache)

# The header tree installs as one unit beside the exported leaves.
install(DIRECTORY include/sigilcore DESTINATION include)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilCore
  BRIEF "The kernels a retained runtime is built on — the reconciler (keyed matching, memo, the identity prune, inherited values, animation lanes and phases) and the caching proof (policy, settled subtrees, the stability release and the bake seam) — over two leaves: comparable type erasure with the field pin, and the mixers and folds several libraries agree on to the bit"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
