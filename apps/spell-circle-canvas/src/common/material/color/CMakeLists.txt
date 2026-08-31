# The colour feature: the colour value and the OKLab round trip are the
# header-only Color.h, which the core includes and which is listed here as
# the feature's own; the OpenColorIO view transforms bake to LUT materials
# over the texture feature. OpenColorIO is already in the dependency
# closure (transitively via OpenImageIO), so this adds no new vcpkg
# dependency — the transforms simply disable with a warning if the package
# is absent. The definition is PUBLIC so a consumer can #ifdef around them.
add_library(SigilMaterialColor STATIC
  Color.cpp
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/color/Color.h
)
target_compile_features(SigilMaterialColor PUBLIC cxx_std_20)
target_include_directories(SigilMaterialColor PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR})
target_link_libraries(SigilMaterialColor PUBLIC SigilMaterialTexture)

find_package(OpenColorIO CONFIG QUIET)
if(TARGET OpenColorIO::OpenColorIO)
  target_sources(SigilMaterialColor PRIVATE Ocio.cpp
    ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/color/Ocio.h)
  target_link_libraries(SigilMaterialColor PRIVATE OpenColorIO::OpenColorIO)
  target_compile_definitions(SigilMaterialColor PUBLIC SIGILMATERIAL_ENABLE_OCIO)
else()
  message(WARNING "OpenColorIO not found — <sigilmaterial/color/Ocio.h> disabled")
endif()

add_executable(material_color_test test/ColorTest.cpp)
target_link_libraries(material_color_test PRIVATE SigilMaterialColor
  SigilMaterialSkia GTest::gtest_main)
add_test(NAME material_color_test COMMAND material_color_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(material_color_bench bench/ColorBench.cpp)
target_link_libraries(material_color_bench PRIVATE SigilMaterialColor
  SigilMaterialSkia benchmark::benchmark)
add_dependencies(benches material_color_bench)
