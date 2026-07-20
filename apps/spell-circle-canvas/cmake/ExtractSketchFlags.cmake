# Lifts the compiler flags for the sketch anchor TU out of
# compile_commands.json into a clang response file — the single source
# of truth for how sketches build is the target graph itself, not a
# hand-maintained flag list.
#
# Inputs: -DCOMPDB=<compile_commands.json> -DANCHOR=<SketchAnchor.cpp>
#         -DCONFIG=<config> -DOUT=<sketch_flags.rsp>
#         -DEXTRA=<extra link inputs, ;-separated>  (e.g. skia archive)

file(READ "${COMPDB}" _json)
string(JSON _count LENGTH "${_json}")

set(_command "")
math(EXPR _last "${_count} - 1")
foreach(_i RANGE ${_last})
  string(JSON _file GET "${_json}" ${_i} file)
  if(NOT _file STREQUAL "${ANCHOR}")
    continue()
  endif()
  # Multi-config generators list the anchor once per config; match the
  # object path ("<target>.dir/<config>/...") against ours.
  string(JSON _output ERROR_VARIABLE _no_output GET "${_json}" ${_i} output)
  if(_no_output OR _output MATCHES "/${CONFIG}/" OR _count EQUAL 1)
    string(JSON _command GET "${_json}" ${_i} command)
    break()
  endif()
endforeach()

if(_command STREQUAL "")
  message(FATAL_ERROR
    "no compile_commands.json entry for ${ANCHOR} (config ${CONFIG})")
endif()

separate_arguments(_args UNIX_COMMAND "${_command}")

# Drop the compiler itself plus the per-object bookkeeping: -c/-o and
# the dependency-file flags ninja appends. Everything else (includes,
# defines, std, arch, sysroot, warnings, optimization) passes through.
list(POP_FRONT _args)
set(_flags "")
set(_skip_next FALSE)
foreach(_arg IN LISTS _args)
  if(_skip_next)
    set(_skip_next FALSE)
    continue()
  endif()
  if(_arg MATCHES "^(-c|-o|-MF|-MT|-MQ)$")
    set(_skip_next TRUE)
    continue()
  endif()
  if(_arg MATCHES "^(-MD|-MMD)$" OR _arg STREQUAL "${ANCHOR}")
    continue()
  endif()
  string(APPEND _flags "${_arg}\n")
endforeach()

foreach(_extra IN LISTS EXTRA)
  string(APPEND _flags "${_extra}\n")
endforeach()

file(WRITE "${OUT}" "${_flags}")
