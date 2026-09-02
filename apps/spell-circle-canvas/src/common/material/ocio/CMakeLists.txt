# The ocio feature: OpenColorIO view transforms baked to LUT materials
# over the texture feature. OpenColorIO is already in the dependency
# closure (transitively via OpenImageIO), so this adds no new vcpkg
# dependency; where the package is absent the feature still builds, with
# `available()` false and every factory answering the empty LUT material,
# so a consumer links one name on every machine. The definition is PUBLIC
# so a consumer can #ifdef around the transforms.
find_package(OpenColorIO CONFIG QUIET)
add_library(SigilMaterialOcio STATIC
  LutRecipe.cpp
  ${SIGIL_MATERIAL_INCLUDE_DIR}/sigilmaterial/ocio/Ocio.h
)
target_compile_features(SigilMaterialOcio PUBLIC cxx_std_20)
target_include_directories(SigilMaterialOcio PUBLIC ${SIGIL_MATERIAL_INCLUDE_DIR})
target_link_libraries(SigilMaterialOcio PUBLIC SigilMaterialTexture)
if(TARGET OpenColorIO::OpenColorIO)
  target_sources(SigilMaterialOcio PRIVATE Ocio.cpp)
  target_link_libraries(SigilMaterialOcio PRIVATE OpenColorIO::OpenColorIO)
  target_compile_definitions(SigilMaterialOcio PUBLIC SIGILMATERIAL_ENABLE_OCIO)
else()
  target_sources(SigilMaterialOcio PRIVATE Unavailable.cpp)
  message(WARNING
    "OpenColorIO not found — <sigilmaterial/ocio/Ocio.h> answers unavailable")
endif()

# The test shades through the Skia backend, and skips where the
# transforms are unavailable.
add_executable(material_ocio_test test/OcioTest.cpp)
target_link_libraries(material_ocio_test PRIVATE SigilMaterialOcio
  SigilMaterialSkia GTest::gtest_main)
add_test(NAME material_ocio_test COMMAND material_ocio_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(material_ocio_bench bench/OcioBench.cpp)
target_link_libraries(material_ocio_bench PRIVATE SigilMaterialOcio
  benchmark::benchmark)
add_dependencies(benches material_ocio_bench)
