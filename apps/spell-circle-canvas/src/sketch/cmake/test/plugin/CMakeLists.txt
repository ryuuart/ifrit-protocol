# Entered on its own as an external project, and through add_subdirectory
# from the main tree, whose policy level it must not lower.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.28)
endif()
if(TARGET SigilSketch)
  sigil_sketch_plugin(sketch_test_sdk_plugin
    SOURCES Example.cpp LIBRARIES SigilComposeCore)
else()
  project(SigilPluginExample LANGUAGES CXX)
  find_package(SigilSketchSDK CONFIG REQUIRED)
  add_library(ExampleCompose ALIAS SigilComposeCore)
  set(SIGIL_TEST_PLUGIN_LIBRARIES ExampleCompose CACHE STRING "Origin targets consumed by the plugin fixture")
  sigil_sketch_plugin(example SOURCES Example.cpp LIBRARIES ${SIGIL_TEST_PLUGIN_LIBRARIES})
  if(SigilComposeKit IN_LIST SIGIL_TEST_PLUGIN_LIBRARIES)
    target_compile_definitions(example PRIVATE SIGIL_TEST_PLUGIN_USE_KIT)
  endif()
endif()
