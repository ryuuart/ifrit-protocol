# SigilComposeWeb — the live page as a leaf.
#
# Header-only adapter from a SigilScry web view to a custom leaf (Web.h).
# Defined only when the Ultralight SDK produced SigilScry, so that nothing
# in this tree links a browser engine by accident: a consumer that wants the
# leaf names this target and gets SigilScry with it.
if(TARGET SigilScry)
  sigil_library(SigilComposeWeb
    HEADERS Web.h
    INTERFACE SigilComposeCore SigilScry)

  # The web() leaf: the kernel never links SigilScry; the adapter is
  # header-only and this test exists only when the SDK produced the target.
  sigil_test(compose_test
    SOURCES test/ComposeWebTest.cpp
    LIBRARIES SigilComposeWeb SigilWeavePorts
    SUITES ComposeWeb LABELS ultralight)
  ultralight_copy_resources(compose_test)
endif()
