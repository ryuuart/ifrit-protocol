# The platform leaf: what Ultralight's Platform singleton is handed — a
# surface whose pixel store is an SkBitmap, a file system with a root
# for the SDK's resources and a root for page content, a logger that
# bridges to a callback, and the resource directory resolved from the
# configured path, the folder staged next to the executable, or the SDK
# location found at configure time. Its public headers are the log level
# and the probe that answers whether that resolved directory actually
# holds what an engine needs to boot; everything else names Ultralight
# types and stays beside the sources.
add_library(SigilScryPlatform STATIC
  FileSystem.cpp
  FileSystem.h
  Logger.cpp
  Logger.h
  ResourceDir.cpp
  ResourceDir.h
  Runtime.cpp
  SkiaSurface.cpp
  SkiaSurface.h
  Utf8.h
  ${SIGIL_SCRY_INCLUDE_DIR}/sigilscry/platform/LogLevel.h
  ${SIGIL_SCRY_INCLUDE_DIR}/sigilscry/platform/Runtime.h
)
target_compile_features(SigilScryPlatform PUBLIC cxx_std_20)
target_include_directories(SigilScryPlatform PUBLIC ${SIGIL_SCRY_INCLUDE_DIR})
target_link_libraries(SigilScryPlatform
  PUBLIC unofficial::skia::skia
  PRIVATE Ultralight::Ultralight)

# Fallback resource location (icudt67l.dat, cacert.pem) compiled in;
# WebEngineConfig::resourceDir overrides it at runtime.
target_compile_definitions(SigilScryPlatform PRIVATE
  IFRIT_WEB_DEFAULT_RESOURCE_DIR="${Ultralight_RESOURCE_DIR}")

# Links the library it exercises and Ultralight for the types its
# handlers take; no renderer is created, so it shares a process with
# nothing. The staged resources are what the resource-directory test
# looks for.
add_executable(scry_platform_test test/PlatformTest.cpp)
target_include_directories(scry_platform_test PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(scry_platform_test PRIVATE SigilScryPlatform
  Ultralight::Ultralight GTest::gtest_main)
ultralight_copy_resources(scry_platform_test)
add_test(NAME scry_platform_test COMMAND scry_platform_test)

# Google Benchmark (use a Release build). Not a test: it builds through the
# `benches` target and runs through scripts/bench_ledger.py.
add_executable(scry_platform_bench bench/PlatformBench.cpp)
target_include_directories(scry_platform_bench PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(scry_platform_bench PRIVATE SigilScryPlatform
  Ultralight::Ultralight benchmark::benchmark)
ultralight_copy_resources(scry_platform_bench)
add_dependencies(benches scry_platform_bench)
