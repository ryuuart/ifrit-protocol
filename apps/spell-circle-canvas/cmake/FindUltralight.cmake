# FindUltralight — locates the Ultralight 1.4 SDK (an ultra-light WebKit
# renderer used by SigilUltralight for HTML/CSS layout on the scene canvases).
#
# The SDK is expected at a standard prefix (/usr/local on the dev Macs:
# headers in /usr/local/include/Ultralight, dylibs in /usr/local/lib).
# Runtime data (ICU tables, CA certs) comes from the SDK archive's
# resources/ folder; per Ultralight's docs it is distributed with the
# application rather than installed with the dylibs, so install it to one
# of the searched locations below (per-user Application Support, or
# /usr/local/share/ultralight for machine-global).
#
# See src/common/ultralight/README.md for SDK installation instructions. Pass
# -DULTRALIGHT_SDK_DIR=/path/to/extracted-sdk to resolve the resources
# straight out of an SDK archive instead of an installed location.
#
# Imported targets:
#   Ultralight::Ultralight  core library (ultralight::* API)
#   Ultralight::AppCore     platform helpers (native font loader, etc.)
#
# Variables:
#   Ultralight_FOUND
#   Ultralight_RESOURCE_DIR  directory containing icudt67l.dat + cacert.pem
#
# Functions:
#   ultralight_copy_resources(<target>)  post-build, stages the resources
#   at $<TARGET_FILE_DIR:target>/resources — Ultralight's standard
#   app-bundling layout, and the first place the SigilUltralight engine looks at
#   runtime. Call it on every executable that links SigilUltralight.

find_path(Ultralight_INCLUDE_DIR
  NAMES Ultralight/Ultralight.h
  PATHS /usr/local/include
)

find_library(Ultralight_LIBRARY NAMES Ultralight PATHS /usr/local/lib)
find_library(Ultralight_AppCore_LIBRARY NAMES AppCore PATHS /usr/local/lib)
find_library(Ultralight_WebCore_LIBRARY NAMES WebCore PATHS /usr/local/lib)
find_library(Ultralight_UltralightCore_LIBRARY NAMES UltralightCore
             PATHS /usr/local/lib)

find_path(Ultralight_RESOURCE_DIR
  NAMES icudt67l.dat
  PATHS /usr/local/share/ultralight/resources
        "$ENV{HOME}/Library/Application Support/Ultralight/resources"
        "${ULTRALIGHT_SDK_DIR}/resources"
  NO_DEFAULT_PATH
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Ultralight
  REQUIRED_VARS
    Ultralight_INCLUDE_DIR
    Ultralight_LIBRARY
    Ultralight_AppCore_LIBRARY
    Ultralight_WebCore_LIBRARY
    Ultralight_UltralightCore_LIBRARY
    Ultralight_RESOURCE_DIR
)

if(Ultralight_FOUND AND NOT TARGET Ultralight::Ultralight)
  get_filename_component(_ultralight_libdir "${Ultralight_LIBRARY}" DIRECTORY)

  # ultralight::* base-class symbols are split across the core dylibs
  # (e.g. FileSystem/Logger dtors live in UltralightCore), so all three
  # non-AppCore dylibs link as one unit.
  add_library(Ultralight::WebCore SHARED IMPORTED)
  set_target_properties(Ultralight::WebCore PROPERTIES
    IMPORTED_LOCATION "${Ultralight_WebCore_LIBRARY}")

  add_library(Ultralight::UltralightCore SHARED IMPORTED)
  set_target_properties(Ultralight::UltralightCore PROPERTIES
    IMPORTED_LOCATION "${Ultralight_UltralightCore_LIBRARY}")

  add_library(Ultralight::Ultralight SHARED IMPORTED)
  set_target_properties(Ultralight::Ultralight PROPERTIES
    IMPORTED_LOCATION "${Ultralight_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Ultralight_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES
      "Ultralight::WebCore;Ultralight::UltralightCore"
    # The SDK dylibs carry @rpath install names and load each other as
    # siblings, so consumers need the SDK lib dir on their runtime
    # search path.
    INTERFACE_LINK_OPTIONS "-Wl,-rpath,${_ultralight_libdir}"
  )

  add_library(Ultralight::AppCore SHARED IMPORTED)
  set_target_properties(Ultralight::AppCore PROPERTIES
    IMPORTED_LOCATION "${Ultralight_AppCore_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Ultralight_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES Ultralight::Ultralight
  )

  unset(_ultralight_libdir)
endif()

function(ultralight_copy_resources target)
  if(NOT Ultralight_RESOURCE_DIR)
    return()
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${Ultralight_RESOURCE_DIR}"
            "$<TARGET_FILE_DIR:${target}>/resources"
    COMMENT "Staging Ultralight resources next to ${target}"
    VERBATIM
  )
endfunction()

mark_as_advanced(
  Ultralight_INCLUDE_DIR
  Ultralight_LIBRARY
  Ultralight_AppCore_LIBRARY
  Ultralight_WebCore_LIBRARY
  Ultralight_UltralightCore_LIBRARY
  Ultralight_RESOURCE_DIR
)
