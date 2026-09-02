# The plate feature: the headless sweep. Every sketch is stepped to its
# declared moment and photographed, so a plate is a function of the
# declaration alone and never of how fast the machine ran.
add_library(SigilSketchPlate STATIC
  Sweep.cpp
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/plate/FrameStats.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/plate/Graphite.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/plate/Sweep.h
)
target_compile_features(SigilSketchPlate PUBLIC cxx_std_20)
target_link_libraries(SigilSketchPlate PUBLIC SigilSketchCore SigilMeasure
  SigilImageEncode)
set_target_properties(SigilSketchPlate PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The device lane: a process-owned device and queue with a Graphite
# context over them, since a run with no window shares nobody's.
if(APPLE AND TARGET SigilSkia)
  target_sources(SigilSketchPlate PRIVATE GraphiteMetal.mm)
  target_link_libraries(SigilSketchPlate PRIVATE SigilSkia)
  target_compile_definitions(SigilSketchPlate PRIVATE SIGILSKETCH_HEADLESS_GPU)
endif()

add_executable(sketch_plate_test test/SweepTest.cpp)
target_include_directories(sketch_plate_test PRIVATE
  ${SIGIL_SKETCH_TEST_SUPPORT_DIR} ${SIGIL_TEST_SUPPORT_DIR})
target_link_libraries(sketch_plate_test PRIVATE SigilSketchPlate
  SigilSketchCanvas SigilWeavePorts GTest::gtest_main)
add_test(NAME sketch_plate_test COMMAND sketch_plate_test)

add_executable(sketch_plate_bench bench/SweepBench.cpp)
target_link_libraries(sketch_plate_bench PRIVATE SigilSketchPlate
  SigilSketchCanvas SigilWeavePorts benchmark::benchmark)
add_dependencies(benches sketch_plate_bench)
