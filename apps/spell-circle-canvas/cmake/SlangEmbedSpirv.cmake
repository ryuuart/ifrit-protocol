# Script mode: writes one C++ header carrying a compiled entry point's
# SPIR-V words.
#
# Inputs: -DNAME=<stem> -DENTRY=<entry> -DSPIRV=<file.spv> -DHEADER=<out.h>
#
# The module reaches the renderer as an array rather than as a file: a
# shader that had to be found on disk at run time would be a second way
# for a build to be incomplete, and the words a device runs would then be
# able to differ from the ones the build compiled.
#
# The bytes are turned into words without arithmetic. A SPIR-V file is
# little-endian words, so four hex byte pairs reversed ARE the literal,
# and reversing them is string work — which is what makes this fast
# enough to run on every shader change.

file(READ "${SPIRV}" _hex HEX)
string(LENGTH "${_hex}" _length)
math(EXPR _remainder "${_length} % 8")
if(NOT _remainder EQUAL 0)
  message(FATAL_ERROR "${SPIRV} is not a whole number of SPIR-V words")
endif()

string(REGEX MATCHALL "[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]"
       _words "${_hex}")

set(_body "")
set(_column 0)
foreach(_word IN LISTS _words)
  string(SUBSTRING "${_word}" 0 2 _b0)
  string(SUBSTRING "${_word}" 2 2 _b1)
  string(SUBSTRING "${_word}" 4 2 _b2)
  string(SUBSTRING "${_word}" 6 2 _b3)
  string(APPEND _body "0x${_b3}${_b2}${_b1}${_b0}u,")
  math(EXPR _column "${_column} + 1")
  if(_column EQUAL 6)
    string(APPEND _body "\n    ")
    set(_column 0)
  else()
    string(APPEND _body " ")
  endif()
endforeach()

set(_out "#pragma once\n")
string(APPEND _out "// Generated from ${NAME}.slang, entry ${ENTRY}. Do not edit.\n\n")
string(APPEND _out "#include <cstdint>\n\n")
string(APPEND _out "namespace sigil::slangmodule::${NAME} {\n\n")
string(APPEND _out "inline constexpr uint32_t kSpirv[] = {\n    ")
string(APPEND _out "${_body}")
string(APPEND _out "\n};\n\n")
string(APPEND _out "}  // namespace sigil::slangmodule::${NAME}\n")

# Written only when it changed, so a rebuild that produced identical
# words does not recompile everything that includes it.
file(WRITE "${HEADER}.tmp" "${_out}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HEADER}.tmp" "${HEADER}")
file(REMOVE "${HEADER}.tmp")
