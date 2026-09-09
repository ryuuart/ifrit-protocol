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

# Every API name the prose spells, compiled against the headers that own
# it — README.md and all five chapters, because a chapter is the same
# canon as the page that links it. SigilCore is scanned for the settle
# policy the values chapter names, and `ch` is the alias the prose writes
# choreograph's own names through.
sigil_doc_probes(motion
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
          ${CMAKE_CURRENT_SOURCE_DIR}/BIND.md
          ${CMAKE_CURRENT_SOURCE_DIR}/CLOCK.md
          ${CMAKE_CURRENT_SOURCE_DIR}/PHYSICS.md
          ${CMAKE_CURRENT_SOURCE_DIR}/SCHEDULE.md
          ${CMAKE_CURRENT_SOURCE_DIR}/VALUES.md
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../core/include
  PRELUDES "<sigilcore/cache/Settle.h>"
  ALIASES ch=choreograph
  FLOORS 6,6,8,0,0
  LIBRARIES SigilMotion SigilCoreCompute)
