# Doxygen sites for the Sigil libraries.
#
# Each library root registers itself through sigil_add_docs(); the root
# calls sigil_finalize_docs() once every subdirectory has been added,
# which records the set as a manifest and adds the targets that read it.
# Building the `docs` target writes one browsable site per library under
# ${CMAKE_BINARY_DIR}/docs, plus an index linking them.
#
# The generation itself — the two passes, the theme, the header, the
# rendered Doxyfiles, the landing page — is scripts/build_docs.py. What
# stays here is what only CMake knows: whether Doxygen is installed,
# where it is, and which libraries registered themselves.

# Where Doxyfile.in and the container files sit, captured while this file
# is being read so the functions below do not have to assume where the
# module was included from.
set(SIGIL_DOCS_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR})

find_package(Doxygen OPTIONAL_COMPONENTS dot)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(NOT DOXYGEN_FOUND)
  message(STATUS "Doxygen not found -- the `docs` target is unavailable")
  function(sigil_add_docs)
  endfunction()
  function(sigil_finalize_docs)
  endfunction()
  return()
endif()

option(SPELLCIRCLE_DOCS_WARN_UNDOCUMENTED
       "Warn about undocumented entities when building the docs" OFF)

# Records one library. Nothing is generated here: a site's cross-library
# links name every OTHER library, which is not known until every
# subdirectory has been added.
function(sigil_add_docs)
  cmake_parse_arguments(ARG "" "NAME;BRIEF;MAINPAGE" "INPUT;STRIP" ${ARGN})

  if(NOT ARG_NAME)
    message(FATAL_ERROR "sigil_add_docs: NAME is required")
  endif()
  if(NOT ARG_INPUT)
    message(FATAL_ERROR "sigil_add_docs(${ARG_NAME}): INPUT is required")
  endif()

  if(NOT ARG_STRIP)
    set(ARG_STRIP ${ARG_INPUT})
  endif()

  set_property(GLOBAL APPEND PROPERTY SIGIL_DOCS_LIBRARIES ${ARG_NAME})
  set_property(GLOBAL PROPERTY SIGIL_DOCS_${ARG_NAME}_BRIEF "${ARG_BRIEF}")
  set_property(GLOBAL PROPERTY SIGIL_DOCS_${ARG_NAME}_INPUT "${ARG_INPUT}")
  set_property(GLOBAL PROPERTY SIGIL_DOCS_${ARG_NAME}_STRIP "${ARG_STRIP}")
  set_property(GLOBAL PROPERTY SIGIL_DOCS_${ARG_NAME}_MAINPAGE "${ARG_MAINPAGE}")
endfunction()

# Writes the manifest and adds the targets that read it.
function(sigil_finalize_docs)
  get_property(libraries GLOBAL PROPERTY SIGIL_DOCS_LIBRARIES)
  if(NOT libraries)
    return()
  endif()

  if(DOXYGEN_DOT_FOUND)
    set(have_dot YES)
  else()
    set(have_dot NO)
  endif()
  if(SPELLCIRCLE_DOCS_WARN_UNDOCUMENTED)
    set(warn_undocumented YES)
  else()
    set(warn_undocumented NO)
  endif()

  # One `key=value` per line, a `library=` line opening each record. The
  # manifest sits beside the other files CMake writes for the build
  # rather than in the work directory, so both directories the docs
  # build produces stay disposable.
  set(manifest
      "# Written by sigil_finalize_docs(); read by scripts/build_docs.py.\n")
  string(APPEND manifest "doxygen=${DOXYGEN_EXECUTABLE}\n")
  string(APPEND manifest "have_dot=${have_dot}\n")
  string(APPEND manifest "warn_undocumented=${warn_undocumented}\n")
  # The generated sites, and nothing else: this is what gets served.
  string(APPEND manifest "docs_root=${CMAKE_BINARY_DIR}/docs\n")
  # Everything used to produce them: the rendered Doxyfiles, the tag
  # files, the theme, the generated header.
  string(APPEND manifest "work=${CMAKE_BINARY_DIR}/docs-build\n")
  string(APPEND manifest "module_dir=${SIGIL_DOCS_MODULE_DIR}\n")

  foreach(lib IN LISTS libraries)
    get_property(brief GLOBAL PROPERTY SIGIL_DOCS_${lib}_BRIEF)
    get_property(input GLOBAL PROPERTY SIGIL_DOCS_${lib}_INPUT)
    get_property(strip GLOBAL PROPERTY SIGIL_DOCS_${lib}_STRIP)
    get_property(mainpage GLOBAL PROPERTY SIGIL_DOCS_${lib}_MAINPAGE)
    string(APPEND manifest "library=${lib}\n")
    string(APPEND manifest "brief=${brief}\n")
    string(APPEND manifest "mainpage=${mainpage}\n")
    string(APPEND manifest "input=${input}\n")
    string(APPEND manifest "strip=${strip}\n")
  endforeach()

  set(manifest_file ${CMAKE_BINARY_DIR}/docs-manifest.txt)
  file(WRITE ${manifest_file} "${manifest}")

  set(build_docs ${CMAKE_SOURCE_DIR}/scripts/build_docs.py)
  add_custom_target(docs
    COMMAND ${Python3_EXECUTABLE} ${build_docs} --manifest ${manifest_file}
    COMMENT "Writing the documentation to ${CMAKE_BINARY_DIR}/docs/index.html"
    VERBATIM)
  foreach(lib IN LISTS libraries)
    add_custom_target(docs-${lib}
      COMMAND ${Python3_EXECUTABLE} ${build_docs} --manifest ${manifest_file}
              --library ${lib}
      COMMENT "Writing the ${lib} documentation"
      VERBATIM)
  endforeach()
endfunction()
