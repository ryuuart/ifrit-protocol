# Libraries register the inputs for cross-linked Doxygen sites.
# Finalization writes their manifest after every library has registered.
# The docs command generates one site per library and a shared index.

find_package(Doxygen OPTIONAL_COMPONENTS dot)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(NOT DOXYGEN_FOUND)
  message(STATUS "Doxygen not found -- the `docs` target is unavailable")
endif()

option(SPELLCIRCLE_DOCS_WARN_UNDOCUMENTED
       "Warn about undocumented entities when building the docs" OFF)

# Records one library. Nothing is generated here: a site's cross-library
# links name every OTHER library, which is not known until every
# subdirectory has been added.
function(sigil_add_docs)
  cmake_parse_arguments(ARG "" "NAME;BRIEF;MAINPAGE" "INPUT;STRIP;INCLUDE_ROOT"
                        ${ARGN})

  if(NOT ARG_NAME)
    message(FATAL_ERROR "sigil_add_docs: NAME is required")
  endif()
  if(NOT ARG_INPUT)
    message(FATAL_ERROR "sigil_add_docs(${ARG_NAME}): INPUT is required")
  endif()

  # Doxygen resolves relative paths against its own working directory.
  # Validate and normalize every path before writing the manifest.
  foreach(name IN ITEMS INPUT STRIP INCLUDE_ROOT MAINPAGE)
    set(checked)
    foreach(entry IN LISTS ARG_${name})
      if(NOT IS_ABSOLUTE "${entry}")
        message(FATAL_ERROR
          "sigil_add_docs(${ARG_NAME}): ${name} '${entry}' must be absolute")
      endif()
      if(NOT EXISTS "${entry}")
        message(FATAL_ERROR
          "sigil_add_docs(${ARG_NAME}): ${name} '${entry}' does not exist")
      endif()
      get_filename_component(entry "${entry}" ABSOLUTE)
      list(APPEND checked "${entry}")
    endforeach()
    set(ARG_${name} ${checked})
  endforeach()

  if(NOT ARG_STRIP)
    set(ARG_STRIP ${ARG_INPUT})
  endif()
  # Without an include root of its own, a library's include lines are cut
  # at the same prefixes as its file names.
  if(NOT ARG_INCLUDE_ROOT)
    set(ARG_INCLUDE_ROOT ${ARG_STRIP})
  endif()
  # The last resort for a path under none of the above: a page named from
  # the application root rather than from the file system root.
  list(APPEND ARG_STRIP ${CMAKE_SOURCE_DIR})

  set_property(GLOBAL APPEND PROPERTY SIGIL_DOCS_LIBRARIES ${ARG_NAME})
  set_property(GLOBAL PROPERTY SIGIL_DOCS_${ARG_NAME}_BRIEF "${ARG_BRIEF}")
  set_property(GLOBAL PROPERTY SIGIL_DOCS_${ARG_NAME}_INPUT "${ARG_INPUT}")
  set_property(GLOBAL PROPERTY SIGIL_DOCS_${ARG_NAME}_STRIP "${ARG_STRIP}")
  set_property(GLOBAL PROPERTY SIGIL_DOCS_${ARG_NAME}_INCLUDE_ROOT
               "${ARG_INCLUDE_ROOT}")
  set_property(GLOBAL PROPERTY SIGIL_DOCS_${ARG_NAME}_MAINPAGE "${ARG_MAINPAGE}")
endfunction()

# Writes the manifest and adds the targets that read it.
function(sigil_finalize_docs)
  # Reference fixtures need neither Doxygen nor generated documentation.
  add_test(NAME reference_generator
    COMMAND ${Python3_EXECUTABLE} -m unittest discover
            -s "${CMAKE_SOURCE_DIR}/scripts/sigil/reference/test"
            -t "${CMAKE_SOURCE_DIR}/scripts"
            -p "test_*.py")
  if(NOT DOXYGEN_FOUND)
    return()
  endif()

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
      "# Written by sigil_finalize_docs(); read by scripts/sigil.py docs.\n")
  string(APPEND manifest "doxygen=${DOXYGEN_EXECUTABLE}\n")
  string(APPEND manifest "have_dot=${have_dot}\n")
  string(APPEND manifest "warn_undocumented=${warn_undocumented}\n")
  # The generated sites, and nothing else: this is what gets served.
  string(APPEND manifest "docs_root=${CMAKE_BINARY_DIR}/docs\n")
  # Everything used to produce them: the rendered Doxyfiles, the tag
  # files, the theme, the generated header.
  string(APPEND manifest "work=${CMAKE_BINARY_DIR}/docs-build\n")
  # The Python package's declarations, written when its extension is
  # built. They carry both the signatures and the spellings a page shows.
  string(APPEND manifest "declarations=${CMAKE_BINARY_DIR}/python/sigil\n")
  # Doxyfile.in, the stylesheet and the container files the verb renders
  # from.
  get_filename_component(templates
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../docs" ABSOLUTE)
  string(APPEND manifest "templates=${templates}\n")

  foreach(lib IN LISTS libraries)
    get_property(brief GLOBAL PROPERTY SIGIL_DOCS_${lib}_BRIEF)
    get_property(input GLOBAL PROPERTY SIGIL_DOCS_${lib}_INPUT)
    get_property(strip GLOBAL PROPERTY SIGIL_DOCS_${lib}_STRIP)
    get_property(include_root GLOBAL PROPERTY SIGIL_DOCS_${lib}_INCLUDE_ROOT)
    get_property(mainpage GLOBAL PROPERTY SIGIL_DOCS_${lib}_MAINPAGE)
    string(APPEND manifest "library=${lib}\n")
    string(APPEND manifest "brief=${brief}\n")
    string(APPEND manifest "mainpage=${mainpage}\n")
    string(APPEND manifest "input=${input}\n")
    string(APPEND manifest "strip=${strip}\n")
    string(APPEND manifest "include_root=${include_root}\n")
  endforeach()

  set(manifest_file "${CMAKE_BINARY_DIR}/docs-manifest.txt")
  file(WRITE "${manifest_file}" "${manifest}")

  set(build_docs "${CMAKE_SOURCE_DIR}/scripts/sigil.py")
  add_custom_target(docs
    COMMAND "${Python3_EXECUTABLE}" "${build_docs}" docs --manifest "${manifest_file}"
    COMMENT "Writing the documentation to ${CMAKE_BINARY_DIR}/docs/index.html"
    VERBATIM)

  foreach(lib IN LISTS libraries)
    add_custom_target(docs-${lib}
      COMMAND "${Python3_EXECUTABLE}" "${build_docs}" docs --manifest "${manifest_file}"
              --library ${lib}
      COMMENT "Writing the ${lib} documentation"
      VERBATIM)
  endforeach()
endfunction()
