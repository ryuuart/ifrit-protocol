# SigilTick — the backing ticking engine for animation: FrameClock
# (pausable, time-scalable, stall-clamped per-frame deltas) driving a
# Ticker that steps a choreograph::Timeline plus arbitrary steppables and
# reports whether more frames are needed (event-driven redraw contract).
# Choreograph itself (sansumbrella's phrase/sequence/motion library, from
# the sigil-vcpkg-registry) is the animation vocabulary and is exposed
# PUBLIC — consumers compose phrases directly.
add_library(SigilTick STATIC
  FrameClock.cpp
  Ticker.cpp
  include/sigiltick/FrameClock.h
  include/sigiltick/Ticker.h
)

target_compile_features(SigilTick PUBLIC cxx_std_20)

target_include_directories(SigilTick PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(SigilTick PUBLIC choreograph::choreograph)

add_executable(tick_test
  test/TickTest.cpp
)

target_link_libraries(tick_test PRIVATE SigilTick GTest::gtest_main)

add_test(NAME tick_test COMMAND tick_test)
