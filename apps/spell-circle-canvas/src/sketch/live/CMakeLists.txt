# The reload engine: a source file watched, rebuilt into a versioned
# dylib with the compiler flags the build captured, dlopened and swapped
# into the running session, the previous one kept alive while a build is
# broken — and the resident set that decides which sessions stay open.
sigil_test(sketch_test
  SOURCES test/BuildCacheTest.cpp test/BenchCadenceTest.cpp test/HostTest.cpp test/ResidencyTest.cpp
          test/WrittenStillTest.cpp test/ShaderReloadTest.cpp
  LIBRARIES SigilSketch SigilSketchTesting)

# A GUEST IMAGE the host's cases adopt, so a case reaches the path a
# rebuilt workspace sketch takes without compiling one: built here, never
# by the host's compiler, and resolving the framework out of the test
# binary as a sketch resolves it out of Sketchbook. Compiled hidden, as the
# host compiles a guest, so its inline definitions join no coalescing set.
if(TARGET sketch_test)
  add_library(sketch_test_shader_guest MODULE test/guest/ShaderAskingGuest.cpp)
  target_link_libraries(sketch_test_shader_guest PRIVATE
    $<COMPILE_ONLY:SigilSketch>)
  set_target_properties(sketch_test_shader_guest PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
  if(APPLE)
    target_link_options(sketch_test_shader_guest PRIVATE
      -undefined dynamic_lookup)
  endif()
  add_dependencies(sketch_test sketch_test_shader_guest)
  # The binary is created by the library's first call, in another
  # directory, and a source's properties are read in the target's.
  set_source_files_properties(test/ShaderReloadTest.cpp
    TARGET_DIRECTORY sketch_test PROPERTIES
    COMPILE_DEFINITIONS
      "SIGIL_SKETCH_SHADER_GUEST=\"$<TARGET_FILE:sketch_test_shader_guest>\"")
endif()

sigil_bench(sketch_bench
  SOURCES bench/HostBench.cpp
  LIBRARIES SigilSketch)

# The session and clock domains' agents, a leaf beside the host they
# answer for.
add_subdirectory(agent)
