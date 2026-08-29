# Doxygen sites for the Sigil libraries.
#
# Each library calls sigil_add_docs() next to its add_library(); the root
# calls sigil_finalize_docs() once every subdirectory has been added.
# Building the `docs` target writes one browsable site per library under
# ${CMAKE_BINARY_DIR}/docs, plus an index linking them.
#
# Generation runs in two passes because the libraries reference each
# other's types in both directions -- SigilWorld takes SigilShape's
# meshes, SigilCompose takes SigilMotion's animatables -- and a tag file
# can only be read after it is written. The first pass writes every tag
# file and no HTML; the second reads all of them and writes the HTML. A
# single pass would resolve only the edges that happen to run in the
# order the subdirectories were added.

# Where Doxyfile.in sits, captured while this file is being read so the
# functions below do not have to assume where the module was included from.
set(SIGIL_DOCS_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR})

find_package(Doxygen OPTIONAL_COMPONENTS dot)

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

# The generated sites, and nothing else: this is what gets served, and
# what someone deletes to force a clean regeneration.
set(SIGIL_DOCS_ROOT ${CMAKE_BINARY_DIR}/docs)

# Everything used to produce them — the configured Doxyfiles, the tag
# files, the theme, the generated header. Kept OUT of the output tree on
# purpose: the Doxyfiles are written by configure_file, so the build has
# no rule that can bring one back, and a `rm -rf build/docs` that took
# them with it would break the build until the next cmake run.
set(SIGIL_DOCS_WORK ${CMAKE_BINARY_DIR}/docs-build)

# Records one library. The Doxyfiles are not configured here: TAGFILES
# has to name every OTHER library, which is not known until every
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

