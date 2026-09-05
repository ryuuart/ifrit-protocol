# Script mode: writes one C++ header carrying a Slang module's text.
#
# Inputs: -DNAME=<stem> -DSOURCE=<file.slang> -DHEADER=<out.h>
#
# The module reaches the program that uses it as a string rather than as
# a file, because the source it is prepended to — a material's body —
# exists only as a value in memory, and a shader that had to be found on
# disk at run time would be a second way for a build to be incomplete.

file(READ "${SOURCE}" _text)

set(_out "#pragma once\n")
string(APPEND _out "// Generated from ${NAME}.slang. Do not edit.\n\n")
string(APPEND _out "#include <string_view>\n\n")
string(APPEND _out "namespace sigil::slangmodule::${NAME} {\n\n")
# A raw string literal carries the module verbatim, delimiter and all,
# so nothing in the shader has to be escaped.
string(APPEND _out "inline constexpr std::string_view kSource = R\"SLANG(\n")
string(APPEND _out "${_text}")
string(APPEND _out ")SLANG\";\n\n")
string(APPEND _out "}  // namespace sigil::slangmodule::${NAME}\n")

# Written only when it changed, so a rebuild that produced identical
# text does not recompile everything that includes it.
file(WRITE "${HEADER}.tmp" "${_out}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HEADER}.tmp" "${HEADER}")
file(REMOVE "${HEADER}.tmp")
