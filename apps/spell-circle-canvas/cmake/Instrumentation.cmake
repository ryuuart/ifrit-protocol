# The two instrumented builds: LLVM source-based coverage and Clang's
# runtime sanitizers. Each is meant for a dedicated build tree — the
# `coverage`, `asan` and `tsan` presets configure one — because flipping
# either switch recompiles every object file. vcpkg dependencies arrive as
# prebuilt archives and are never instrumented: coverage reports cover this
# repository's sources, and a sanitizer check that needs both sides
# instrumented misfires across that boundary.
#
# Compile flags are gated on language because SpellCircleMac's Swift
# sources go through swiftc, which rejects these clang flags. The link
# flags are gated the same way through LINK_LANGUAGE: a C++/ObjC++ link
# runs through the clang++ driver, which embeds the runtime, while the
# Swift executable's swift-driver link must not see the flag. The Swift
# app still links the instrumented SpellCircleMacBridge dylib, which
# carries its own runtime reference.

option(SPELLCIRCLE_COVERAGE
  "Instrument C/C++/ObjC++ targets for llvm-cov source-based coverage" OFF)
if(SPELLCIRCLE_COVERAGE)
  # The flags are Clang's; AppleClang and clang-cl both carry Clang in
  # their compiler id and accept them.
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
      "SPELLCIRCLE_COVERAGE requires a Clang toolchain: "
      "-fprofile-instr-generate/-fcoverage-mapping are Clang's source-based "
      "coverage and ${CMAKE_CXX_COMPILER_ID} does not implement them.")
  endif()
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-fprofile-instr-generate;-fcoverage-mapping>")
  add_link_options(
    "$<$<LINK_LANGUAGE:C,CXX,OBJC,OBJCXX>:-fprofile-instr-generate>")
endif()

# A CMake list of -fsanitize= names: "address;undefined" for the
# ASan+UBSan lane, "thread" for TSan.
set(SPELLCIRCLE_SANITIZE "" CACHE STRING
  "Sanitizers to compile and link with (e.g. address;undefined or thread); empty builds none")
if(SPELLCIRCLE_SANITIZE)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR
      "SPELLCIRCLE_SANITIZE requires a Clang toolchain: the -fsanitize= "
      "runtimes configured here ship with Clang and "
      "${CMAKE_CXX_COMPILER_ID} does not provide them.")
  endif()
  if("thread" IN_LIST SPELLCIRCLE_SANITIZE AND "address" IN_LIST SPELLCIRCLE_SANITIZE)
    message(FATAL_ERROR
      "SPELLCIRCLE_SANITIZE cannot combine thread with address: each "
      "runtime interposes the allocator and shadows the address space its "
      "own way, and a process can only host one. Build separate trees.")
  endif()
  list(JOIN SPELLCIRCLE_SANITIZE "," sanitize_names)
  set(sanitize_compile_flags
    "-fsanitize=${sanitize_names}"
    # Reports unwind through frame pointers; without them the stacks in a
    # report degrade to unusable fragments.
    "-fno-omit-frame-pointer")
  if("undefined" IN_LIST SPELLCIRCLE_SANITIZE)
    # UBSan's default is print-and-continue, which lets a finding scroll
    # past inside a passing test. Non-recoverable checks abort instead, so
    # a UBSan finding fails the test that triggered it.
    list(APPEND sanitize_compile_flags "-fno-sanitize-recover=undefined")
  endif()
  if("address" IN_LIST SPELLCIRCLE_SANITIZE)
    # workaround: instrumented Skia headers meeting an uninstrumented
    # archive.
    #
    # Skia's TArray grows a poisoning flag in any translation unit compiled
    # under ASan, which moves every member that follows an array inside a
    # header type — SkPathBuilder's fill type among them. Half of such an
    # object's accessors are inline (this layout) and half live in the
    # prebuilt archive (the other layout), so values written through one
    # are read back from the wrong offset and quietly revert. Forcing this
    # header ahead of every Skia include leaves Skia's feature macro
    # undefined here, pinning our objects to the archive's layout; the
    # header itself states what that costs.
    add_compile_options(
      "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-include;${CMAKE_CURRENT_LIST_DIR}/SkiaSanitizerAbi.h>")
  endif()
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:${sanitize_compile_flags}>")
  add_link_options(
    "$<$<LINK_LANGUAGE:C,CXX,OBJC,OBJCXX>:-fsanitize=${sanitize_names}>")
endif()
