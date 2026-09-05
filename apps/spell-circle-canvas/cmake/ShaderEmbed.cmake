# Script mode: writes the header and the translation unit carrying one
# directory of shader text.
#
# Inputs: -DNAME=<stem> -DSHADER_NAMESPACE=<ns> -DDIRECTORY=<dir>
#         -DFILES=<file;file;...> -DHEADER=<out.h> -DSOURCE=<out.cpp>
#
# Each file's bytes become a raw string literal in a table keyed by the
# path relative to DIRECTORY. A raw literal carries the shader verbatim,
# so nothing in it has to be escaped — the one text that cannot be
# carried is one containing the terminator itself, which is a build
# failure here rather than a mangled body at run time.

set(_terminator ")SHADER\"")

set(_entries "")
set(_names "")
foreach(_file IN LISTS FILES)
  file(RELATIVE_PATH _key "${DIRECTORY}" "${_file}")
  file(READ "${_file}" _text)
  string(FIND "${_text}" "${_terminator}" _clash)
  if(NOT _clash EQUAL -1)
    message(FATAL_ERROR "${_file} contains the raw literal terminator")
  endif()
  string(APPEND _entries "    {\"${_key}\", R\"SHADER(\n${_text})SHADER\"},\n")
  list(APPEND _names "${_key}")
endforeach()

set(_out "#pragma once\n")
string(APPEND _out "// Generated from the shader directory. Do not edit.\n\n")
string(APPEND _out "#include <span>\n#include <string_view>\n\n")
string(APPEND _out "namespace ${SHADER_NAMESPACE} {\n\n")
string(APPEND _out
  "/** One stock shader: its path beneath this library's shader directory,\n"
  " *  and its text, compiled into the archive. */\n"
  "struct ShaderSource {\n"
  "  std::string_view name;\n"
  "  std::string_view text;\n"
  "};\n\n")
string(APPEND _out
  "/** The text of the stock shader @p name, or an empty view when this\n"
  " *  library ships no such file. */\n"
  "std::string_view shaderSource(std::string_view name);\n\n")
string(APPEND _out
  "/** Every stock shader this library ships, in name order. */\n"
  "std::span<const ShaderSource> shaderSources();\n\n")
string(APPEND _out "}  // namespace ${SHADER_NAMESPACE}\n")

file(WRITE "${HEADER}.tmp" "${_out}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HEADER}.tmp" "${HEADER}")
file(REMOVE "${HEADER}.tmp")

set(_body "// Generated from the shader directory. Do not edit.\n\n")
string(APPEND _body "#include <sigilshaders/${NAME}.h>\n\n")
string(APPEND _body "namespace ${SHADER_NAMESPACE} {\n\n")
string(APPEND _body "namespace {\n\n")
string(APPEND _body "constexpr ShaderSource kSources[] = {\n")
string(APPEND _body "${_entries}")
string(APPEND _body "};\n\n}  // namespace\n\n")
string(APPEND _body
  "std::string_view shaderSource(std::string_view name) {\n"
  "  for (const ShaderSource& source : kSources)\n"
  "    if (source.name == name) return source.text;\n"
  "  return {};\n"
  "}\n\n")
string(APPEND _body
  "std::span<const ShaderSource> shaderSources() { return kSources; }\n\n")
string(APPEND _body "}  // namespace ${SHADER_NAMESPACE}\n")

file(WRITE "${SOURCE}.tmp" "${_body}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SOURCE}.tmp" "${SOURCE}")
file(REMOVE "${SOURCE}.tmp")
