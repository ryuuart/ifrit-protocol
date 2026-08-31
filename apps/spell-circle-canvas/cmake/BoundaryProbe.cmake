# A tier boundary's NEGATIVE CONTROL, as a test.
#
# A boundary is structural when the code that would cross it cannot be
# built. Proving that takes a target that must FAIL to build, which no
# compiled test binary can hold: a translation unit that does not compile
# cannot also run assertions. So the probe is EXCLUDE_FROM_ALL — the
# ordinary build never sees it — and the test is a build of that one
# target which has to come back with a specific message. Demanding the
# message and not merely a non-zero status is what keeps an unrelated
# breakage, a renamed target or an empty source from reading as the
# boundary holding.
#
#   sigil_boundary_probe(
#     TARGET    <name>          the executable and the test both
#     SOURCES   <file>...       the translation unit that must not build
#     LIBRARIES <target>...     what it links, which is what it probes
#     EXPECT    <regex>...      any one of these in the build output
#                               makes the test pass)
function(sigil_boundary_probe)
  cmake_parse_arguments(PROBE "" "TARGET" "SOURCES;LIBRARIES;EXPECT" ${ARGN})
  add_executable(${PROBE_TARGET} EXCLUDE_FROM_ALL ${PROBE_SOURCES})
  target_link_libraries(${PROBE_TARGET} PRIVATE ${PROBE_LIBRARIES})
  add_test(NAME ${PROBE_TARGET}
           COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                   --config $<CONFIG> --target ${PROBE_TARGET})
  set_tests_properties(${PROBE_TARGET} PROPERTIES
    PASS_REGULAR_EXPRESSION "${PROBE_EXPECT}"
    # One build tool at a time in one build tree, however many probes
    # the run selects.
    RESOURCE_LOCK sigil_boundary_probe_build)
endfunction()
