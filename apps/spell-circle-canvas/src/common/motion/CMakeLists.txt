# SigilMotion — the backing ticking engine for animation: FrameClock
# (pausable, time-scalable, stall-clamped per-frame deltas) driving a
# Ticker that steps a choreograph::Timeline plus arbitrary steppables and
# reports whether more frames are needed (event-driven redraw contract).
# Choreograph itself (sansumbrella's phrase/sequence/motion library, from
# the sigil-vcpkg-registry) is the animation vocabulary and is exposed
# PUBLIC — consumers compose phrases directly.
add_library(SigilMotion STATIC
  FrameClock.cpp
  Ticker.cpp
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
