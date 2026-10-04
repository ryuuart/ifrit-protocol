# Verification headers have a separate include root and require this target.
add_library(SigilComposeTesting INTERFACE)
target_include_directories(SigilComposeTesting INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(SigilComposeTesting INTERFACE SigilComposeCore
  SigilMeasure)

sigil_test(compose_test
  SOURCES test/ComposeTestChecks.cpp test/ComposeTestScene.cpp
  LIBRARIES SigilComposeCore SigilComposeTesting SigilWeavePorts)

sigil_bench(compose_bench
  SOURCES bench/ComposeChecksBench.cpp
  LIBRARIES SigilComposeCore SigilComposeTesting SigilWeavePorts)
