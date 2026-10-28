# SigilTree — the kernels a retained runtime is built on, as one feature
# library per directory. README.md is the canon.
#
#   SigilTreeReconcile   reconcile/   descriptions reconciled onto a host's
#                                     retained tree: keyed matching, the
#                                     memo and what it follows, the
#                                     identity prune and the phase runner
#   SigilTreeCache       cache/       what a host may keep between frames:
#                                     the cache policy, the settled-subtree
#                                     proof, the stability release and the
#                                     bake seam
#
# Both stand on SigilCore's vocabulary — the comparable erasure a seam
# value takes and the follow leaf a memo's describe is recorded through —
# and the reconciler publishes its counts into SigilMeasure, which is why
# this directory is added after measure's. Nothing here draws, lays out,
# shapes text or animates: a host implements the operations each kernel
# names, and keeps everything those operations act on.
sigil_library_root(SigilTree
  BRIEF "The kernels a retained runtime is built on: the reconciler (keyed matching, the memo and what it follows, the identity prune and the phase runner) and the caching proof (policy, settled subtrees, the stability release and the bake seam)"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/docs/Groups.dox
       ${CMAKE_CURRENT_SOURCE_DIR}/CACHE.md)

add_subdirectory(reconcile)
add_subdirectory(cache)

# Every API name the prose spells, compiled against the headers that own
# it — README.md, its chapter and every entity page under
# reference/pages/, because a chapter is the same canon as the page that
# links it, and a page carries the prose a brief in the header cannot.
#
# Three include roots outside this library are scanned rather than
# linked: SigilCore, for the follow leaf and the memo the boundary names;
# SigilMeasure, for the counters the reconcile stats publish into; and the
# tree-wide test support, whose own namespace is what tells the extractor
# that a fake host's `sigil::tree::test` is a namespace and not a type.
sigil_doc_probes(tree
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/CACHE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/ReconcileHost.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/functions/orderByReads.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
           ${CMAKE_CURRENT_SOURCE_DIR}/../measure/include
           ${SIGIL_TEST_SUPPORT_DIR}
  PRELUDES "<sigilcore/memo/Rebuild.h>"
  FLOORS 8,0,2,2,0
  LIBRARIES SigilTreeCache SigilTreeReconcile SigilCoreMemo)
