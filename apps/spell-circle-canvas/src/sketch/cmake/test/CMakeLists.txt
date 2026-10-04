# Native consumers exercise normal SDK targets and the separate helper
# package through the same host artifact-loading contract.
add_subdirectory(plugin)
# The native boundary fixtures declare libraries of their own and rewrite
# their inputs while the case runs, so they are configured only inside the
# separate build SketchSDK.NativeLibraryBoundary drives, which turns this on.
option(SIGIL_SKETCH_NATIVE_FIXTURES
  "Configure the native boundary fixtures that SketchSDK.NativeLibraryBoundary builds" OFF)
if(SIGIL_SKETCH_NATIVE_FIXTURES)
  add_subdirectory(native)
endif()
add_dependencies(sketch_test sketch_test_sdk_plugin)
get_target_property(sdk_directory SigilSketch SIGIL_SKETCH_SDK_DIRECTORY)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME SketchSDK.ExternalToolchain
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/SketchSDKTest.py"
    --cmake "${CMAKE_COMMAND}" --compiler "${CMAKE_CXX_COMPILER}"
    --sdk "${sdk_directory}" --config "$<CONFIG>"
    --source "${CMAKE_CURRENT_LIST_DIR}/plugin"
    --host "$<TARGET_FILE:sketch_test>"
    --in-tree-plugin "$<TARGET_FILE:sketch_test_sdk_plugin>"
    --nm "${CMAKE_NM}")
set_tests_properties(SketchSDK.ExternalToolchain PROPERTIES TIMEOUT 180
  LABELS "integration;e2e")

if(SIGIL_BUILD_APPS)
  set(native_cache)
  foreach(name IN ITEMS CMAKE_CXX_COMPILER CMAKE_TOOLCHAIN_FILE CMAKE_PREFIX_PATH
      VCPKG_TARGET_TRIPLET VCPKG_INSTALLED_DIR CMAKE_OSX_DEPLOYMENT_TARGET
      CMAKE_OSX_ARCHITECTURES CMAKE_OSX_SYSROOT)
    string(APPEND native_cache "set(${name} \"${${name}}\" CACHE STRING \"\" FORCE)\n")
  endforeach()
  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/native-cache.cmake"
    CONTENT "${native_cache}")
  add_test(NAME SketchSDK.NativeLibraryBoundary
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/NativeBoundaryTest.py"
      --cmake "${CMAKE_COMMAND}" --source "${PROJECT_SOURCE_DIR}"
      --cache "${CMAKE_CURRENT_BINARY_DIR}/native-cache.cmake" --config "$<CONFIG>")
  set_tests_properties(SketchSDK.NativeLibraryBoundary PROPERTIES TIMEOUT 1800
    RUN_SERIAL TRUE LABELS "integration;e2e")
endif()
