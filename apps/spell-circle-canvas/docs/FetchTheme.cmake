# The documentation theme, fetched from the network into the build tree.
#
# Doxygen's stock HTML is close to unusable on a phone: fixed-width
# tables, a navigation tree that assumes a mouse, and no viewport-aware
# layout. Doxygen Awesome replaces the stylesheet without touching the
# generated HTML structure, so the fix is a download rather than a fork.
#
# Run as a script, not included:
#
#     cmake -DOUT_DIR=<dir> -P FetchTheme.cmake
#
# The `docs` target does this for you; nothing here runs during a normal
# build, and configuring the project never touches the network.
#
# Rules, as for any other fetched dependency:
#  * an OPEN licence, and the licence file is fetched alongside;
#  * pinned to an immutable commit, never a branch or a tag, so the URL
#    cannot change under the hash;
#  * an EXPECTED_HASH per file, so a changed byte is a hard failure and
#    not a silent substitution;
#  * never vendored into the source tree.

# jothepro/doxygen-awesome-css, MIT. The commit release v2.4.2 points at.
set(_theme_commit d52eafe3e9303399fda15661f3d7bb8fe3d7eabc)
set(_theme_base
    "https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/${_theme_commit}")

# file | sha256
set(_theme_manifest
    "doxygen-awesome.css|5ec49e2dfd097f6b5384e3aae0476eab47748e311fc70e207925f8fcc37477b9"
    "doxygen-awesome-sidebar-only.css|dc7ddd235375b71ecb0af920faa6b925ee9445ac617f3bc962b0b0db97da7b4f"
    "doxygen-awesome-sidebar-only-darkmode-toggle.css|c1939ca910d2282068482abc72e9edcf9835e4de153ebe8b428cbace92ed4c2c"
    "doxygen-awesome-darkmode-toggle.js|de752867789ed21154983c22ef34441137b4cc558d5a2f92013f5b894483e5a4"
    "doxygen-awesome-fragment-copy-button.js|009b4c9982c18bc68c6366321298316e9054a620e37b99de1276ff6a1e2c65a0"
    "doxygen-awesome-paragraph-link.js|f9fe333b516cdc259a25475b0ca472e8e091fd7abf9020e54949c4677a7a427f"
    "doxygen-awesome-interactive-toc.js|a7d6a4d59809b650afd011af6fc8805075aeb5e310940fb9583a42652fe87ba8"
    "LICENSE|e3da754c3f657cc78594fa2e8a3283665f78c743df2485fa9e498a8973051191")

if(NOT OUT_DIR)
  message(FATAL_ERROR "FetchTheme.cmake: -DOUT_DIR=<dir> is required")
endif()

foreach(entry IN LISTS _theme_manifest)
  string(REPLACE "|" ";" parts "${entry}")
  list(GET parts 0 name)
  list(GET parts 1 hash)
  set(dest "${OUT_DIR}/${name}")

  # A file whose bytes already match is not re-fetched, so a rebuilt
  # docs target is offline after the first run.
  if(EXISTS "${dest}")
    file(SHA256 "${dest}" have)
    if(have STREQUAL hash)
      continue()
    endif()
  endif()

  message(STATUS "Fetching ${name}")
  file(DOWNLOAD "${_theme_base}/${name}" "${dest}"
       EXPECTED_HASH SHA256=${hash}
       TLS_VERIFY ON
       STATUS status)
  list(GET status 0 code)
  if(NOT code EQUAL 0)
    list(GET status 1 text)
    file(REMOVE "${dest}")
    message(FATAL_ERROR "Fetching ${name} failed: ${text}")
  endif()
endforeach()
