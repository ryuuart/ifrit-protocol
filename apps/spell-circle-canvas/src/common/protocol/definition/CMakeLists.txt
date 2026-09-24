# THE DEFINITION AND WHAT DERIVES FROM IT IN THE BUILD. flatc reads
# protocol.fbs into its generated header, the reflected schema with its
# documentation and its attributes kept (--bfbs-comments, and
# --bfbs-builtins for the `streaming` mark every event carries), and that
# schema embedded beside every table; sigil_schema_values writes the
# value types; sigil_protocol reads the reflected schema and writes each
# domain's agent seam and client, and the description the Python
# generator reads. All of it lands under one include root in the build
# tree and is never a file in the checkout.
find_package(flatbuffers CONFIG REQUIRED)

set(PROTOCOL_SCHEMA ${CMAKE_CURRENT_LIST_DIR}/protocol.fbs)
set(PROTOCOL_OUT ${SIGIL_PROTOCOL_GENERATED}/sigilprotocol)

add_custom_command(
  OUTPUT ${PROTOCOL_OUT}/protocol_generated.h
         ${PROTOCOL_OUT}/protocol_bfbs_generated.h
         ${PROTOCOL_OUT}/protocol.bfbs
         ${PROTOCOL_OUT}/protocol_values.h
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PROTOCOL_OUT}
  COMMAND flatbuffers::flatc --cpp -b --schema --bfbs-gen-embed
          --bfbs-comments --bfbs-builtins -o ${PROTOCOL_OUT} ${PROTOCOL_SCHEMA}
  COMMAND sigil_schema_values ${PROTOCOL_SCHEMA} -o ${PROTOCOL_OUT}
  DEPENDS ${PROTOCOL_SCHEMA} sigil_schema_values
  COMMENT "Generating the protocol definition's C++ and value types"
  VERBATIM)

set(protocol_domain_headers)
foreach(domain IN LISTS SIGIL_PROTOCOL_DOMAINS)
  string(SUBSTRING ${domain} 0 1 head)
  string(TOUPPER ${head} head)
  string(SUBSTRING ${domain} 1 -1 rest)
  list(APPEND protocol_domain_headers
       ${PROTOCOL_OUT}/${domain}/${head}${rest}Agent.h
       ${PROTOCOL_OUT}/${domain}/${head}${rest}Client.h)
endforeach()
list(JOIN SIGIL_PROTOCOL_DOMAINS "," protocol_domains)
add_custom_command(
  OUTPUT ${protocol_domain_headers} ${PROTOCOL_OUT}/Tables.h
         ${SIGIL_PROTOCOL_DESCRIPTION}
  COMMAND sigil_protocol ${PROTOCOL_OUT}/protocol.bfbs -o ${PROTOCOL_OUT}
          --domains ${protocol_domains}
          --description ${SIGIL_PROTOCOL_DESCRIPTION}
  DEPENDS ${PROTOCOL_OUT}/protocol.bfbs sigil_protocol
  COMMENT "Generating the protocol's agents, clients and description"
  VERBATIM)

set(SIGIL_PROTOCOL_HEADERS
  ${PROTOCOL_OUT}/protocol_generated.h
  ${PROTOCOL_OUT}/protocol_bfbs_generated.h
  ${PROTOCOL_OUT}/protocol_values.h
  ${PROTOCOL_OUT}/Tables.h
  ${protocol_domain_headers})
set(SIGIL_PROTOCOL_HEADERS ${SIGIL_PROTOCOL_HEADERS} PARENT_SCOPE)

# What the generated code stands on — an answer, the host's side of a
# command, the caller's side — and the definition's own bytes. The
# generated headers are sources of this target, so every consumer waits
# for them.
sigil_library(SigilProtocolDefinition
  SOURCES Definition.cpp ${SIGIL_PROTOCOL_HEADERS}
          ${SIGIL_PROTOCOL_DESCRIPTION}
  HEADERS Answer.h Call.h Definition.h Mount.h
  PUBLIC SigilDataValues SigilDataDecode flatbuffers::flatbuffers)
target_include_directories(SigilProtocolDefinition
  PUBLIC ${SIGIL_PROTOCOL_GENERATED})

# THE SEAM SELF-CHECKS, all under one label so a red case higher up is
# triaged by running the layer below: the reflected definition keeps its
# services, its documentation and its attributes; every parameter and
# result table crosses JSON text, value and JSON text again byte for
# byte; and a generated wire() and client carry a command between them.
sigil_test(protocol_test
  SOURCES test/DefinitionTest.cpp test/RoundTripTest.cpp test/WiringTest.cpp
  LIBRARIES SigilProtocolDefinition
  DEFINITIONS SIGIL_PROTOCOL_BFBS="${PROTOCOL_OUT}/protocol.bfbs"
  LABELS protocol)

# THE DRIFT CHECK: the committed Python client and reference pages are
# what the Python generator beside the definition writes from the
# description the build just wrote.
add_test(NAME protocol_drift
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/generate.py
          --description ${SIGIL_PROTOCOL_DESCRIPTION} --check)
set_tests_properties(protocol_drift PROPERTIES LABELS protocol)
