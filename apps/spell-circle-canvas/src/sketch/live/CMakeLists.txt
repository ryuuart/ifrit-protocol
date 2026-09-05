# The reload engine: a source file watched, rebuilt into a versioned
# dylib with the compiler flags the build captured, dlopened and swapped
# into the running session, the previous one kept alive while a build is
# broken — and the resident set that decides which sessions stay open.
sigil_test(sketch_test
  SOURCES test/HostTest.cpp test/ResidencyTest.cpp
  LIBRARIES SigilSketch)

sigil_bench(sketch_bench
  SOURCES bench/HostBench.cpp
  LIBRARIES SigilSketch)
