# workaround: the Adobe Substance 3D SDK ships a substance-config.cmake
# but no version file and no find module, and it is a licensed download
# no port can fetch, so an installed copy is located by convention rather
# than by find_package's own search.
#
# FindSubstance — locates an unpacked Substance 3D SDK (the
# "substance-<os>-<arch>-v<version>-<hash>" download). Each search root
# holds one directory per version and the highest version wins. Set
# SUBSTANCE_SDK_DIR on the command line, or SUBSTANCE_SDK_DIR in the
# environment, to name one outright and skip the search.
#
# Optional by design: without an SDK SigilSubstance and everything that
# links it are left out of the build.
#
# Variables:
#   Substance_FOUND
#   SUBSTANCE_SDK_DIR   cached, the directory holding substance-config.cmake

set(SUBSTANCE_SDK_DIR "" CACHE PATH
    "Adobe Substance 3D SDK directory (holds substance-config.cmake)")

if(NOT SUBSTANCE_SDK_DIR AND DEFINED ENV{SUBSTANCE_SDK_DIR})
  set(SUBSTANCE_SDK_DIR "$ENV{SUBSTANCE_SDK_DIR}" CACHE PATH "" FORCE)
endif()

if(NOT SUBSTANCE_SDK_DIR)
  set(_substance_roots
      "$ENV{HOME}/.local/opt/substance"
      /usr/local/opt/substance
      /opt/homebrew/opt/substance
      /opt/substance)
  set(_substance_best "")
  set(_substance_best_version "")
  foreach(root IN LISTS _substance_roots)
    file(GLOB _substance_candidates "${root}/*")
    foreach(candidate IN LISTS _substance_candidates)
      if(NOT EXISTS "${candidate}/substance-config.cmake")
        continue()
      endif()
      # The version is the v<major>.<minor>… field of the download's name;
      # a directory renamed past recognition sorts as 0 and only wins when
      # it is the only one.
      get_filename_component(_substance_name "${candidate}" NAME)
      set(_substance_version 0)
      if(_substance_name MATCHES "v([0-9]+(\\.[0-9]+)*)")
        set(_substance_version "${CMAKE_MATCH_1}")
      endif()
      if(_substance_best_version STREQUAL ""
         OR _substance_version VERSION_GREATER _substance_best_version)
        set(_substance_best "${candidate}")
        set(_substance_best_version "${_substance_version}")
      endif()
    endforeach()
  endforeach()
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
