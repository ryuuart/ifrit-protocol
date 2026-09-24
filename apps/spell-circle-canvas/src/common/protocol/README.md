# SigilProtocol

One protocol every Sigil host speaks and every client of one is generated
from. A host — Sketchbook, Seer, the product receiver — is asked what it
is, told how its clock moves, made to open a sketch and photograph it,
and asked what its registry holds, all as commands of one definition;
whatever asks, a test in the same process, a script over a socket or a
panel in Seer, speaks the same commands in the same words.

The definition is a FlatBuffers schema, `definition/protocol.fbs`. Its
`rpc_service` blocks are the DOMAINS — `host`, `clock`, `session`,
`registry` — each in a namespace of its own; a domain's calls are its
COMMANDS, each taking one ordinary table and answering one; and a second
service beside it, named for the domain with `Events` after it, lists
its EVENTS, each marked `(streaming: "server")` and answering the table
it carries. Everything else here derives from that file, and nothing
derived is kept by hand.

## The chain

```
 definition/protocol.fbs            the definition: domains, commands, events,
          |                         tables, /// documentation, marks
          | flatc -b --schema --bfbs-comments --bfbs-builtins
          v
 protocol.bfbs  ------------------  the reflected definition: services,
          |                         documentation and attributes kept
          | sigil_schema_values           | sigil_protocol
          v                               v
 protocol_values.h            <Domain>Agent.h   <Domain>Client.h   Tables.h
 value types, JSON forms      agent, events,    C++ client         every table
                              wire()                   |
                                  |                    |     protocol_description.json
                                  v                    |               | generate.py
                               AGENTS                  |               v
                     (in the library that owns         |     Python client, reference
                      the domain; a host mounts)       |     pages (committed, checked)
                                  |                    |               |
                                  v                    v               v
                             DISPATCHER <----------- CLIENTS -----> PANELS
                                  |                                (Seer)
                                  v
                              ENDPOINT   ws:// on loopback, <state>/protocol-address,
                                         the definition served at /protocol
```

Seven seams, in the order a message crosses them. Each promises
something and refuses something, and a failure names the seam that
refused.

**1. The definition** promises that every domain, command, event, table,
field and enumeration value carries its documentation where it is
declared, that a command takes one table and answers one, and that an
event is the table its events service answers. It refuses unions and
structs: a message is a table, and a command with nothing to say takes
or answers `Empty`. Two attributes carry what the parser cannot know:
`(experimental)` on a part whose shape may still change, and
`(asynchronous)` on a command its agent answers later — after frames
have been drawn — through a reply rather than a return. The schema's
root is `Error`, the one table every domain answers, because a root is
what makes flatc embed the reflected definition beside every table;
`Revision`'s declared defaults ARE the definition's revision.

**2. The generator** reads the reflected definition, never the text, so
what it sees is what flatc understood. flatc keeps `rpc_service` blocks
in the reflection as services, keeps the `///` comments with
`--bfbs-comments`, keeps a user attribute always and a builtin one —
the `streaming` mark — only with `--bfbs-builtins`; the build passes
both. `sigil_protocol` refuses a definition with an undocumented part,
an events service with no domain beside it, an event not marked
streaming, a command that is, or a domain with events and no `enable`
and `disable`; and it refuses a definition whose domains are not the
ones the build names, so a new domain is named in this directory's
`CMakeLists.txt` before it builds. It writes, into the build tree as
flatc's C++ is:

- `<domain>/<Service>Agent.h`: the agent interface, one pure virtual per
  command; the events emitter, one member per event; and `wire()`.
- `<domain>/<Service>Client.h`: the typed C++ client.
- `Tables.h`: every table as one type list, `sigil::protocol::Tables`,
  and their names as the definition spells them,
  `sigil::protocol::tableNames`.
- `protocol_description.json`: the whole definition as JSON, which
  `definition/generate.py` reads to write the Python client under
  `apps/python/sigil/sigil/protocol/` and one reference page per domain
  under `reference/domains/`. Those two trees are committed, because a
  wheel ships the one and the documentation site reads the other.

The value types beside them are `sigil_schema_values`': every table of
the definition is a plain struct in the value namespace beside its own —
`sigil::protocol::clock::values::StepParameters` — starting at the
defaults the definition declares, comparing with `==`, and crossing its
JSON form through `sigil::data::values::fromJson` and
`sigil::data::values::toJson`, each table read through the embedded
schema by `sigil::data::Schema::rootedAt`.

**3. The agents** answer a domain's commands, and each lives in the
library that owns the domain: the registry's in the sketch library's
core, the session's in its host tier, the clock's policy over the frame
clock. A host only mounts them. `sigil::protocol::clock::ClockAgent` is
one such interface: a command answered at once returns a
`sigil::protocol::Answer` — the result, or an `Error` made by
`sigil::protocol::refusal` — and one marked asynchronous is handed a
`sigil::protocol::Reply` and answers through it, once. The libraries
under a domain never know the protocol; their values are tested on
their own, and an agent is a thin reading of them.

