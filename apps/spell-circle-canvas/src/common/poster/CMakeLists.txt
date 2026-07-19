# IfritPoster — Yoga-composed, z-ordered, animated typographic layouts
# over TextFlow. Currently design + feasibility spike only: DESIGN.md is
# the proposal, poster_spike_test proves the load-bearing assumption
# (Yoga measure/baseline callbacks driven by TextFlow). The library
# itself lands once the API design is agreed.
add_executable(poster_spike_test
  test/PosterSpikeTest.cpp
)

target_link_libraries(poster_spike_test PRIVATE
  TextFlow TextFlowPorts yoga::yogacore GTest::gtest_main)

add_test(NAME poster_spike_test COMMAND poster_spike_test)
