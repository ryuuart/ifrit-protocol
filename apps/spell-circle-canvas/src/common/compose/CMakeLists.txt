# IfritCompose — data-driven, cacheable, animated drawable components
# over Yoga + TextFlow + Choreograph/IfritTick + Skia. Currently design
# + feasibility spike only: DESIGN.md is the architecture, API.md the
# surface, STRESS_TESTS.md the implementation-validation catalog;
# compose_spike_test proves the load-bearing assumption (Yoga
# measure/baseline callbacks driven by TextFlow). The library lands
# with phase 1.
add_executable(compose_spike_test
  test/ComposeSpikeTest.cpp
)

target_link_libraries(compose_spike_test PRIVATE
  TextFlow TextFlowPorts yoga::yogacore GTest::gtest_main)

add_test(NAME compose_spike_test COMMAND compose_spike_test)