**4. The dispatcher** holds one `sigil::protocol::Handler` per method,
filed by a generated `wire()` — `sigil::protocol::clock::wire` mounts a
clock agent on anything that satisfies `sigil::protocol::Mounts`. Each
handler takes the parameters' JSON text and answers through a
`sigil::protocol::Respond`: `sigil::protocol::answerNow` and
`sigil::protocol::answerLater` read the parameters as their table,
refusing with `invalidParameters` before the agent is asked, and answer
the result as its JSON form. `enable` and `disable` are the
dispatcher's own and no `wire()` mounts them: no event of a domain
reaches a client before its `enable` or after its `disable`. Events go
out through a `sigil::protocol::Emit`, one generated member per event —
`sigil::protocol::clock::ClockEvents` — each built on
`sigil::protocol::emitEvent`. The dispatcher itself, with its sessions
and its in-process form, is the runtime's next link: what it will
promise is stated here, and no header of this library declares it yet.
It answers a method the definition does not declare with
`methodNotFound`, one of a domain it has not mounted with `notMounted`,
and a message that is no request with `invalidRequest`, each naming the
method; it never leaves a command unanswered.

**5. The endpoint**, also the runtime's next link, is one of SigilData's
connections on SigilIO's `ws://`, bound to loopback on the port asked
for or on any free one. It writes the address it bound to
`<state>/protocol-address` before the first frame, serves the reflected
definition — `sigil::protocol::definition`, the very bytes the
generator read — at `/protocol`, and runs every handler on the frame
thread, inside the hub's dispatch, so an agent never races the paint. It
refuses a peer that is not on loopback unless one was stated. Mounted
with no client, a frame performs no dispatch. Sketchbook and Seer mount
one by default; the product receiver only when `--inspect` asks.

**6. The clients** speak through a `sigil::protocol::Caller`: `call`
sends a method and its parameters' JSON text and hands the answer back
once, and `listen` hands every event of a method to a listener.
`sigil::protocol::clock::ClockClient` is the generated C++ client, one
member per command answering through a reply and one per event;
`sigil::protocol::callWith` refuses parameters their table cannot hold
before anything is sent, and fails an answer that does not read as the
result, naming the method. The Python client, `sigil.protocol`, is the
same shape: one class per domain, one method per command taking the
parameter table's fields as keywords and answering the result as a
frozen dataclass, and one `on…` method per event. Both clients send the
same text, so a test in the same process and a script over the socket
exercise one path.

**7. The panels** are Seer's, over the same clients, and come after the
runtime; the first a client asks for is `host.describe`, which answers
the version, the domains mounted, the clock's policy, the state root and
the sessions open.

## The errors

`sigil::protocol::ErrorCode` names the seam that refused, and every
`sigil::protocol::values::Error` carries it beside the seam's own words.

| Code | Seam | When |
| --- | --- | --- |
| `invalidRequest` | the dispatcher | the message is no request: not JSON, or without its id or method |
| `methodNotFound` | the dispatcher | the definition declares no such command |
| `notMounted` | the dispatcher | the definition declares it, and this host mounts no agent for its domain |
| `invalidParameters` | the handler | the parameters do not fit the command's table; the agent is not asked |
| `failed` | the agent | the agent was asked and could not do it |

## Virtual time is the one determinism seam

The `clock` domain replaces every other way a run was made repeatable.
`Wall` is the clock a person watches; under `Advance` nothing moves but
by `clock.step`, which takes frames or seconds at a stated rate and
answers once the last frame is drawn; `Pause` freezes the clock and the
recordings a hub plays; `PauseWhileLoading` is the wall clock held while
a page, a font or a resource is still arriving. A budget set with the
policy sends `budgetExpired` once it has run, which is how a client
asks whether a session has settled at its declared moment. A sketch
reads only whether its clock is the wall's, never which client drives
it.

## The command line that remains

Two flags this protocol adds to a host, and only these: `--inspect`,
with an optional port, which mounts the endpoint where a host does not
by default; and `--state` with a directory, the state root the address
file, the stills and every cache are kept under. Every other flag a
host keeps becomes a short script of commands run in process: a still at
two seconds at double density is `clock.setPolicy` to `Advance`,
`clock.step` to two seconds, and `session.still` at density two.

## Headers

- `definition/Answer.h` — `Answer`, `Reply`, `refusal`: what a command
  answers.
- `definition/Mount.h` — `Respond`, `Handler`, `Mounts`, `Emit`,
  `readParameters`, `answerText`, `answerNow`, `answerLater`,
  `emitEvent`: the host's side of a command.
- `definition/Call.h` — `Caller`, `callWith`, `listenFor`: the caller's
  side.
- `definition/Definition.h` — `definition`: the reflected definition's
  bytes.

## Changing the definition

Edit `definition/protocol.fbs`, build `SigilProtocolDefinition`, then run

```sh
python3 src/common/protocol/definition/generate.py \
    --description build/generated/protocol/protocol_description.json
```

and commit the Python client and the reference pages it rewrites
together with the definition. A new domain is a namespace with a
service and, where it has events, an events service with `enable` and
`disable`; add its name to `SIGIL_PROTOCOL_DOMAINS` in this directory's
`CMakeLists.txt`, since the generator refuses a domain the build did
not name.

## The self-checks

`ctest -L protocol` runs every seam's own check, so a red case higher
up is triaged by running the layer below it:

- the reflected definition keeps its eight services, every part's
  documentation and every mark, and is the same bytes flatc wrote;
- every table the definition declares — `sigil::protocol::Tables` —
  crosses JSON text, value and JSON text again byte for byte, from the
  value made with nothing set and from a sample of every field shape;
- every domain's `wire()` mounts exactly the commands its agent
  answers, a handler refuses parameters that do not fit before the agent
  is asked, an asynchronous command answers when its reply is called,
  an event goes out as its table's text, and a client reads a result
  back as its table and fails one that is not;
- `protocol_drift`: the committed Python client and reference pages
  are what `generate.py` writes from the description the build wrote;
- the documentation probe compiles every qualified name this README and
  the reference pages spell.

`reference/README.md` lists the pages.
