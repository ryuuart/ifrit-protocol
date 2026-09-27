# The reload engine: a source file watched, rebuilt into a versioned
# dylib with the compiler flags the build captured, dlopened and swapped
# into the running session, the previous one kept alive while a build is
# broken — and the resident set that decides which sessions stay open.
sigil_test(sketch_test
  SOURCES test/BuildCacheTest.cpp test/BenchCadenceTest.cpp test/GuestPictureTest.cpp
          test/HostTest.cpp test/ResidencyTest.cpp
          test/WrittenStillTest.cpp test/ShaderReloadTest.cpp
  LIBRARIES SigilSketch SigilSketchTesting SigilMediaImageEncode)

# GUEST IMAGES the host's cases adopt, so a case reaches the path a
# rebuilt workspace sketch takes without compiling one: built here, never
# by the host's compiler, and resolving the framework out of the test
# binary as a sketch resolves it out of Sketchbook. Compiled hidden, as the
# host compiles a guest, so its inline definitions join no coalescing set
# and every type it names is an identity of its own. One asks for a
# shader, one loads a picture through the host's hub.
if(TARGET sketch_test)
  foreach(guest shader image)
    set(guestTarget sketch_test_${guest}_guest)
    string(TOUPPER ${guest} guestWord)
    if(guest STREQUAL "shader")
      add_library(${guestTarget} MODULE test/guest/ShaderAskingGuest.cpp)
      set(guestCase test/ShaderReloadTest.cpp)
    else()
      add_library(${guestTarget} MODULE test/guest/ImageAskingGuest.cpp)
      set(guestCase test/GuestPictureTest.cpp)
    endif()
    target_link_libraries(${guestTarget} PRIVATE $<COMPILE_ONLY:SigilSketch>)
    set_target_properties(${guestTarget} PROPERTIES
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN ON)
    if(APPLE)
      target_link_options(${guestTarget} PRIVATE -undefined dynamic_lookup)
    endif()
    add_dependencies(sketch_test ${guestTarget})
    # The binary is created by the library's first call, in another
    # directory, and a source's properties are read in the target's.
    set_source_files_properties(${guestCase}
      TARGET_DIRECTORY sketch_test PROPERTIES
      COMPILE_DEFINITIONS
        "SIGIL_SKETCH_${guestWord}_GUEST=\"$<TARGET_FILE:${guestTarget}>\"")
  endforeach()
endif()

sigil_bench(sketch_bench
  SOURCES bench/HostBench.cpp
  LIBRARIES SigilSketch)

# The session and clock domains' agents, a leaf beside the host they
# answer for.
add_subdirectory(agent)
