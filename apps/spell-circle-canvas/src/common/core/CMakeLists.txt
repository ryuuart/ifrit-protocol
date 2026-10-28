# SigilCore — the vocabulary every other Sigil library shares, as one
# feature library per directory. README.md is the canon.
#
#   SigilCoreComparable  comparable/  what a value needs before anything
#                                     can decide it did not change:
#                                     comparable type erasure and the
#                                     field pin
#   SigilCoreCompute     compute/     the arithmetic several libraries
#                                     agree on to the bit: the seeded
#                                     mixers, the identifying folds, the
#                                     shaped curve, the angle and the time
#                                     unit
#   SigilCoreCallable    callable/    the one prefix search in the tree: a
#                                     callable called with the parameters it
#                                     named, and the type-erased call a verb
#                                     holds one in
#   SigilCoreSchedule    schedule/    where independent work runs: one
#                                     parallel for over the task runtime,
#                                     and a fan-out of its own threads for
#                                     calls that block on a disk or a
#                                     server
#   SigilCoreFollow      follow/      what a describe read that can change
#                                     after it: a value that counts its
#                                     changes, the record of the reads,
#                                     and the scope that records them
#   SigilCoreMemo        memo/        the keyed memo: rebuild on change,
#                                     the value held with its guard, and
#                                     the snap a drifting key is taken
#                                     through
#   SigilCoreReport      report/      the one reporting door: a library's
#                                     warning in one format, and the
#                                     reporter a host installs to hear it
#
# The schedule feature is the one every library that divides work over
# threads links, so that a task runtime is named in one file of this
# repository and no header of it spells one.
#
# Every feature depends on the standard library (Boost.PFR, and oneTBB
# privately, aside) and on nothing of this project's but another feature
# here, which is what lets a library anywhere in the tree link one without
# acquiring a kernel, a device or a drawing library.
sigil_library_root(SigilCore
  BRIEF "The vocabulary every Sigil library shares: comparable type erasure with the field pin, the mixers, folds, curve, angle and time unit several libraries agree on to the bit, the one prefix search a callable is called with the parameters it named through, where independent work runs, the record of the shared values a describe reads and follows, the keyed memo, and the one door a library's warning is reported through"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/docs/Groups.dox
       ${CMAKE_CURRENT_SOURCE_DIR}/CALLABLE.md
       ${CMAKE_CURRENT_SOURCE_DIR}/COMPARABLE.md
       ${CMAKE_CURRENT_SOURCE_DIR}/COMPUTE.md
       ${CMAKE_CURRENT_SOURCE_DIR}/SCHEDULE.md)

add_subdirectory(comparable)
add_subdirectory(compute)
add_subdirectory(callable)
add_subdirectory(schedule)
add_subdirectory(follow)
add_subdirectory(memo)
add_subdirectory(report)

# Every API name the prose spells, compiled against the headers that own
# it — README.md, all four chapters and every entity page under
# reference/pages/, because a chapter is the same canon as the page that
# links it, and a page carries the prose a brief in the header cannot.
#
# The tree-wide test support is scanned rather than linked. A name this
# library's documents spell that belongs to a library ABOVE it — the value
# noise SigilGeometry's path leaf agrees with, SigilDraw's field — is
# written unqualified, since resolving it would mean a test binary at the
# bottom of the tree linking most of the tree.
sigil_doc_probes(core
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/CALLABLE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/COMPARABLE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/COMPUTE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/SCHEDULE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Curve.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/types/Field.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/functions/concurrentIo.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/functions/mix64.md
          ${CMAKE_CURRENT_SOURCE_DIR}/reference/pages/functions/parallelFor.md
  INCLUDES ${SIGIL_TEST_SUPPORT_DIR}
  FLOORS 72,6,3,15,0
  LIBRARIES SigilCoreCallable SigilCoreComparable SigilCoreCompute
            SigilCoreFollow SigilCoreMemo SigilCoreReport SigilCoreSchedule)
