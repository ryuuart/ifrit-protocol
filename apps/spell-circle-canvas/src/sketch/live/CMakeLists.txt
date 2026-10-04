# Native sessions, module loading and source rebuilds with captured toolchain
# flags. A failed replacement leaves the current session running.
sigil_test(sketch_test
  SOURCES test/BuildCacheTest.cpp test/BenchCadenceTest.cpp test/GuestPictureTest.cpp
          test/HostTest.cpp test/PluginTest.cpp test/ResidencyTest.cpp
          test/WrittenStillTest.cpp test/ShaderReloadTest.cpp
  LIBRARIES SigilSketch SigilSketchTesting SigilMediaImageEncode
  SUITES SketchPlugin SketchGuestPicture SketchHostBuildDirectory
  LABELS integration e2e)

# A GUEST IMAGE the host's cases adopt, so a case reaches the path a
# rebuilt workspace sketch takes without compiling one: built here, never
# by the host's compiler, and resolving the framework out of the test
# binary as a sketch resolves it out of Sketchbook. Compiled hidden, as the
# host compiles a guest, so its inline definitions join no coalescing set
# and every type it names is an identity of its own. It loads a picture
# through the host's hub.
if(TARGET sketch_test)
  sigil_sketch_plugin(sketch_test_image_guest
    SOURCES test/guest/ImageAskingGuest.cpp
    LIBRARIES SigilMediaCore SigilIOHub)
  foreach(mode IN ITEMS factory metadata frame)
    sigil_sketch_plugin(sketch_test_${mode}_guest
      SOURCES test/guest/ThrowingGuest.cpp)
    target_compile_definitions(sketch_test_${mode}_guest PRIVATE
      "SIGIL_TEST_THROW_${mode}=1")
    add_dependencies(sketch_test sketch_test_${mode}_guest)
  endforeach()
  # The binary is created by the library's first call, in another
  # directory, and a source's properties are read in the target's.
  set_source_files_properties(test/PluginTest.cpp
    TARGET_DIRECTORY sketch_test PROPERTIES COMPILE_DEFINITIONS
      "SIGIL_SKETCH_IMAGE_GUEST=\"$<TARGET_FILE:sketch_test_image_guest>\";SIGIL_SKETCH_FACTORY_GUEST=\"$<TARGET_FILE:sketch_test_factory_guest>\";SIGIL_SKETCH_METADATA_GUEST=\"$<TARGET_FILE:sketch_test_metadata_guest>\";SIGIL_SKETCH_FRAME_GUEST=\"$<TARGET_FILE:sketch_test_frame_guest>\"")
  add_dependencies(sketch_test sketch_test_image_guest)
  set_source_files_properties(test/GuestPictureTest.cpp
    TARGET_DIRECTORY sketch_test PROPERTIES
    COMPILE_DEFINITIONS
      "SIGIL_SKETCH_IMAGE_GUEST=\"$<TARGET_FILE:sketch_test_image_guest>\"")
endif()

sigil_bench(sketch_bench
  SOURCES bench/HostBench.cpp
  LIBRARIES SigilSketch SigilWeavePorts)

# The session and clock domains' agents, a leaf beside the host they
# answer for.
add_subdirectory(agent)
