# SigilComposeWeb — the live page as a leaf.
#
# The adapter from a SigilScry web view to a custom leaf (Web.h), drawn in
# Web.cpp so the header names no renderer type.
# Defined only when the Ultralight SDK produced SigilScry, so that nothing
# in this tree links a browser engine by accident: a consumer that wants the
# leaf names this target and gets SigilScry with it.
if(TARGET SigilScry)
  sigil_library(SigilComposeWeb
    SOURCES Web.cpp
    HEADERS Web.h
    PUBLIC SigilComposeCore SigilMaterial SigilScry
    PRIVATE SigilDraw)

  # The web() leaf: the kernel never links SigilScry; this test exists only
  # when the SDK produced the target.
  sigil_test(compose_test
    SOURCES test/ComposeWebTest.cpp
    LIBRARIES SigilComposeWeb SigilWeavePorts
    SUITES ComposeWeb LABELS ultralight)
  ultralight_copy_resources(compose_test)
endif()
