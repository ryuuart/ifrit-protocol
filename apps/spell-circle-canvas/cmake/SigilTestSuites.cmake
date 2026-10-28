# One ctest entry per GoogleTest suite, run by ctest itself in script mode.
#
#   cmake -D SIGIL_SUITES_STEP=discover
#         -D TEST_EXECUTABLE=<binary> -D TEST_FILTER=<gtest filter>
#         -D TEST_LABELS=<labels> -D TEST_SERIAL=<TRUE|FALSE>
#         -D TEST_WORKING_DIRECTORY=<directory> -D TESTS_FILE=<output>
#         -D TEST_LIBRARY=<library> -P SigilTestSuites.cmake
#     Lists the binary's cases under the filter and writes TESTS_FILE: one
#     add_test() per suite, named <library>.<Suite>, whose command is this
#     script's launch step over that suite's cases. The library's name
#     keeps two libraries' suites of one name apart and selects a library
#     with `ctest -R '^<library>\.'`. A suite whose cases the filter names
#     one by one (Suite.Case) gets one entry per such case, named
#     <library>.<Suite>.<Case>, because a group of labels claims exactly
#     those.
#
#   cmake -D SIGIL_SUITES_STEP=launch
#         -D TEST_EXECUTABLE=<binary> -D TEST_FILTER=<gtest filter>
#         -P SigilTestSuites.cmake
#     Runs the binary over the filter and prints what it printed. A run
#     that fails exits 1. A run in which every case skipped and none failed
#     exits 77, the skip status the entry declares, so a suite reads as
#     skipped only when nothing in it ran; a suite with one skipped case
#     beside cases that passed reads as passed.

cmake_minimum_required(VERSION 3.31)

set(_sigil_skip_status 77)

if(SIGIL_SUITES_STEP STREQUAL "launch")
  execute_process(
    COMMAND "${TEST_EXECUTABLE}" "--gtest_filter=${TEST_FILTER}"
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output
    RESULT_VARIABLE result)
  message("${output}")
  if(NOT result EQUAL 0)
    message(FATAL_ERROR
      "${TEST_EXECUTABLE} --gtest_filter=${TEST_FILTER} exited with ${result}")
  endif()
  if(output MATCHES "\\[  PASSED  \\] 0 tests\\."
     AND output MATCHES "\\[  SKIPPED \\]")
    cmake_language(EXIT ${_sigil_skip_status})
  endif()
  return()
endif()

if(NOT SIGIL_SUITES_STEP STREQUAL "discover")
  message(FATAL_ERROR "SIGIL_SUITES_STEP is discover or launch")
endif()

execute_process(
  COMMAND "${TEST_EXECUTABLE}" --gtest_list_tests "--gtest_filter=${TEST_FILTER}"
  WORKING_DIRECTORY "${TEST_WORKING_DIRECTORY}"
  OUTPUT_VARIABLE listing
  ERROR_VARIABLE errors
  RESULT_VARIABLE result
  TIMEOUT 60)
if(NOT result EQUAL 0)
  message(FATAL_ERROR
    "${TEST_EXECUTABLE} could not list its cases (${result}):\n${errors}")
endif()

# The filter's own parts: the cases it names one by one, and what it leaves
# out, which every entry carries so a case another group claims stays out.
set(positive "${TEST_FILTER}")
set(negative "")
string(FIND "${TEST_FILTER}" "-" dash)
if(dash GREATER_EQUAL 0)
  string(SUBSTRING "${TEST_FILTER}" 0 ${dash} positive)
  math(EXPR after "${dash} + 1")
  string(SUBSTRING "${TEST_FILTER}" ${after} -1 negative)
endif()
string(REPLACE ":" ";" positives "${positive}")
set(named_cases)
foreach(pattern IN LISTS positives)
  if(pattern MATCHES "\\." AND NOT pattern MATCHES "[*?]")
    list(APPEND named_cases "${pattern}")
  endif()
endforeach()

# GoogleTest lists a suite flush left with a trailing dot and each of its
# cases indented beneath it; a parameter or a type follows a `#`.
string(REPLACE ";" "\\;" listing "${listing}")
string(REPLACE "\n" ";" lines "${listing}")
set(suites)
set(suite "")
foreach(line IN LISTS lines)
  string(REGEX REPLACE " *#.*$" "" line "${line}")
  if(line MATCHES "^([^ ].*)\\.$")
    set(suite "${CMAKE_MATCH_1}")
    list(APPEND suites "${suite}")
    set(cases_${suite})
  elseif(line MATCHES "^  (.+)$" AND suite)
    list(APPEND cases_${suite} "${CMAKE_MATCH_1}")
  endif()
endforeach()

set(content "")
function(_sigil_add_entry suite_or_case filter)
  set(name "${TEST_LIBRARY}.${suite_or_case}")
  set(disabled FALSE)
  if(name MATCHES "(^|[./])DISABLED_")
    set(disabled TRUE)
  endif()
  string(APPEND content
    "add_test([==[${name}]==] [==[${CMAKE_COMMAND}]==]"
    " -D SIGIL_SUITES_STEP=launch"
    " -D [==[TEST_EXECUTABLE=${TEST_EXECUTABLE}]==]"
    " -D [==[TEST_FILTER=${filter}]==]"
    " -P [==[${CMAKE_CURRENT_LIST_FILE}]==])\n"
    "set_tests_properties([==[${name}]==] PROPERTIES"
    " LABELS [==[${TEST_LABELS}]==]"
    " SKIP_RETURN_CODE ${_sigil_skip_status}"
    " WORKING_DIRECTORY [==[${TEST_WORKING_DIRECTORY}]==]"
    " DISABLED ${disabled}")
  if(TEST_SERIAL)
    string(APPEND content " RUN_SERIAL TRUE")
  endif()
  string(APPEND content ")\n")
  set(content "${content}" PARENT_SCOPE)
endfunction()

foreach(suite IN LISTS suites)
  set(whole FALSE)
  foreach(case IN LISTS cases_${suite})
    if("${suite}.${case}" IN_LIST named_cases)
      _sigil_add_entry("${suite}.${case}" "${suite}.${case}")
    else()
      set(whole TRUE)
    endif()
  endforeach()
  if(whole)
    set(filter "${suite}.*")
    if(negative)
      string(APPEND filter "-${negative}")
    endif()
    _sigil_add_entry("${suite}" "${filter}")
  endif()
endforeach()

file(WRITE "${TESTS_FILE}" "${content}")
