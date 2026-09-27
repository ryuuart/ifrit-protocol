# workaround: the Adobe Substance 3D SDK ships a substance-config.cmake
# but no version file and no find module, and it is a licensed download
# no port can fetch, so an installed copy is located by convention rather
# than by find_package's own search.
#
# FindSubstance — locates an unpacked Substance 3D SDK. Each search root
# holds one directory per install, named for its version (`9.4.6` or
# `v9.4.6`) or left as the download's own name
# ("substance-<os>-<arch>-v<version>-<hash>"), and the newest wins. Set
# SUBSTANCE_SDK_DIR on the command line, or SUBSTANCE_SDK_DIR in the
# environment, to name one outright and skip the search.
#
# Optional by design: without an SDK the Substance feature of SigilMaterial
# still builds and answers that it is unavailable.
#
# Variables:
#   Substance_FOUND
#   SUBSTANCE_SDK_DIR   cached, the directory holding substance-config.cmake

set(SUBSTANCE_SDK_DIR "" CACHE PATH
    "Adobe Substance 3D SDK directory (holds substance-config.cmake)")

if(NOT SUBSTANCE_SDK_DIR AND DEFINED ENV{SUBSTANCE_SDK_DIR})
  set(SUBSTANCE_SDK_DIR "$ENV{SUBSTANCE_SDK_DIR}" CACHE PATH "" FORCE)
endif()

include(${CMAKE_CURRENT_LIST_DIR}/SubstanceInstall.cmake)

if(NOT SUBSTANCE_SDK_DIR)
  set(_substance_roots
      "$ENV{HOME}/.local/opt/substance"
      /usr/local/opt/substance
      /opt/homebrew/opt/substance
      /opt/substance)
  substance_newest_install(_substance_best ${_substance_roots})
  if(_substance_best)
    set(SUBSTANCE_SDK_DIR "${_substance_best}" CACHE PATH "" FORCE)
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Substance
  REQUIRED_VARS SUBSTANCE_SDK_DIR
  FAIL_MESSAGE "no Adobe Substance 3D SDK found")

if(Substance_FOUND AND NOT EXISTS "${SUBSTANCE_SDK_DIR}/substance-config.cmake")
  message(FATAL_ERROR
    "SUBSTANCE_SDK_DIR is ${SUBSTANCE_SDK_DIR}, which holds no "
    "substance-config.cmake — point it at the directory that does")
endif()
