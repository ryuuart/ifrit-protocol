# The set feature: the 3D sketch surface and the session that drives it —
# a ticker, a retained Scene, and one set body describing a frame as a
# function of the scene time.
#
# ITS PUBLIC DEPENDENCIES ARE THE 3D SKETCH API. It links no device: the
# runtime a session draws through is a value the process installs, so a
# machine with no device runs every set on the CPU mesh executor.
add_library(SigilSketchSet STATIC
  Session.cpp
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/set/Set.h
)
target_compile_features(SigilSketchSet PUBLIC cxx_std_20)
target_link_libraries(SigilSketchSet PUBLIC
  SigilSketchCore SigilWorldScene SigilWorldKit SigilWorldFrame
  SigilWorldElement SigilWorldLight SigilMaterialKit SigilMotionClock)
# The session's lap timer is read in its source alone.
target_link_libraries(SigilSketchSet PRIVATE SigilMeasure)
set_target_properties(SigilSketchSet PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(sketch_set_test test/SetSessionTest.cpp)
target_include_directories(sketch_set_test PRIVATE
  ${SIGIL_SKETCH_TEST_SUPPORT_DIR})
target_link_libraries(sketch_set_test PRIVATE SigilSketchSet SigilWeavePorts SigilWeaveFonts
  SigilGeometryKit GTest::gtest_main)
add_test(NAME sketch_set_test COMMAND sketch_set_test)

add_executable(sketch_set_bench bench/SetSessionBench.cpp)
target_link_libraries(sketch_set_bench PRIVATE SigilSketchSet SigilWeavePorts SigilWeaveFonts
  SigilGeometryKit benchmark::benchmark)
add_dependencies(benches sketch_set_bench)
