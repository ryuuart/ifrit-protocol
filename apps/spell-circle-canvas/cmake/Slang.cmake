# Compiling Slang at BUILD TIME, into the build tree and never into the
# source tree — the same posture the FlatBuffers C++ header has.
#
# A shader module here is compiled by `slangc` when the library is built
# and again, with a material's body appended to it, when the library
# runs: a material exists only as a value in memory, so the source it is
# prepended to cannot be finished ahead of time. What the build compiles
# is therefore the module ALONE, and what it produces is two things:
#
#   the SPIR-V, in the build tree, which nothing reads — compiling it is
#   the point, because it makes a mistake in a shader a build failure
#   rather than a first-frame surprise. A module with no entry point has
#   no SPIR-V to emit and is compiled to a Slang module instead, which
#   asks the same question of it;
#   a header carrying the module's TEXT, which is what the run-time
#   compile prepends.
#
# The float model is set downstream of `slangc`, not by it: `-fp-mode` is
# a no-op for these targets, and the model is pinned where the code is
# finally generated — `-ffp-contract=off` on generated C++,
# `-fmetal-math-mode=safe -ffp-contract=off` on Metal, and
# MVK_CONFIG_FAST_MATH_ENABLED=0 for MoltenVK at run time. Slang through
# 2026.7.1 emits no NoContraction decoration in SPIR-V — `-fp-mode
# precise` included — so a driver is free to fuse a
# multiply-add inside a module compiled here; a kernel that needs the
# unfused answer has to reach the same result without depending on it.

find_package(slang CONFIG REQUIRED)

# Where the generated headers land. A consumer adds this to its include
# path and reaches a module as <sigilslang/<name>.h>.
set(SIGIL_SLANG_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/slang"
    CACHE INTERNAL "generated Slang module headers")

