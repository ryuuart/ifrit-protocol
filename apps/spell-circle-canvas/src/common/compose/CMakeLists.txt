# IfritCompose — data-driven, cacheable, animated drawable components
# over Yoga + TextFlow + Choreograph/IfritTick + Skia. DESIGN.md is the
# architecture, API.md the surface, STRESS_TESTS.md the acceptance
# catalog. This is the phase-1 kernel: element values, keyed
# reconciler, Yoga layout with TextFlow-measured text, stacking paint,
# automatic picture caching of provably-static subtrees, and
# Choreograph transitions/bindings.
add_library(IfritCompose STATIC
  Compose.cpp
  Composer.cpp
  ComposeInternal.h
  include/ifritcompose/Compose.h
)

target_compile_features(IfritCompose PUBLIC cxx_std_20)

target_include_directories(IfritCompose PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(IfritCompose
  PUBLIC
    IfritTick
    TextFlow
    unofficial::skia::skia
  PRIVATE
    IfritImage
    yoga::yogacore
)

add_executable(compose_test
  test/ComposeTest.cpp
)
target_link_libraries(compose_test PRIVATE
  IfritCompose TextFlowPorts GTest::gtest_main)
add_test(NAME compose_test COMMAND compose_test)

# The pre-implementation feasibility spike stays: it pins the
# Yoga+TextFlow measurement contract independently of the kernel.
add_executable(compose_spike_test
  test/ComposeSpikeTest.cpp
)
target_link_libraries(compose_spike_test PRIVATE
  TextFlow TextFlowPorts yoga::yogacore GTest::gtest_main)
add_test(NAME compose_spike_test COMMAND compose_spike_test)

# Perf gate (run in Release): describe/reconcile/draw costs, memo hit
# behavior, cache effectiveness. Numbers land in STRESS_TESTS.md.
add_executable(compose_bench
  bench/ComposeBench.cpp
)
target_link_libraries(compose_bench PRIVATE
  IfritCompose TextFlowPorts benchmark::benchmark)

# Headless gallery: renders stress-catalog panels to PNG (out dir:
# first argument, default ./compose_demo_out). The interactive Qt
# ComposeGallery follows in a later phase.
add_executable(compose_demo
  examples/compose_demo.cpp
)
target_link_libraries(compose_demo PRIVATE IfritCompose TextFlowPorts)
