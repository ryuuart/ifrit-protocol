# SigilMotion — animation with no renderer in it. Two halves:
#
#   the CLOCK — FrameClock (pausable, time-scalable, stall-clamped
#   per-frame deltas) driving a Ticker that steps a choreograph::Timeline
#   plus arbitrary steppables and reports whether more frames are needed
#   (event-driven redraw contract);
#
#   the VALUES — <sigilmotion/Animation.h>: Transition, the ease:: house
#   curves as EaseFn values, the animate()/from()/to()/through() keyframe
#   builders, bind() shaped bindings, and Animatable<T>, the property
#   slot that holds any one of them. These lived in
#   <sigilcompose/Compose.h> until 2026-07-29; they are choreograph,
#   chrono and float math only, so they belong here where SigilWorld and
#   SigilGeometry can reach them without linking a drawing library.
#   RESOLVING an Animatable against a paint context stays with the
#   consumer — SigilMotion ships the value, not a resolve surface.
#
# Choreograph itself (sansumbrella's phrase/sequence/motion library, from
# the sigil-vcpkg-registry) is the animation vocabulary and is exposed
# PUBLIC — consumers compose phrases directly. It stays the ONLY link
# dependency: anything that would drag Skia, Yoga or a kernel type in
# here does not belong here.
add_library(SigilMotion STATIC
  FrameClock.cpp
  Ticker.cpp
  include/sigilmotion/Animation.h
  include/sigilmotion/FrameClock.h
  include/sigilmotion/Ticker.h
)

target_compile_features(SigilMotion PUBLIC cxx_std_20)

target_include_directories(SigilMotion PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(SigilMotion PUBLIC choreograph::choreograph)

add_executable(motion_test
  test/MotionTest.cpp
)

target_link_libraries(motion_test PRIVATE SigilMotion GTest::gtest_main)

add_test(NAME motion_test COMMAND motion_test)

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
