# The core feature: what a sketch is, what it declares about its canvas,
# what it reaches for, and the seam that says which runtime draws it.
#
# It links the resource hub (a sketch reaches for files), the comparable
# erasure the kind seam is built on, and the instruments every runtime
# times itself with — and nothing that draws: a consumer that only wants
# to know what sketches exist links this alone.
#
# The crash reporter is here rather than beside either host because BOTH
# have a guest: the live host calls into a dylib it just loaded, and the
# sweep calls into a hundred sketches in one process, and a fault inside
# one is a fault inside the host either way.
add_library(SigilSketchCore STATIC
  Assets.cpp
  Crash.cpp
  Device.cpp
  Registry.cpp
  Sources.cpp
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/Assets.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/CanvasSpec.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/Crash.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/Device.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/Kind.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/Registry.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/Session.h
  ${SIGIL_SKETCH_INCLUDE_DIR}/sigilsketch/core/Sources.h
)
target_compile_features(SigilSketchCore PUBLIC cxx_std_20)
target_include_directories(SigilSketchCore PUBLIC ${SIGIL_SKETCH_INCLUDE_DIR})
target_link_libraries(SigilSketchCore PUBLIC
  SigilCoreComparable
  SigilImageAsset
  SigilIO
  SigilVideoDecode)
set_target_properties(SigilSketchCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(sketch_core_test test/AssetsTest.cpp test/CrashTest.cpp
  test/DeviceTest.cpp
  test/RegistryTest.cpp
  test/SourcesTest.cpp)
target_include_directories(sketch_core_test PRIVATE ${SIGIL_TEST_SUPPORT_DIR})
target_link_libraries(sketch_core_test PRIVATE
  SigilSketchCore
  SigilVideoEncode
  GTest::gtest_main)
add_test(NAME sketch_core_test COMMAND sketch_core_test)

# Google Benchmark (use a Release build). Not a test: it builds through
# the `benches` target and runs through scripts/bench_ledger.py.
add_executable(sketch_core_bench bench/RegistryBench.cpp)
target_link_libraries(sketch_core_bench PRIVATE SigilSketchCore
  benchmark::benchmark)
add_dependencies(benches sketch_core_bench)
