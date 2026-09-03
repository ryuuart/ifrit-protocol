# The read feature: a stage poured into geometry's decode::Model, with
# its emitters and cameras read back as values. Links the codec feature
# the Model comes from, SigilWorldElement and SigilWorldLight and the geometry
# camera those values are; OpenUSD is private to it.
add_library(SigilUsdRead STATIC
  Camera.cpp
  Environment.cpp
  GfMatrix.h
  Instancer.cpp
  Lights.cpp
  Material.cpp
  Mesh.cpp
  Primvar.h
  ReadContext.h
  Reader.cpp
  ${SIGIL_USD_INCLUDE_DIR}/sigilusd/read/Reader.h
)
target_compile_features(SigilUsdRead PUBLIC cxx_std_20)
target_include_directories(SigilUsdRead PUBLIC ${SIGIL_USD_INCLUDE_DIR})
target_link_libraries(SigilUsdRead
  PUBLIC SigilGeometryMeshCodec SigilGeometryMesh SigilGeometryMeshCamera
         SigilWorldElement SigilWorldLight
  PRIVATE usd usdGeom usdShade usdLux ar sdf tf gf vt)

# Links the library it exercises, the runtime probe it skips on, and the
# writer whose stages it round-trips; the committed fixtures under
# test/assets/ reach it through the IFRIT_USD_TEST_ASSET_DIR definition.
add_executable(usd_read_test test/ReadTest.cpp)
# The library's own test directory holds the scratch path, the runtime
# skip and the scene both doors are exercised over.
target_include_directories(usd_read_test PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../test)
target_link_libraries(usd_read_test PRIVATE SigilUsdRead SigilUsdWrite
  SigilUsdRuntime SigilGeometryKit GTest::gtest_main)
target_compile_definitions(usd_read_test PRIVATE
  IFRIT_USD_TEST_ASSET_DIR="${CMAKE_CURRENT_LIST_DIR}/test/assets")
add_test(NAME usd_read_test COMMAND usd_read_test)

# Google Benchmark (use a Release build). Not a test: it builds through the
# `benches` target and runs through scripts/bench_ledger.py.
add_executable(usd_read_bench bench/ReadBench.cpp)
target_link_libraries(usd_read_bench PRIVATE SigilUsdRead SigilUsdWrite
  SigilUsdRuntime SigilGeometryKit benchmark::benchmark)
add_dependencies(benches usd_read_bench)
