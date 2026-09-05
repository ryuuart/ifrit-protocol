# The one lane a library's STOCK shader text reaches it by: the bytes are
# compiled into the archive, so a binary carries every shader it can draw
# with wherever it runs.
#
# sigil_shader_sources(<target> DIR <dir> NAMESPACE <ns> [NAME <stem>]
#                      [PATTERNS <glob>...])
#   Globs <dir> (CONFIGURE_DEPENDS, so a file added there is picked up by
#   the next build), generates one header and one translation unit
#   carrying every file's bytes as a `std::string_view` table keyed by the
#   path relative to <dir>, and adds both — and the shader files
#   themselves, so an editor shows them beside the code — to <target>.
#   The accessor is declared in <ns> and reached as
#   <sigilshaders/<stem>.h>, where <stem> defaults to the target's name
#   without its Sigil prefix. PATTERNS defaults to *.sksl and *.slang.
#
# One accessor per target and no re-export: a consumer that needs another
# library's shader text asks that library for it by name.
#
# A shader a user authored is a different thing and arrives a different
# way — by URI through SigilIO, from wherever the user keeps it. What is
# embedded here is only what this repository ships.

set(SIGIL_SHADER_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/shaders"
    CACHE INTERNAL "generated shader-source tables")

function(sigil_shader_sources target)
  cmake_parse_arguments(ARG "" "DIR;NAMESPACE;NAME" "PATTERNS" ${ARGN})
  if(NOT ARG_DIR OR NOT ARG_NAMESPACE)
    message(FATAL_ERROR
      "sigil_shader_sources(${target}): DIR and NAMESPACE are required")
  endif()
  if(NOT ARG_NAME)
    string(REGEX REPLACE "^Sigil" "" ARG_NAME "${target}")
  endif()
  if(NOT ARG_PATTERNS)
    set(ARG_PATTERNS "*.sksl" "*.slang")
  endif()

  get_filename_component(_dir "${ARG_DIR}" ABSOLUTE)
  set(_globs)
  foreach(_pattern IN LISTS ARG_PATTERNS)
    list(APPEND _globs "${_dir}/${_pattern}" "${_dir}/*/${_pattern}")
  endforeach()
  file(GLOB_RECURSE _files CONFIGURE_DEPENDS ${_globs})
  list(SORT _files)
  if(NOT _files)
    message(FATAL_ERROR
      "sigil_shader_sources(${target}): ${_dir} holds no shader")
  endif()

  set(_header "${SIGIL_SHADER_GENERATED_DIR}/sigilshaders/${ARG_NAME}.h")
  set(_source "${SIGIL_SHADER_GENERATED_DIR}/${ARG_NAME}.cpp")
  set(_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ShaderEmbed.cmake")
  add_custom_command(
    OUTPUT "${_header}" "${_source}"
    COMMAND ${CMAKE_COMMAND}
            -DNAME=${ARG_NAME}
            "-DSHADER_NAMESPACE=${ARG_NAMESPACE}"
            "-DDIRECTORY=${_dir}"
            "-DFILES=${_files}"
            "-DHEADER=${_header}"
            "-DSOURCE=${_source}"
            -P "${_script}"
    DEPENDS ${_files} "${_script}"
    COMMENT "embedding ${ARG_NAME} shader sources"
    VERBATIM)

  set_source_files_properties(${_files} PROPERTIES HEADER_FILE_ONLY TRUE)
  target_sources(${target} PRIVATE "${_header}" "${_source}" ${_files})
  target_include_directories(${target} PRIVATE "${SIGIL_SHADER_GENERATED_DIR}")
endfunction()
