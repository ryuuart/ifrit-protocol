# The platform leaf: what Ultralight's Platform singleton is handed — a
# surface whose pixel store is an SkBitmap, a file system with a root
# for the SDK's resources and a root for page content, a logger that
# bridges to a callback, and the resource directory resolved from the
# configured path, the folder staged next to the executable, or the SDK
# location found at configure time. Its public headers are the log level
# and the probe that answers whether that resolved directory actually
# holds what an engine needs to boot; everything else names Ultralight
# types and stays beside the sources.
sigil_library(SigilScryPlatform
  SOURCES FileSystem.cpp FileSystem.h Logger.cpp Logger.h ResourceDir.cpp
          ResourceDir.h Runtime.cpp SkiaSurface.cpp SkiaSurface.h Utf8.h
  HEADERS LogLevel.h Runtime.h
  PUBLIC unofficial::skia::skia
  PRIVATE Ultralight::Ultralight)

# Fallback resource location (icudt67l.dat, cacert.pem) compiled in;
# WebEngineConfig::resourceDir overrides it at runtime.
target_compile_definitions(SigilScryPlatform PRIVATE
  SIGILSCRY_DEFAULT_RESOURCE_DIR="${Ultralight_RESOURCE_DIR}")

# Links the library it exercises and Ultralight for the types its
# handlers take; no renderer is created, so it shares a process with
# nothing. The staged resources are what the resource-directory test
# looks for.
sigil_test(scry_platform_test
  SOURCES test/PlatformTest.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
  LIBRARIES SigilScryPlatform Ultralight::Ultralight)
ultralight_copy_resources(scry_platform_test)

sigil_bench(scry_platform_bench
  SOURCES bench/PlatformBench.cpp
  SUPPORT_DIRS ${CMAKE_CURRENT_LIST_DIR}
  LIBRARIES SigilScryPlatform Ultralight::Ultralight)
ultralight_copy_resources(scry_platform_bench)
