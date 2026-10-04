# The web leaf is available only when the browser SDK produced its target.
if(TARGET SigilScry)
  sigil_library(SigilComposeWeb
    SOURCES Web.cpp
    HEADERS Web.h
    PUBLIC SigilComposeCore SigilMaterial SigilScry
    PRIVATE SigilDraw)

  sigil_test(compose_test
    SOURCES test/ComposeWebTest.cpp
    LIBRARIES SigilComposeWeb SigilWeavePorts
    SUITES ComposeWeb LABELS ultralight)
  ultralight_copy_resources(compose_test)
endif()
