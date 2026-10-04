# The reload engine: a source file watched, rebuilt into a versioned
# dylib with the compiler flags the build captured, dlopened and swapped
# into the running session, the previous one kept alive while a build is
# broken — and the resident set that decides which sessions stay open.
sigil_test(sketch_test
  SOURCES test/BuildCacheTest.cpp test/BenchCadenceTest.cpp test/GuestPictureTest.cpp
          test/GuestFailureTest.cpp test/HostTest.cpp test/ResidencyTest.cpp
          test/WrittenStillTest.cpp test/ShaderReloadTest.cpp
  LIBRARIES SigilSketch SigilSketchTesting SigilMediaImageEncode
  SUITES SketchGuestPicture SketchGuestFailure SketchHostBuildDirectory
  LABELS integration e2e)

# GUEST IMAGES the host's cases adopt, so a case reaches the path a
# rebuilt workspace sketch takes without compiling one: built here, never
# by the host's compiler, and resolving the framework out of the test
# binary as a sketch resolves it out of its host. Compiled hidden, as the
# host compiles a guest, so its inline definitions join no coalescing set
# and every type it names is an identity of its own. The image guest loads
# a picture through the host's hub; the throwing guests throw from the
# kind factory, from the ABI query, and from every frame.
if(TARGET sketch_test)
  function(sigil_sketch_test_guest target)
    add_library(${target} MODULE ${ARGN})
    target_link_libraries(${target} PRIVATE $<COMPILE_ONLY:SigilSketch>
      $<COMPILE_ONLY:SigilMediaCore> $<COMPILE_ONLY:SigilIOHub>)
    set_target_properties(${target} PROPERTIES
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN ON)
    if(APPLE)
      target_link_options(${target} PRIVATE -undefined dynamic_lookup)
    endif()
    add_dependencies(sketch_test ${target})
  endfunction()
  sigil_sketch_test_guest(sketch_test_image_guest test/guest/ImageAskingGuest.cpp)
  foreach(mode IN ITEMS factory metadata frame)
    sigil_sketch_test_guest(sketch_test_${mode}_guest test/guest/ThrowingGuest.cpp)
    target_compile_definitions(sketch_test_${mode}_guest PRIVATE
      "SIGIL_TEST_THROW_${mode}=1")
  endforeach()
  # The binary is created by the library's first call, in another
  # directory, and a source's properties are read in the target's.
  set_source_files_properties(test/GuestPictureTest.cpp
    TARGET_DIRECTORY sketch_test PROPERTIES
    COMPILE_DEFINITIONS
      "SIGIL_SKETCH_IMAGE_GUEST=\"$<TARGET_FILE:sketch_test_image_guest>\"")
  set_source_files_properties(test/GuestFailureTest.cpp
    TARGET_DIRECTORY sketch_test PROPERTIES COMPILE_DEFINITIONS
      "SIGIL_SKETCH_IMAGE_GUEST=\"$<TARGET_FILE:sketch_test_image_guest>\";SIGIL_SKETCH_FACTORY_GUEST=\"$<TARGET_FILE:sketch_test_factory_guest>\";SIGIL_SKETCH_METADATA_GUEST=\"$<TARGET_FILE:sketch_test_metadata_guest>\";SIGIL_SKETCH_FRAME_GUEST=\"$<TARGET_FILE:sketch_test_frame_guest>\"")
endif()

sigil_bench(sketch_bench
  SOURCES bench/HostBench.cpp
  LIBRARIES SigilSketch SigilWeavePorts)

# The session and clock domains' agents, a leaf beside the host they
# answer for.
add_subdirectory(agent)