# Writes the Doxyfiles, the two passes of build rules, and the index.
function(sigil_finalize_docs)
  get_property(libraries GLOBAL PROPERTY SIGIL_DOCS_LIBRARIES)
  if(NOT libraries)
    return()
  endif()

  # The theme and the header are fetched/generated once and shared by
  # every library's site.
  set(theme_dir ${SIGIL_DOCS_WORK}/theme)
  set(theme_css
      ${theme_dir}/doxygen-awesome.css
      ${theme_dir}/doxygen-awesome-sidebar-only.css
      ${theme_dir}/doxygen-awesome-sidebar-only-darkmode-toggle.css)
  set(theme_js
      ${theme_dir}/doxygen-awesome-darkmode-toggle.js
      ${theme_dir}/doxygen-awesome-fragment-copy-button.js
      ${theme_dir}/doxygen-awesome-paragraph-link.js
      ${theme_dir}/doxygen-awesome-interactive-toc.js)

  add_custom_command(
    OUTPUT ${theme_css} ${theme_js}
    COMMAND ${CMAKE_COMMAND} -DOUT_DIR=${theme_dir}
            -P ${SIGIL_DOCS_MODULE_DIR}/FetchTheme.cmake
    DEPENDS ${SIGIL_DOCS_MODULE_DIR}/FetchTheme.cmake
    COMMENT "Fetching the documentation theme"
    VERBATIM)

  set(header ${SIGIL_DOCS_WORK}/header.html)
  add_custom_command(
    OUTPUT ${header}
    COMMAND ${CMAKE_COMMAND} -DDOXYGEN=${DOXYGEN_EXECUTABLE}
            -DWORK=${SIGIL_DOCS_WORK}/header-work -DOUT=${header}
            -P ${SIGIL_DOCS_MODULE_DIR}/MakeHeader.cmake
    DEPENDS ${SIGIL_DOCS_MODULE_DIR}/MakeHeader.cmake
    COMMENT "Generating the documentation header"
    VERBATIM)

  set(custom_css ${SIGIL_DOCS_MODULE_DIR}/custom.css)
  string(JOIN " \\\n                         " SIGIL_DOCS_STYLESHEETS
         ${theme_css} ${custom_css})
  string(JOIN " \\\n                         " SIGIL_DOCS_EXTRA_FILES
         ${theme_js})

  if(DOXYGEN_DOT_FOUND)
    set(SIGIL_DOCS_HAVE_DOT YES)
  else()
    set(SIGIL_DOCS_HAVE_DOT NO)
  endif()

  if(SPELLCIRCLE_DOCS_WARN_UNDOCUMENTED)
    set(warn_undocumented YES)
  else()
    set(warn_undocumented NO)
  endif()

  # Pass one: every tag file, no HTML.
  set(all_tagfiles "")
  foreach(lib IN LISTS libraries)
    set(tagfile ${SIGIL_DOCS_WORK}/${lib}.tag)
    list(APPEND all_tagfiles ${tagfile})

    _sigil_docs_configure(${lib} tags
      GENERATE_HTML NO
      TAGFILE ${tagfile}
      TAGFILES_IN ""
      WARN_UNDOCUMENTED NO
      HAVE_DOT NO
      HEADER ""
      STYLESHEETS ""
      EXTRA_FILES "")

    get_property(input GLOBAL PROPERTY SIGIL_DOCS_${lib}_INPUT)
    set(sources "")
    foreach(dir IN LISTS input)
      if(IS_DIRECTORY ${dir})
        file(GLOB_RECURSE found CONFIGURE_DEPENDS
             ${dir}/*.h ${dir}/*.hpp ${dir}/*.md)
        list(APPEND sources ${found})
      else()
        list(APPEND sources ${dir})
      endif()
    endforeach()

    add_custom_command(
      OUTPUT ${tagfile}
      COMMAND ${CMAKE_COMMAND} -E make_directory
              ${SIGIL_DOCS_WORK} ${SIGIL_DOCS_ROOT}/${lib}
      COMMAND ${DOXYGEN_EXECUTABLE} ${SIGIL_DOCS_WORK}/${lib}/Doxyfile.tags
      DEPENDS ${sources} ${SIGIL_DOCS_WORK}/${lib}/Doxyfile.tags
      COMMENT "Indexing ${lib} for cross-library links"
      VERBATIM)
  endforeach()

  # Pass two: HTML, each library reading every other library's tag file.
  set(site_targets "")
  foreach(lib IN LISTS libraries)
    set(tagfiles_in "")
    foreach(other IN LISTS libraries)
      if(NOT other STREQUAL lib)
        # `tag=path` points a resolved name at the site that documents it.
        # The path is relative so the whole docs tree can be moved or served
        # from anywhere.
        list(APPEND tagfiles_in
             "${SIGIL_DOCS_WORK}/${other}.tag=../../${other}/html")
      endif()
    endforeach()
    string(JOIN " \\\n                         " tagfiles_in ${tagfiles_in})

    _sigil_docs_configure(${lib} html
      GENERATE_HTML YES
      TAGFILE ""
      TAGFILES_IN "${tagfiles_in}"
      WARN_UNDOCUMENTED ${warn_undocumented}
      HAVE_DOT ${SIGIL_DOCS_HAVE_DOT}
      HEADER ${header}
      STYLESHEETS "${SIGIL_DOCS_STYLESHEETS}"
      EXTRA_FILES "${SIGIL_DOCS_EXTRA_FILES}")

    add_custom_target(docs-${lib}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SIGIL_DOCS_ROOT}/${lib}
      COMMAND ${DOXYGEN_EXECUTABLE} ${SIGIL_DOCS_WORK}/${lib}/Doxyfile
      DEPENDS ${all_tagfiles} ${SIGIL_DOCS_WORK}/${lib}/Doxyfile
              ${theme_css} ${theme_js} ${header} ${custom_css}
      COMMENT "Writing the ${lib} documentation"
      VERBATIM)
    list(APPEND site_targets docs-${lib})
  endforeach()

  _sigil_docs_write_index("${libraries}")

  # Everything docker needs travels with the generated site, so
  # ${SIGIL_DOCS_ROOT} is a complete build context on its own.
  add_custom_target(docs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SIGIL_DOCS_ROOT}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/docs-index/index.html
            ${SIGIL_DOCS_ROOT}/index.html
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SIGIL_DOCS_MODULE_DIR}/Dockerfile
            ${SIGIL_DOCS_MODULE_DIR}/nginx.conf
            ${SIGIL_DOCS_MODULE_DIR}/dockerignore
            ${SIGIL_DOCS_ROOT}/
    COMMAND ${CMAKE_COMMAND} -E rename
            ${SIGIL_DOCS_ROOT}/dockerignore
            ${SIGIL_DOCS_ROOT}/.dockerignore
    COMMENT "Documentation written to ${SIGIL_DOCS_ROOT}/index.html")
  add_dependencies(docs ${site_targets})
endfunction()

# Configures one Doxyfile variant (`tags` or `html`) for one library.
function(_sigil_docs_configure lib variant)
  cmake_parse_arguments(
    ARG ""
    "GENERATE_HTML;TAGFILE;TAGFILES_IN;WARN_UNDOCUMENTED;HAVE_DOT;HEADER;STYLESHEETS;EXTRA_FILES"
    "" ${ARGN})

  get_property(SIGIL_DOCS_BRIEF GLOBAL PROPERTY SIGIL_DOCS_${lib}_BRIEF)
  get_property(input GLOBAL PROPERTY SIGIL_DOCS_${lib}_INPUT)
  get_property(strip GLOBAL PROPERTY SIGIL_DOCS_${lib}_STRIP)
  get_property(SIGIL_DOCS_MAINPAGE GLOBAL PROPERTY SIGIL_DOCS_${lib}_MAINPAGE)

  string(JOIN " \\\n                         " SIGIL_DOCS_INPUT ${input})
  string(JOIN " \\\n                         " SIGIL_DOCS_STRIP ${strip})

  set(SIGIL_DOCS_NAME ${lib})
  set(SIGIL_DOCS_OUTPUT ${SIGIL_DOCS_ROOT}/${lib})
  set(SIGIL_DOCS_GENERATE_HTML ${ARG_GENERATE_HTML})
  set(SIGIL_DOCS_TAGFILE ${ARG_TAGFILE})
  set(SIGIL_DOCS_TAGFILES_IN ${ARG_TAGFILES_IN})
  set(SIGIL_DOCS_WARN_UNDOCUMENTED ${ARG_WARN_UNDOCUMENTED})
  set(SIGIL_DOCS_HAVE_DOT ${ARG_HAVE_DOT})
  set(SIGIL_DOCS_HEADER ${ARG_HEADER})
  set(SIGIL_DOCS_STYLESHEETS ${ARG_STYLESHEETS})
  set(SIGIL_DOCS_EXTRA_FILES ${ARG_EXTRA_FILES})

  if(variant STREQUAL tags)
    set(out ${SIGIL_DOCS_WORK}/${lib}/Doxyfile.tags)
  else()
    set(out ${SIGIL_DOCS_WORK}/${lib}/Doxyfile)
  endif()

  configure_file(${SIGIL_DOCS_MODULE_DIR}/Doxyfile.in ${out} @ONLY)
endfunction()

# The landing page. Doxygen writes one self-contained site per library
# and has no notion of a set of them, so the set gets its own page.
function(_sigil_docs_write_index libraries)
  # Registration order is subdirectory order, which is a build concern
  # and means nothing to a reader looking for a library by name.
  list(SORT libraries CASE INSENSITIVE)

  set(cards "")
  foreach(lib IN LISTS libraries)
    get_property(brief GLOBAL PROPERTY SIGIL_DOCS_${lib}_BRIEF)
    string(APPEND cards
           "  <li><a href=\"${lib}/html/index.html\"><b>${lib}</b>"
           "<span>${brief}</span></a></li>\n")
  endforeach()

  file(WRITE ${CMAKE_BINARY_DIR}/docs-index/index.html
"<!doctype html>
<html lang=\"en\"><head><meta charset=\"utf-8\">
<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">
<title>SpellCircle libraries</title>
<style>
  :root { color-scheme: light dark; }
  body { font: 16px/1.6 -apple-system, system-ui, sans-serif;
         max-width: 46rem; margin: 4rem auto; padding: 0 1.5rem; }
  h1 { font-size: 1.5rem; margin-bottom: .25rem; }
  p.lede { color: GrayText; margin-top: 0; }
  ul { list-style: none; padding: 0; }
  li { margin: .5rem 0; }
  a { display: block; padding: .8rem 1rem; border: 1px solid;
      border-color: color-mix(in srgb, currentColor 20%, transparent);
      border-radius: .5rem; text-decoration: none; color: inherit; }
  a:hover { border-color: color-mix(in srgb, currentColor 45%, transparent); }
  b { display: block; }
  span { color: GrayText; font-size: .9rem; }
</style></head><body>
<h1>SpellCircle libraries</h1>
<p class=\"lede\">Generated from the headers. Each library's README is its
front page, and types resolve across library boundaries.</p>
<ul>
${cards}</ul>
</body></html>
")
endfunction()
