# The light feature: emitters as plain comparable values — a sun, a
# point light, a spot, their falloffs and the per-frame budget. No
# device, no registry, no renderer: glm and nothing else, so a consumer
# that only needs to say where the lights are links this alone.
find_package(glm CONFIG REQUIRED)
add_library(SigilWorldLight STATIC
  Light.cpp
  ${SIGIL_WORLD_INCLUDE_DIR}/sigilworld/light/Light.h
)
target_compile_features(SigilWorldLight PUBLIC cxx_std_20)
target_include_directories(SigilWorldLight PUBLIC ${SIGIL_WORLD_INCLUDE_DIR})
target_link_libraries(SigilWorldLight PUBLIC glm::glm)

# Links only the library it exercises, so a link edge that pulled the
# device in would show up as a build failure here.
add_executable(world_light_test test/LightTest.cpp)
target_link_libraries(world_light_test PRIVATE SigilWorldLight
  GTest::gtest_main)
add_test(NAME world_light_test COMMAND world_light_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(world_light_bench bench/LightBench.cpp)
target_link_libraries(world_light_bench PRIVATE SigilWorldLight
  benchmark::benchmark)
add_dependencies(benches world_light_bench)
