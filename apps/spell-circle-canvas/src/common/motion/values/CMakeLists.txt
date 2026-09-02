# The animation values: Transition and the ease:: house curves, the
# animate()/from()/to()/through() keyframe builders, ramp(), phase(),
# quantizeTime() and Animatable<T>, the property slot that holds any one
# of them or a shaped binding (hence it links the bind leaf) — the held
# motion an animatable runs as, which is why this feature links the
# clock: a value that is MOVING is moving on a ticker — and the lanes a
# host retargets those motions through, which name a host's storage
# enumeration and nothing else about it.
#
# It links SigilCoreComparable for the field pin the comparators over
# these values sit under. That is an edge OUT of this library, and the
# one target it reaches is a header-only leaf over the standard library
# and Boost.PFR: no kernel, no device, nothing that draws.
# <sigilmotion/Animation.h> rides here as the umbrella over this feature
# and bind.
add_library(SigilMotionValues STATIC
  Animated.cpp
  Transition.cpp
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/Animation.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/values/Animatable.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/values/Animated.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/values/Keyframes.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/values/Lanes.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/values/Time.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/values/Transition.h
  ${SIGIL_MOTION_INCLUDE_DIR}/sigilmotion/values/Values.h
)
target_compile_features(SigilMotionValues PUBLIC cxx_std_20)
target_include_directories(SigilMotionValues PUBLIC
  ${SIGIL_MOTION_INCLUDE_DIR})
target_link_libraries(SigilMotionValues PUBLIC
  SigilMotionBind SigilMotionClock SigilCoreComparable
  choreograph::choreograph)

# Links the values and the clock that drives them, and nothing that
# draws, so a link edge that pulled a drawing library in would show up
# as a build failure here.
add_executable(motion_values_test test/ValuesTest.cpp test/AnimatedTest.cpp
  test/LanesTest.cpp)
target_link_libraries(motion_values_test PRIVATE
  SigilMotionValues SigilMotionClock GTest::gtest_main)
add_test(NAME motion_values_test COMMAND motion_values_test)

# Google Benchmark (use a Release build), linking only what it measures.
# Not a test: it builds through the `benches` target and runs through
# scripts/bench_ledger.py.
add_executable(motion_values_bench bench/ValuesBench.cpp)
target_link_libraries(motion_values_bench PRIVATE SigilMotionValues
  benchmark::benchmark)
add_dependencies(benches motion_values_bench)
