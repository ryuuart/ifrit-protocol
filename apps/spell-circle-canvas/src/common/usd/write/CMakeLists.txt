# The write feature: a stage built from the values a scene is made of and
# saved in the format the file's extension asks for. Links the geometry
# features it takes values from, SigilMaterialKit for the surface recipe
# whose params and map slots become a preview surface, SigilWorldLight for
# the emitters and SigilWorldElement for the environment map a dome light
# carries; OpenUSD, the image encoder that turns a texture into bytes and
# the sink that lands them beside the stage are private to it.
add_library(SigilUsdWrite STATIC
  Camera.cpp
  Environment.cpp
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
  PUBLIC SigilMaterialKit SigilWorldElement SigilWorldLight SigilGeometryMesh
         SigilGeometryMeshPop SigilGeometryMeshCamera unofficial::skia::skia
  PRIVATE SigilImageEncode SigilLoaderSource usd usdGeom usdShade usdLux sdf
          tf gf vt)

# Links the library it exercises, the runtime probe it skips on, and USD
# itself to inspect the authored stage.
add_executable(usd_write_test test/WriteTest.cpp)
# The library's own test directory holds the scratch path, the runtime
# skip and the scene both doors are exercised over.
target_include_directories(usd_write_test PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../test)
target_link_libraries(usd_write_test PRIVATE SigilUsdWrite SigilUsdRuntime
  SigilGeometryKit GTest::gtest_main usd usdGeom usdShade sdf tf vt)
add_test(NAME usd_write_test COMMAND usd_write_test)

# Google Benchmark (use a Release build). Not a test: it builds through the
# `benches` target and runs through scripts/bench_ledger.py.
add_executable(usd_write_bench bench/WriteBench.cpp)
target_link_libraries(usd_write_bench PRIVATE SigilUsdWrite SigilUsdRuntime
  SigilGeometryKit benchmark::benchmark)
add_dependencies(benches usd_write_bench)
