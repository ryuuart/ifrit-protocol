# SigilComposeWeb — the live page as a leaf.
#
# Header-only adapter from a SigilScry web view to a custom leaf (Web.h).
# Defined only when the Ultralight SDK produced SigilScry, so that nothing
# in this tree links a browser engine by accident: a consumer that wants the
# leaf names this target and gets SigilScry with it.
if(TARGET SigilScry)
  add_library(SigilComposeWeb INTERFACE
    ${SIGIL_COMPOSE_INCLUDE_DIR}/sigilcompose/web/Web.h
  )
  target_link_libraries(SigilComposeWeb INTERFACE SigilComposeCore SigilScry)

  # The web() leaf: the kernel never links SigilScry; the adapter is
  # header-only and this test exists only when the SDK produced the target.
  add_executable(compose_web_test
    test/ComposeWebTest.cpp
  )
  target_include_directories(compose_web_test PRIVATE
    ${SIGIL_COMPOSE_TEST_SUPPORT_DIR})
  target_link_libraries(compose_web_test PRIVATE
    SigilComposeWeb SigilWeavePorts GTest::gtest_main)
  ultralight_copy_resources(compose_web_test)
  add_test(NAME compose_web_test COMMAND compose_web_test)
endif()
