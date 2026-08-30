# SigilMotion — animation with no renderer in it, as three libraries a
# consumer links by what it uses:
#
#   SigilMotionBind   — <sigilmotion/Bind.h>: bind(), the Bound chain and
#                       the value-noise field behind wiggle(). The leaf:
#                       choreograph, cmath and integers only.
#   SigilMotionValues — <sigilmotion/Values.h>: Transition, the ease::
#                       house curves as EaseFn values, the animate()/
#                       from()/to()/through() keyframe builders, ramp(),
#                       phase(), quantizeTime() and Animatable<T>, the
#                       property slot that holds any one of them or a
#                       shaped binding (hence it links SigilMotionBind).
#   SigilMotionClock  — <sigilmotion/FrameClock.h> and <sigilmotion/
#                       Ticker.h>: a pausable, time-scalable, stall-clamped
#                       frame clock driving a Ticker that steps a
#                       choreograph::Timeline plus arbitrary steppables,
#                       derives Outputs through a Bound chain (hence it
#                       links SigilMotionBind), and reports whether more
#                       frames are needed.
#
# SigilMotion is the umbrella over all three; <sigilmotion/Animation.h>
# is the matching umbrella header over Values.h and Bind.h.
#
# Choreograph itself (sansumbrella's phrase/sequence/motion library, from
# the sigil-vcpkg-registry) is the animation vocabulary and is exposed
# PUBLIC — consumers compose phrases directly. It stays the ONLY external
# link dependency: anything that would drag Skia, Yoga or a kernel type
# in here does not belong here. RESOLVING an Animatable against a paint
# context stays with the consumer — SigilMotion ships the value, not a
# resolve surface.

add_library(SigilMotionBind STATIC
  Bind.cpp
  include/sigilmotion/Bind.h
)
target_compile_features(SigilMotionBind PUBLIC cxx_std_20)
target_include_directories(SigilMotionBind PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(SigilMotionBind PUBLIC choreograph::choreograph)

add_library(SigilMotionValues INTERFACE
  include/sigilmotion/Animation.h
  include/sigilmotion/Values.h
)
target_compile_features(SigilMotionValues INTERFACE cxx_std_20)
target_include_directories(SigilMotionValues INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(SigilMotionValues INTERFACE
  SigilMotionBind choreograph::choreograph)

add_library(SigilMotionClock STATIC
  FrameClock.cpp
  Ticker.cpp
  include/sigilmotion/FrameClock.h
  include/sigilmotion/Ticker.h
)
target_compile_features(SigilMotionClock PUBLIC cxx_std_20)
target_include_directories(SigilMotionClock PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)
target_link_libraries(SigilMotionClock PUBLIC
  SigilMotionBind choreograph::choreograph)

add_library(SigilMotion INTERFACE)
target_link_libraries(SigilMotion INTERFACE
  SigilMotionClock SigilMotionValues SigilMotionBind)

# One test per library, each linking only the library it exercises (and
# the clock where a value is driven by the Ticker), so a link edge that
# pulled a drawing library in would show up as a build failure here.
add_executable(motion_clock_test test/MotionClockTest.cpp)
target_link_libraries(motion_clock_test PRIVATE SigilMotionClock GTest::gtest_main)
add_test(NAME motion_clock_test COMMAND motion_clock_test)

add_executable(motion_values_test test/MotionValuesTest.cpp)
target_link_libraries(motion_values_test PRIVATE
  SigilMotionValues SigilMotionClock GTest::gtest_main)
add_test(NAME motion_values_test COMMAND motion_values_test)

add_executable(motion_bind_test test/MotionBindTest.cpp
)
target_link_libraries(motion_bind_test PRIVATE SigilMotionBind GTest::gtest_main)
add_test(NAME motion_bind_test COMMAND motion_bind_test)

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
