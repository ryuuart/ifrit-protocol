# The write feature: a stage built from the values a scene is made of and
# saved in the format the file's extension asks for. Links the geometry
# features it takes values from, SigilMaterialKit for the surface recipe
# whose params and map slots become a preview surface, and SigilWorldLight
# for the emitters; OpenUSD is private to it.
add_library(SigilUsdWrite STATIC
  Camera.cpp
  GfMatrix.h
  Lights.cpp
  Material.cpp
  Mesh.cpp
  Stamps.cpp
  Writer.cpp
  WriterImpl.h
  ${SIGIL_USD_INCLUDE_DIR}/sigilusd/write/Writer.h
)
target_compile_features(SigilUsdWrite PUBLIC cxx_std_20)
target_include_directories(SigilUsdWrite PUBLIC ${SIGIL_USD_INCLUDE_DIR})
target_link_libraries(SigilUsdWrite
  PUBLIC SigilMaterialKit SigilWorldLight SigilGeometryMesh
         SigilGeometryMeshPop SigilGeometryMeshCamera unofficial::skia::skia
  PRIVATE usd usdGeom usdShade usdLux sdf tf gf vt)

# Links the library it exercises, the runtime probe it skips on, and USD
# itself to inspect the authored stage.
add_executable(usd_write_test test/WriteTest.cpp)
target_link_libraries(usd_write_test PRIVATE SigilUsdWrite SigilUsdRuntime
  GTest::gtest_main usd usdGeom usdShade sdf tf vt)
add_test(NAME usd_write_test COMMAND usd_write_test)

# Google Benchmark (use a Release build). Not a test: it builds through the
# `benches` target and runs through scripts/bench_ledger.py.
add_executable(usd_write_bench bench/WriteBench.cpp)
target_link_libraries(usd_write_bench PRIVATE SigilUsdWrite SigilUsdRuntime
  benchmark::benchmark)
add_dependencies(benches usd_write_bench)
