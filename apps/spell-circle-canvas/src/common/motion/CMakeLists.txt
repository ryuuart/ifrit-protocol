# SigilMotion — animation with no renderer in it, as one feature library
# per directory. Each feature is its own target with its own tests and
# benchmarks, and links only the features beneath it, so a consumer that
# needs one tier links that tier alone. `SigilMotion` is the umbrella: an
# interface over every feature, for a consumer of the whole library.
#
# Choreograph itself (sansumbrella's phrase/sequence/motion library, from
# the sigil-vcpkg-registry) is the animation vocabulary and is exposed
# PUBLIC — consumers compose phrases directly. It stays the ONLY external
# link dependency: anything that would drag Skia, Yoga or a kernel type
# in here does not belong here. RESOLVING an Animatable against a paint
# context stays with the consumer — SigilMotion ships the value, not a
# resolve surface.
#
# <sigilmotion/Animation.h> is the umbrella header over the values and
# bind features. Test fixtures every binary shares sit in test/support/;
# a feature's test binary includes "support/<Name>.h".
sigil_library_root(SigilMotion
  BRIEF "Animation timing, animatable values and schedules, with no renderer in them"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/BIND.md
       ${CMAKE_CURRENT_SOURCE_DIR}/CLOCK.md
       ${CMAKE_CURRENT_SOURCE_DIR}/PHYSICS.md
       ${CMAKE_CURRENT_SOURCE_DIR}/SCHEDULE.md
       ${CMAKE_CURRENT_SOURCE_DIR}/VALUES.md)

# Dependencies point down this list.
add_subdirectory(bind)
add_subdirectory(clock)
add_subdirectory(physics)
add_subdirectory(schedule)
add_subdirectory(values)

sigil_library(SigilMotion
  INTERFACE SigilMotionClock SigilMotionValues SigilMotionBind
            SigilMotionPhysics SigilMotionSchedule)

# EVERY API NAME THE PROSE SPELLS, compiled against the headers that own
# it — README.md and all five chapters, because a chapter is the same
# canon as the page that links it and prose nobody compiles is prose that
# goes stale. The translation unit is GENERATED from the documents on
# every build, so a passage written today is covered today and a rename
# the prose missed is a build break rather than a confident wrong answer;
# the generator FAILS on a documented name no header declares, so the
# guard cannot go quiet. The script is the tree's, under src/test, because
# more than one library wants it.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(MOTION_API_DOC_PROBES ${CMAKE_CURRENT_BINARY_DIR}/MotionApiDocProbes.cpp)
file(GLOB_RECURSE MOTION_PUBLIC_HEADERS
     CONFIGURE_DEPENDS include/sigilmotion/*.h)
set(MOTION_DOCS
    ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    ${CMAKE_CURRENT_SOURCE_DIR}/BIND.md
    ${CMAKE_CURRENT_SOURCE_DIR}/CLOCK.md
    ${CMAKE_CURRENT_SOURCE_DIR}/PHYSICS.md
    ${CMAKE_CURRENT_SOURCE_DIR}/SCHEDULE.md
    ${CMAKE_CURRENT_SOURCE_DIR}/VALUES.md)
set(MOTION_DOC_ARGS)
foreach(doc IN LISTS MOTION_DOCS)
  list(APPEND MOTION_DOC_ARGS --md ${doc})
endforeach()
add_custom_command(
  OUTPUT ${MOTION_API_DOC_PROBES}
  COMMAND ${Python3_EXECUTABLE} ${SIGIL_TEST_SUPPORT_DIR}/docs/api_doc_probes.py
          ${MOTION_DOC_ARGS}
          --include ${SIGIL_INCLUDE_DIR}/sigilmotion
          --include ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
          --library sigilmotion
          --namespace sigil::motion
          --suite MotionDocs
          --prelude "<gtest/gtest.h>"
          --prelude "<sigilcore/cache/Settle.h>"
          --alias ch=choreograph
          --floors 6,6,8,0
          --out ${MOTION_API_DOC_PROBES}
  DEPENDS ${MOTION_DOCS}
          ${SIGIL_TEST_SUPPORT_DIR}/docs/api_doc_probes.py
          ${MOTION_PUBLIC_HEADERS}
  COMMENT "Extracting the motion docs' names into compile probes"
  VERBATIM)

# The generator's own fixtures: a real name must yield a probe, an unreal
# one must fail the run, and an operator spelling must be exempted and
# reported by name. One Python invocation, and the only check that
# notices the EXTRACTOR narrowing, since a generator that silently probes
# less still emits a TU that compiles green.
add_test(NAME motion_api_doc_probes_self_test
  COMMAND ${Python3_EXECUTABLE} ${SIGIL_TEST_SUPPORT_DIR}/docs/api_doc_probes.py
          --self-test)

# The probes are their own object library, because a generated source is
# compiled by a target in the directory that generates it, and this
# library's one test binary is assembled in the feature directories. It
# links the umbrella and the one SigilCore leaf the prose names outside
# this library, because a document that spells a name is claiming that
# name exists wherever it lives.
add_library(motion_api_doc_probes OBJECT ${MOTION_API_DOC_PROBES})
target_link_libraries(motion_api_doc_probes PRIVATE
  SigilMotion SigilCoreCompute GTest::gtest)

sigil_test(motion_test
  LIBRARIES motion_api_doc_probes SigilMotion)