# sigil_slang_module(
#   NAME    <stem>              the header is <stem>.h in sigilslang/
#   SOURCE  <file.slang>
#   [ENTRIES <e1> <e2> ...]     entry points to compile, one call each
#   [PROFILE <p>]               default spirv_1_5
#   [INCLUDE <dir> ...]         module search paths for `import`
#   [DEPENDS <file> ...]        extra files the compile depends on
#   [OUT_VAR <var>]             receives the generated header's path
#   [CPP_VAR <var>]             also emit C++ for the host, path returned
#   [SPIRV_VAR <var>]           also emit a header carrying the entry's
#                               SPIR-V words, path returned
# )
#
# A module with no ENTRIES is compiled all the same, to a Slang module,
# so that one only ever prepended to run-time source is still checked
# here.
#
# CPP_VAR and SPIRV_VAR are the SINGLE-SOURCE KERNEL lane: one invocation
# emits both the C++ a host executor calls and the SPIR-V a device
# executor dispatches, so the two cannot be two pieces of arithmetic.
# Both want exactly one entry point — the C++ target names its output
# once, and a kernel whose device side is one dispatch has one entry to
# name. The C++ the emitter produces must be compiled with
# `-ffp-contract=off`, which is what makes every optimisation level, and
# therefore a Debug build and a Release one, produce the same bits;
# `sigil_slang_kernel_flags` sets it on a source file.
function(sigil_slang_module)
  cmake_parse_arguments(ARG "" "NAME;SOURCE;PROFILE;OUT_VAR;CPP_VAR;SPIRV_VAR"
                        "ENTRIES;INCLUDE;DEPENDS" ${ARGN})
  if(NOT ARG_NAME OR NOT ARG_SOURCE)
    message(FATAL_ERROR "sigil_slang_module needs NAME and SOURCE")
  endif()
  if(NOT ARG_PROFILE)
    set(ARG_PROFILE spirv_1_5)
  endif()
  get_filename_component(_source "${ARG_SOURCE}" ABSOLUTE)
  set(_header "${SIGIL_SLANG_GENERATED_DIR}/sigilslang/${ARG_NAME}.h")

  set(_includeFlags "")
  foreach(_dir IN LISTS ARG_INCLUDE)
    list(APPEND _includeFlags -I "${_dir}")
  endforeach()

  if((ARG_CPP_VAR OR ARG_SPIRV_VAR))
    list(LENGTH ARG_ENTRIES _entryCount)
    if(NOT _entryCount EQUAL 1)
      message(FATAL_ERROR
              "sigil_slang_module: ${ARG_NAME} asks for a host or an embedded "
              "build and names ${_entryCount} entry points; it wants one")
    endif()
  endif()

  # The host build, when one is asked for: the same invocation that emits
  # the SPIR-V emits it, because two invocations of one compiler over one
  # source is still one source and one more thing to keep in step.
  set(_cppFlags "")
  set(_cpp "")
  if(ARG_CPP_VAR)
    set(_cpp "${SIGIL_SLANG_GENERATED_DIR}/${ARG_NAME}.cpp")
    set(_cppFlags -target cpp -o "${_cpp}")
  endif()

  # One slangc invocation per entry point: the driver names its output
  # per -o, and one file per entry keeps the failure specific.
  set(_spvFiles "")
  set(_spvCommands "")
  foreach(_entry IN LISTS ARG_ENTRIES)
    set(_spv "${SIGIL_SLANG_GENERATED_DIR}/${ARG_NAME}.${_entry}.spv")
    list(APPEND _spvFiles "${_spv}")
    list(APPEND _spvCommands COMMAND slang::slangc "${_source}"
         -entry ${_entry} ${_includeFlags} ${_cppFlags}
         -target spirv -profile ${ARG_PROFILE} -o "${_spv}")
    # A -o binds to the target before it, so the host build is named once
    # and only alongside the first (and only) entry.
    set(_cppFlags "")
  endforeach()
  if(_cpp)
    list(APPEND _spvFiles "${_cpp}")
  endif()

  # …and the words themselves as a header, because a shader that had to
  # be found on disk at run time would be a second way for a build to be
  # incomplete.
  set(_spvHeader "")
  if(ARG_SPIRV_VAR)
    list(GET ARG_ENTRIES 0 _onlyEntry)
    set(_spvHeader "${SIGIL_SLANG_GENERATED_DIR}/sigilslang/${ARG_NAME}.spv.h")
    list(APPEND _spvCommands COMMAND ${CMAKE_COMMAND}
         -DNAME=${ARG_NAME}
         -DENTRY=${_onlyEntry}
         "-DSPIRV=${SIGIL_SLANG_GENERATED_DIR}/${ARG_NAME}.${_onlyEntry}.spv"
         "-DHEADER=${_spvHeader}"
         -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SlangEmbedSpirv.cmake")
    list(APPEND _spvFiles "${_spvHeader}")
  endif()
  if(NOT ARG_ENTRIES)
    # A module with nothing to run has no SPIR-V to emit — the compiler
    # says so — so it is checked by compiling it to a module instead,
    # which is the whole of what "does this module compile" means.
    set(_module "${SIGIL_SLANG_GENERATED_DIR}/${ARG_NAME}.slang-module")
    list(APPEND _spvFiles "${_module}")
    list(APPEND _spvCommands COMMAND slang::slangc "${_source}"
         ${_includeFlags} -o "${_module}")
  endif()

  add_custom_command(
    OUTPUT "${_header}" ${_spvFiles}
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${SIGIL_SLANG_GENERATED_DIR}/sigilslang"
    ${_spvCommands}
    COMMAND ${CMAKE_COMMAND}
            -DNAME=${ARG_NAME}
            -DSOURCE=${_source}
            -DHEADER=${_header}
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SlangEmbed.cmake"
    DEPENDS "${_source}" ${ARG_DEPENDS}
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SlangEmbed.cmake"
    COMMENT "slangc ${ARG_NAME}.slang"
    VERBATIM)
  if(ARG_OUT_VAR)
    set(${ARG_OUT_VAR} "${_header}" PARENT_SCOPE)
  endif()
  if(ARG_CPP_VAR)
    set(${ARG_CPP_VAR} "${_cpp}" PARENT_SCOPE)
  endif()
  if(ARG_SPIRV_VAR)
    set(${ARG_SPIRV_VAR} "${_spvHeader}" PARENT_SCOPE)
  endif()
endfunction()

# The float model the generated C++ is compiled under, set on one source
# file. Contraction OFF is the whole of it: clang fuses a multiply-add
# within a statement by default and a device's compiler makes its own
# decision, so the two answers part company on the first polynomial;
# pinned off, every optimisation level agrees and a Debug build and a
# Release one produce the same bits. The generated file is a compiler's
# output and is not held to the warnings a hand-written one is.
function(sigil_slang_kernel_flags file)
  set_source_files_properties("${file}" PROPERTIES
    COMPILE_OPTIONS "-ffp-contract=off;-w"
    SKIP_LINTING ON)
endfunction()
