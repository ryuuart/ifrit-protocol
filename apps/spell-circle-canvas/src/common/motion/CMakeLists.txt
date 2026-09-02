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
# Every public header lives under one include root, include/sigilmotion/
# <feature>/, so a feature adds the root and spells its own directory.
# <sigilmotion/Animation.h> is the umbrella header over the values and
# bind features.

set(SIGIL_MOTION_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

# Dependencies point down this list.
add_subdirectory(bind)
add_subdirectory(clock)
add_subdirectory(values)

add_library(SigilMotion INTERFACE)
target_link_libraries(SigilMotion INTERFACE
  SigilMotionClock SigilMotionValues SigilMotionBind)

# Registers this library with the `docs` target (see cmake/Docs.cmake).
# The README is the site's front page, so the generated pages open on the
# same document that is canon for the library.
sigil_add_docs(
  NAME SigilMotion
  BRIEF "Animation timing and animatable values, with no renderer in them"
  INPUT
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP
        ${CMAKE_CURRENT_SOURCE_DIR}/include)
