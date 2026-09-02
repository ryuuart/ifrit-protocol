# The draw feature: the two SkCanvas ops Graphite leaves unimplemented,
# decomposed into ones every backend performs. Header-only over Skia and
# nothing else — no device, no context, no bring-up — so a consumer that
# never brings a Graphite context up still draws a nine-slice frame and a
# sprite sheet that survive being recorded on one canvas and replayed on
# another. That is why this feature is unconditional where the rest of
# the library is gated on the Graphite backend being built.
add_library(SigilSkiaDraw INTERFACE
  ${SIGIL_SKIA_INCLUDE_DIR}/sigilskia/draw/Direct.h
)
target_compile_features(SigilSkiaDraw INTERFACE cxx_std_20)
target_include_directories(SigilSkiaDraw INTERFACE ${SIGIL_SKIA_INCLUDE_DIR})
target_link_libraries(SigilSkiaDraw INTERFACE unofficial::skia::skia)

# The band split is arithmetic, so its test needs no GPU and no context —
# which is the point of the feature standing apart from the bring-up.
add_executable(sigilskia_draw_test test/DirectTest.cpp)
target_link_libraries(sigilskia_draw_test PRIVATE SigilSkiaDraw
  GTest::gtest_main)
add_test(NAME sigilskia_draw_test COMMAND sigilskia_draw_test)
