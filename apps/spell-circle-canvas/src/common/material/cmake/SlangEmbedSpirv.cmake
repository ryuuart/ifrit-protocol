# Script mode: writes one C++ header carrying a compiled entry point's
# SPIR-V words.
#
# Inputs: -DNAME=<stem> -DENTRY=<entry> -DSPIRV=<file.spv> -DHEADER=<out.h>
#         [-DPRECISE=ON] for a module whose float model is pinned
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

# THE FLOAT MODEL, DECLARED IN THE MODULE, for a PRECISE one. A driver
# that relaxes floating point wherever a shader does not say otherwise
# reads one statement: the entry point's `SignedZeroInfNanPreserve`
# execution mode at 32 bits, which a relaxed compile could not honour.
# The compiler has no option that writes it, so it is written here, into
# the words the device runs: the capability after the header, the mode
# after the entry point. Neither needs an id of its own, so the module's
# bound stands; the mode is core from SPIR-V 1.4, which is the oldest
# module this accepts.
if(PRECISE)
  # A word's value from its bytes as the file stores them.
  function(_spirv_value stored out)
    string(SUBSTRING "${stored}" 0 2 _b0)
    string(SUBSTRING "${stored}" 2 2 _b1)
    string(SUBSTRING "${stored}" 4 2 _b2)
    string(SUBSTRING "${stored}" 6 2 _b3)
    math(EXPR _value "0x${_b3}${_b2}${_b1}${_b0}")
    set(${out} ${_value} PARENT_SCOPE)
  endfunction()
  # …and the bytes a value is stored as.
  function(_spirv_stored value out)
    math(EXPR _hexadecimal "${value}" OUTPUT_FORMAT HEXADECIMAL)
    string(SUBSTRING "${_hexadecimal}" 2 -1 _digits)
    string(LENGTH "${_digits}" _width)
    while(_width LESS 8)
      string(PREPEND _digits "0")
      math(EXPR _width "${_width} + 1")
    endwhile()
    string(TOLOWER "${_digits}" _digits)
    string(SUBSTRING "${_digits}" 0 2 _b3)
    string(SUBSTRING "${_digits}" 2 2 _b2)
    string(SUBSTRING "${_digits}" 4 2 _b1)
    string(SUBSTRING "${_digits}" 6 2 _b0)
    set(${out} "${_b0}${_b1}${_b2}${_b3}" PARENT_SCOPE)
  endfunction()

  set(_capabilityOp 17)
  set(_entryPointOp 15)
  set(_executionModeOp 16)
  set(_preserve 4466)       # the SignedZeroInfNanPreserve capability
  set(_preserveMode 4461)   # …and the execution mode of the same name

  list(GET _words 1 _versionWord)
  _spirv_value(${_versionWord} _version)
  if(_version LESS 66560)   # 1.4
    message(FATAL_ERROR "${SPIRV} is older than SPIR-V 1.4, which a "
            "precise module's float model needs")
  endif()

  list(LENGTH _words _count)
  set(_at 5)
  set(_declared FALSE)
  set(_entry "")
  set(_after -1)
  while(_at LESS _count AND _after LESS 0)
    list(GET _words ${_at} _first)
    _spirv_value(${_first} _instruction)
    math(EXPR _opcode "${_instruction} & 65535")
    math(EXPR _length "${_instruction} >> 16")
    if(_length LESS 1)
      message(FATAL_ERROR "${SPIRV} holds an instruction of no words")
    endif()
    if(_opcode EQUAL _capabilityOp)
      math(EXPR _operand "${_at} + 1")
      list(GET _words ${_operand} _named)
      _spirv_value(${_named} _capability)
      if(_capability EQUAL _preserve)
        set(_declared TRUE)
      endif()
    elseif(_opcode EQUAL _entryPointOp)
      math(EXPR _operand "${_at} + 2")
      list(GET _words ${_operand} _entry)
      math(EXPR _after "${_at} + ${_length}")
    endif()
    math(EXPR _at "${_at} + ${_length}")
  endwhile()
  if(_after LESS 0)
    message(FATAL_ERROR "${SPIRV} names no entry point")
  endif()

  # The mode first, because inserting it leaves every earlier position
  # where it was.
  math(EXPR _modeHead "(4 << 16) | ${_executionModeOp}")
  _spirv_stored(${_modeHead} _modeHeadStored)
  _spirv_stored(${_preserveMode} _modeStored)
  _spirv_stored(32 _widthStored)
  list(INSERT _words ${_after} ${_modeHeadStored} ${_entry} ${_modeStored}
       ${_widthStored})
  if(NOT _declared)
    math(EXPR _capabilityHead "(2 << 16) | ${_capabilityOp}")
    _spirv_stored(${_capabilityHead} _capabilityHeadStored)
    _spirv_stored(${_preserve} _capabilityStored)
    list(INSERT _words 5 ${_capabilityHeadStored} ${_capabilityStored})
  endif()
endif()

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
# words does not recompile everything that includes it — and TOUCHED
# either way, because the header is a declared output of the rule that
# runs this script and a build system compares its timestamp against the
# shader's: left older than its input, the rule would re-run on every
# build for ever.
file(WRITE "${HEADER}.tmp" "${_out}")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${HEADER}.tmp" "${HEADER}")
file(REMOVE "${HEADER}.tmp")
file(TOUCH "${HEADER}")
