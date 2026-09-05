# What this build has to arrange about the toolchain and the package
# prefixes underneath it, before the first find_package() and before the
# first target. Both entries here are workarounds; each says what it
# compensates for.

# workaround: vcpkg's toolchain orders its own two prefixes — the release
# tree and debug/ beside it — by CMAKE_BUILD_TYPE, which a multi-config
# generator never sets, and reads "not set" as Debug: the debug prefix
# lands first, and every find_library() that resolves one archive for all
# configurations answers with the debug one. A Release binary then carries
# debug archives (skia's dav1d, curl's ssl and crypto, ffmpeg's vpx and
# openh264 among them). The prefixes are re-ordered here, before the first
# find_package(), so the release tree is searched first; a package whose
# module resolves a release and a debug archive as a pair is unaffected,
# since it names each by its own variable.
#
# The order also decides which .pc file a pkg-config lookup reads, since
# FindPkgConfig derives PKG_CONFIG_PATH from CMAKE_PREFIX_PATH: both
# prefixes ship a pkgconfig/ directory, one .pc describes one build, and
# the answer is cached as one path for every configuration.
#
# One order is all a multi-config generator can have — the search paths
# are settled once, for every configuration at once — so what is chosen
# here is the answer a lookup gets when it has no way to ask per
# configuration. A Debug binary still links debug archives wherever the
# package names the release and the debug one separately, which is every
# imported target with a per-configuration location and every module that
# fills a `<name>_LIBRARY_RELEASE`/`_DEBUG` pair.
foreach(_prefix_list IN ITEMS CMAKE_PREFIX_PATH CMAKE_LIBRARY_PATH
                              CMAKE_FIND_ROOT_PATH)
  set(_suffix "")
  # Quoted, because if() dereferences an unquoted argument that names a
  # variable, and every name in this loop is one.
  if(_prefix_list STREQUAL "CMAKE_LIBRARY_PATH")
    set(_suffix "/lib/manual-link")
  endif()
  set(_release "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}${_suffix}")
  set(_debug "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug${_suffix}")
  set(_paths "${${_prefix_list}}")
  list(REMOVE_ITEM _paths "${_release}" "${_debug}")
  list(INSERT _paths 0 "${_release}" "${_debug}")
  set(${_prefix_list} "${_paths}")
endforeach()
unset(_prefix_list)
unset(_suffix)
unset(_release)
unset(_debug)
unset(_paths)

# workaround: vcpkg configs name some dependencies as imported targets and
# others as archive paths, and one archive named both ways is two link
# items CMake cannot merge, so it reaches ld64 twice and is noted there.
if(APPLE)
  include(CheckLinkerFlag)
  check_linker_flag(CXX "-Wl,-no_warn_duplicate_libraries"
    HAVE_NO_WARN_DUPLICATE_LIBRARIES) # ld64 from Xcode 15 on
  if(HAVE_NO_WARN_DUPLICATE_LIBRARIES)
    add_link_options("LINKER:-no_warn_duplicate_libraries")
  endif()
endif()
