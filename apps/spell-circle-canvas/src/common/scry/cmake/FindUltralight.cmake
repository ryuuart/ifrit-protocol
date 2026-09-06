# workaround: the Ultralight SDK ships no CMake config, splits base-class
# symbols across three dylibs, sets @rpath install names with no rpath,
# and distributes its resources with the application.
#
# FindUltralight — locates the Ultralight 1.4 SDK (an ultra-light WebKit
# renderer used by SigilScry for HTML/CSS layout on the scene canvases).
#
# An installed SDK is looked for in a version directory under
# ~/.local/opt/ultralight — the per-user prefix a licensed download is
# unpacked to, highest version first, each holding the archive's include/,
# bin/ and resources/ — and then at the system prefix (headers in
# /usr/local/include/Ultralight, dylibs in /usr/local/lib).
# ULTRALIGHT_SDK_DIR names one outright and is searched before both.
#
# Runtime data (ICU tables, CA certs) comes from the SDK's resources/
# folder, which the SDK distributes with the application rather than with
# the dylibs, so a copy installed apart from an SDK root goes to one of the
# other searched locations (per-user Application Support, or
# /usr/local/share/ultralight for machine-global).
#
# AppCore's headers are included as <Ultralight/AppCore/...>, so wherever
# the SDK stands they sit inside its include/Ultralight rather than beside
# it.
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
#   app-bundling layout, and the first place the SigilScry engine looks at
#   runtime. Call it on every executable that links SigilScry.

# The SDK roots to search, in the order they win: one named on the
# command line, then each version under the per-user prefix, newest first.
set(_ultralight_roots)
if(ULTRALIGHT_SDK_DIR)
  list(APPEND _ultralight_roots "${ULTRALIGHT_SDK_DIR}")
endif()
file(GLOB _ultralight_versions "$ENV{HOME}/.local/opt/ultralight/*")
list(SORT _ultralight_versions COMPARE NATURAL ORDER DESCENDING)
list(APPEND _ultralight_roots ${_ultralight_versions})

set(_ultralight_includes)
set(_ultralight_libs)
set(_ultralight_resources)
foreach(root IN LISTS _ultralight_roots)
  list(APPEND _ultralight_includes "${root}/include")
  # The archive ships its dylibs in bin/; an installed tree may put them
  # in lib/ instead.
  list(APPEND _ultralight_libs "${root}/bin" "${root}/lib")
  list(APPEND _ultralight_resources "${root}/resources")
endforeach()

# The SDK roots are HINTS and the system prefix is a PATH, because a PATH
# is the last place searched: an SDK unpacked for this user wins over a
# copy someone once installed machine-global.
find_path(Ultralight_INCLUDE_DIR
  NAMES Ultralight/Ultralight.h
  HINTS ${_ultralight_includes}
  PATHS /usr/local/include
)

find_library(Ultralight_LIBRARY NAMES Ultralight
             HINTS ${_ultralight_libs} PATHS /usr/local/lib)
find_library(Ultralight_AppCore_LIBRARY NAMES AppCore
             HINTS ${_ultralight_libs} PATHS /usr/local/lib)
find_library(Ultralight_WebCore_LIBRARY NAMES WebCore
             HINTS ${_ultralight_libs} PATHS /usr/local/lib)
find_library(Ultralight_UltralightCore_LIBRARY NAMES UltralightCore
             HINTS ${_ultralight_libs} PATHS /usr/local/lib)

find_path(Ultralight_RESOURCE_DIR
  NAMES icudt67l.dat
  PATHS ${_ultralight_resources}
        /usr/local/share/ultralight/resources
        "$ENV{HOME}/Library/Application Support/Ultralight/resources"
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
  )
  # The SDK dylibs carry @rpath install names and load each other as
  # siblings, so a consumer needs the SDK lib dir on its runtime search
  # path — which CMake adds for it, from the imported location. Naming it
  # here as well would put the same -rpath on the link line twice, and the
  # linker warns about that on every consumer.

  add_library(Ultralight::AppCore SHARED IMPORTED)
  set_target_properties(Ultralight::AppCore PROPERTIES
    IMPORTED_LOCATION "${Ultralight_AppCore_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Ultralight_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES Ultralight::Ultralight
  )
endif()

# The engine reads its resources from a `resources` directory beside the
# binary. A custom target rather than a POST_BUILD step, because the
# binary is a library's whole test or bench binary and the directory
# asking for the staging is not the one that created it.
function(ultralight_copy_resources target)
  if(NOT Ultralight_RESOURCE_DIR OR TARGET ${target}_ultralight_resources)
    return()
  endif()
  add_custom_target(${target}_ultralight_resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${Ultralight_RESOURCE_DIR}"
            "$<TARGET_FILE_DIR:${target}>/resources"
    COMMENT "Staging Ultralight resources next to ${target}"
    VERBATIM
  )
  add_dependencies(${target} ${target}_ultralight_resources)
endfunction()

mark_as_advanced(
  Ultralight_INCLUDE_DIR
  Ultralight_LIBRARY
  Ultralight_AppCore_LIBRARY
  Ultralight_WebCore_LIBRARY
  Ultralight_UltralightCore_LIBRARY
  Ultralight_RESOURCE_DIR
)
