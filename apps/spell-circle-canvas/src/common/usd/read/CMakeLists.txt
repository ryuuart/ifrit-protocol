# The read feature: a stage poured into geometry's decode::Model. Links
# the codec feature the Model comes from; OpenUSD is private to it.
add_library(SigilUsdRead STATIC
  GfMatrix.h
  Instancer.cpp
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
  PUBLIC SigilGeometryMeshCodec SigilGeometryMesh
  PRIVATE usd usdGeom usdShade sdf tf gf vt)

# Links the library it exercises, the runtime probe it skips on, and the
# writer whose stages it round-trips; the committed fixtures under
# test/assets/ reach it through the IFRIT_USD_TEST_ASSET_DIR definition.
add_executable(usd_read_test test/ReadTest.cpp)
target_link_libraries(usd_read_test PRIVATE SigilUsdRead SigilUsdWrite
  SigilUsdRuntime GTest::gtest_main)
target_compile_definitions(usd_read_test PRIVATE
  IFRIT_USD_TEST_ASSET_DIR="${CMAKE_CURRENT_LIST_DIR}/test/assets")
add_test(NAME usd_read_test COMMAND usd_read_test)

# Google Benchmark (use a Release build). Not a test: it builds through the
# `benches` target and runs through scripts/bench_ledger.py.
add_executable(usd_read_bench bench/ReadBench.cpp)
target_link_libraries(usd_read_bench PRIVATE SigilUsdRead SigilUsdWrite
  SigilUsdRuntime benchmark::benchmark)
add_dependencies(benches usd_read_bench)
