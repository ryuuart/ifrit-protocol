# The optional SigilSketch × SigilScry bridge. A host configures one lazy
# process-owned engine and sketches borrow it; ordinary SigilScry consumers
# continue to construct and own WebEngine directly.
add_library(SigilSketchScry STATIC
  SharedEngine.cpp
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/scry/SharedEngine.h
)
target_compile_features(SigilSketchScry PUBLIC cxx_std_20)
target_include_directories(SigilSketchScry PUBLIC ${SIGIL_SKETCH_INCLUDE_DIR})
target_link_libraries(SigilSketchScry PUBLIC SigilScry)
set_target_properties(SigilSketchScry PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(sketch_scry_test test/SharedEngineTest.cpp)
target_link_libraries(sketch_scry_test PRIVATE
  SigilSketchScry GTest::gtest_main)
ultralight_copy_resources(sketch_scry_test)
add_test(NAME sketch_scry_test COMMAND sketch_scry_test)
