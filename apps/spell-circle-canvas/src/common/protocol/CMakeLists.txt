# SigilProtocol — the one protocol every Sigil host speaks and every
# client of one is generated from: a FlatBuffers definition whose
# rpc_service blocks are its domains, the generator that writes each
# domain's agent seam and clients out of it, and the runtime a host
# mounts its agents on.
#
# The definition is the source of truth and everything else derives from
# it: the C++ it generates is written into the build tree, as flatc's is,
# and the Python client and the reference pages are committed and held
# to it by a drift case. The libraries a domain describes never know the
# protocol; an agent over one lives in the library that owns the domain,
# and a host only mounts it.
#
# Every public header lives under one include root, include/sigilprotocol/
# <feature>/, and every generated one under the build tree's
# generated/protocol/sigilprotocol/, so both are spelled
# <sigilprotocol/…>.

# The reference pages: one per domain, written by the Python generator
# beside the definition, named once here and read by both the
# documentation site and the probe below.
set(SIGIL_PROTOCOL_DOMAINS host clock session registry)
set(SIGIL_PROTOCOL_REFERENCE ${CMAKE_CURRENT_SOURCE_DIR}/reference/README.md)
foreach(domain IN LISTS SIGIL_PROTOCOL_DOMAINS)
  list(APPEND SIGIL_PROTOCOL_REFERENCE
       ${CMAKE_CURRENT_SOURCE_DIR}/reference/domains/${domain}.md)
endforeach()

# Where the build writes what derives from the definition: the headers
# under an include root of their own, and the description the Python
# generator reads. The description's path is the whole tree's, since the
# one `generate` target at the root runs that generator too.
set(SIGIL_PROTOCOL_GENERATED ${CMAKE_BINARY_DIR}/generated/protocol)
set(SIGIL_PROTOCOL_DESCRIPTION ${SIGIL_PROTOCOL_GENERATED}/protocol_description.json
    CACHE INTERNAL "the protocol definition as JSON, which generate.py reads")

sigil_library_root(SigilProtocol
  BRIEF "The inspection and automation protocol: one definition, the agents a host mounts, and the clients generated from it"
  DOCS ${CMAKE_CURRENT_SOURCE_DIR}/docs/Groups.dox ${SIGIL_PROTOCOL_REFERENCE})

# Dependencies point down this list.
add_subdirectory(generator)
add_subdirectory(definition)
add_subdirectory(dispatch)
add_subdirectory(endpoint)

sigil_library(SigilProtocol
  INTERFACE SigilProtocolDefinition SigilProtocolDispatch
            SigilProtocolEndpoint)

# Every API name the prose spells, compiled against the headers that own
# it — the generated ones included, which the scan reads once they are
# written.
sigil_doc_probes(protocol
  READMES ${CMAKE_CURRENT_SOURCE_DIR}/README.md ${SIGIL_PROTOCOL_REFERENCE}
  INCLUDES ${SIGIL_PROTOCOL_GENERATED}/sigilprotocol
           ${PROJECT_SOURCE_DIR}/src/common/data/include/sigildata
  DEPENDS ${SIGIL_PROTOCOL_HEADERS}
  LIBRARIES SigilProtocol)
