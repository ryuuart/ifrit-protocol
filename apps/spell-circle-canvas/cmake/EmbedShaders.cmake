# Shader text is embedded verbatim; a raw literal terminator is refused.
set(_terminator ")SHADER\"")
set(_entries "")
foreach(_file IN LISTS FILES)
  file(RELATIVE_PATH _key "${DIRECTORY}" "${_file}")
  file(READ "${_file}" _text)
  string(FIND "${_text}" "${_terminator}" _clash)
  if(NOT _clash EQUAL -1)
    message(FATAL_ERROR "${_file} contains the raw literal terminator")
  endif()
  string(APPEND _entries "    {\"${_key}\", R\"SHADER(\n${_text})SHADER\"},\n")
endforeach()

configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/ShaderSources.h.in"
               "${HEADER}" @ONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/ShaderSources.cpp.in"
               "${SOURCE}" @ONLY)
