# SigilCore — the kernels a retained runtime is built on, as one feature
# library per directory. README.md is the canon.
#
#   SigilCoreReconcile   reconcile/   descriptions reconciled onto a host's
#                                     retained tree: keyed matching, the
#                                     memo, the identity prune, the
#                                     inherited-value channel, the animation
#                                     lanes and the phase runner
#
# Every public header lives under one include root, include/sigilcore/
# <feature>/, so a feature adds the root and spells its own directory.

set(SIGIL_CORE_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

add_subdirectory(reconcile)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilCore
  BRIEF "The reconciler kernel a retained runtime hosts: keyed matching, memo, the identity prune, inherited values, animation lanes and phases"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
