# The reload engine: a source file watched, rebuilt into a versioned
# dylib with the compiler flags the build captured, dlopened and swapped
# into the running session, the previous one kept alive while a build is
# broken — and the resident set that decides which sessions stay open.
sigil_test(sketch_test
  SOURCES test/BuildCacheTest.cpp test/BenchCadenceTest.cpp test/HostTest.cpp test/ResidencyTest.cpp
          test/WrittenStillTest.cpp test/ShaderReloadTest.cpp
  LIBRARIES SigilSketch SigilSketchTesting)

sigil_bench(sketch_bench
  SOURCES bench/HostBench.cpp
  LIBRARIES SigilSketch)

# The session and clock domains' agents, a leaf beside the host they
# answer for.
add_subdirectory(agent)
