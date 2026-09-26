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
 definition/protocol.fbs           the definition: domains, commands, events,
          |                        tables, /// documentation, marks
          | flatc -b --schema --bfbs-comments --bfbs-builtins
          v
 protocol.bfbs  -----------------  the reflected definition: services,
          |                        documentation and attributes kept
          | sigil_schema_values     | sigil_protocol, per domain
          v                         v
 protocol_values.h       clock/ClockAgent.h    clock/ClockClient.h   Tables.h
 value types and         agent, events,        C++ client            every
 their JSON forms        wire()                     |                table
                              |                     |
                              |                     |     protocol_description.json
                              |                     |          | generate.py
                              v                     |          v
                           AGENTS                   |     Python client, reference
                (in the library that owns           |     pages (committed, checked)
                 the domain; a host mounts;         |          |
                 host is this library's own)        |          |
                              |                     |          |
                              v                     |          |
                         DISPATCHER  one per host: handlers by method, a
                              |      session per client however it came,
                              |      events by domain, the refusals no
                              |      agent sees
                   +----------+-----------+         |          |
                   | attach               | attach  |          |
                   v                      v         |          |
               InProcess              ENDPOINT      |          |
          one client, a caller,  ws:// on loopback, |          |
          no socket (tests,      text frames,       |          |
          command lists)         <state>/protocol-address,     |
                   ^             /protocol served,  |          |
                   |             inside sigil::io::advance(hub)          |
                   |                      ^         |          v
                   +------- CLIENTS ------+---------+       PANELS
                                                            (Seer)
```

Seven seams, in the order a message crosses them; the first six are
built, and every host mounts the endpoint — Sketchbook and Seer by
default, the product receiver when `--inspect` asks. Each promises
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
both. `sigil_protocol` refuses, naming the part, a definition with an
undocumented part; a union or a struct; a default that is no finite
number; no `Empty`; a domain service not named for its namespace's last
word raised — `clock`'s is `Clock`, the name its headers are written
under — or two namespaces ending in one word; an events service with no
domain beside it; an event not marked streaming, a command that is, or
an event that takes anything but `Empty`; a domain with events and no
`enable` and `disable`, one with them and no events, or an `enable` or
`disable` that takes or answers anything but `Empty`. It refuses a
definition whose domains are not the ones the build names, so a new
domain is named in this directory's `CMakeLists.txt` before it builds.
It writes, into the build tree as flatc's C++ is:

- `<domain>/<Domain>Agent.h` — `clock/ClockAgent.h`: the agent
  interface, one pure virtual per command; the events emitter, one
  member per event; and `wire()`.
- `<domain>/<Domain>Client.h`: the typed C++ client.
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
library that owns the domain: the registry's beside the sketch library's
core, the session's and the clock's beside its host tier, the clock's
policy SigilMotion's value over the frame clock. A host only mounts them,
and the sketch library's PROTOCOL.md chapter says what they answer.
The `host` domain's is this library's own,
`sigil::protocol::HostDomain`, which every dispatcher mounts on itself,
so `host.describe` answers on every host before any other agent is
written: the definition's revision and the domains mounted and the
clients attached, which the dispatcher knows, beside what only the
program knows — its name and version, its state root, its clock's
policy and its sessions open — which it supplies as a
`sigil::protocol::Program`. `sigil::protocol::clock::ClockAgent` is
one such interface: a command answered at once returns a
`sigil::protocol::Answer` — the result, or an `Error` made by
`sigil::protocol::refusal` — and one marked asynchronous is handed a
`sigil::protocol::Reply` and answers through it, once. The libraries
under a domain never know the protocol; their values are tested on
their own, and an agent is a thin reading of them.

**4. The dispatcher**, `sigil::protocol::Dispatcher`, holds one
`sigil::protocol::Handler` per method, filed by a generated `wire()` —
`sigil::protocol::clock::wire` mounts a clock agent on anything that
satisfies `sigil::protocol::Mounts`. Each handler takes the parameters'
JSON text and answers through a `sigil::protocol::Respond`:
`sigil::protocol::answerNow` and `sigil::protocol::answerLater` read the
parameters as their table and answer the result as its JSON form.
`enable` and `disable` are the dispatcher's own and no `wire()` mounts
them. Events go out through the `sigil::protocol::Emit` that
`sigil::protocol::Dispatcher::events` hands a generated
`sigil::protocol::clock::ClockEvents`, each member built on
`sigil::protocol::emitEvent`, which answers false where the event's
table cannot hold it or there is nowhere to send it, and is marked so
the sender cannot pass that by unseen.

Every client attached is a SESSION, one id each. A command is answered
in this order, and the first seam that refuses answers, naming the
method: a method the definition does not declare is `methodNotFound`;
one of a domain no agent is mounted for is `notMounted` — so a declared
command a host never wired answers at once and is told apart from one
nobody ever declared; parameters are then held against the command's
table, member by member, and a member the table does not declare, a
value of the wrong type, a whole number out of its type's range or a
name no value of an enumeration carries is `invalidParameters` naming
the parameter, and the agent is never asked; only then is the handler
run, which reads the parameters once more as its table. Inside it,
`sigil::protocol::Dispatcher::asking` is the session being answered, so
an agent that sets something for one client alone keys it by that, and
`sigil::protocol::Dispatcher::onDetach` tells it when that client has
gone, to clear it: a detaching client's overrides go with it. Events of
a domain reach a session only between its `enable` and its `disable`.
An answer is owed once: a second goes nowhere, one owed to a session
that has since detached goes nowhere, and a reply an agent lets go
without calling is answered `failed` as it goes — no command is left
unanswered. Everything runs on the one thread that drives the
dispatcher.

A host holds ONE dispatcher and mounts its agents on it once; every way
a client arrives attaches to that one through
`sigil::protocol::Dispatcher::attach`, with a
`sigil::protocol::Attachment` saying how its events reach it and what
it is told as it is let go, so a client in the same process and one on
the socket are sessions of the same dispatcher, each hearing the events
it enabled and each listed among those `host.describe` names attached.
`sigil::protocol::InProcess` is the dispatcher with no socket: one
client attached in the same process, whose `caller()` a generated client
speaks through and whose `send()` takes the very envelope text a socket
carries, so a test proves what a script would be told, and a host runs
a command list the same way. Letting a client go detaches it, as a
socket closing does; a client its dispatcher let go, one that detached
itself and one moved from are each refused `notSent`, in words saying
which. Letting the dispatcher go tells every client still attached that
enabled `host` that the host is closing, and runs no `onDetach`
listener: what an agent keyed by session goes with the host.

**5. The endpoint**, `sigil::protocol::Endpoint`, puts a host's
dispatcher behind one of SigilData's connections on SigilIO's `ws://`
listener at `/sigil`, on the port a `sigil::protocol::EndpointPolicy`
asks for or any free one, and every message it sends is a text frame.
It holds loopback alone, so another machine cannot reach it however it
asks; where the policy states peers beyond it, it holds every interface
and admits loopback and those peers, and every other is refused before
it becomes a client. A stated peer is a bare IP address: one that is
anything else opens nothing, and the endpoint's error names it. The
state root is the dispatcher's program's, and without one the endpoint
listens nowhere. Before the constructor returns — so before the first
frame — it writes the address a client dials,
`ws://127.0.0.1:PORT/sigil`, to `<state>/protocol-address`, as a browser
writes the port its debugging socket took, and serves the reflected
definition — `sigil::protocol::definition`, the very bytes the generator
read — at HTTP GET `/protocol` on the same port, written under
`<state>/protocol-pages/`. A peer's first request attaches it as a
session and every answer goes back to that peer alone; a peer whose
socket closes is detached on the next frame. Text that is no JSON at all
carries no id to answer and reaches no handler: the connection counts it
among what it could not read, and in process the same text is answered
`invalidRequest`. Every request is answered
inside the hub's dispatch, on the frame thread, so an agent never races
the paint, and with no client attached a frame runs no handler at all.
Letting the endpoint go detaches the clients it attached and no other,
telling each that enabled `host` that the endpoint is closing, through
`host.detached` sent before the socket closes, and takes back the
address file while it still names this endpoint. It is made on a hub and
a dispatcher and let go before either, and before the agents mounted on
that dispatcher, whose `onDetach` listeners its going runs. Sketchbook's
window and Seer put one on their dispatcher by default, a headless
Sketchbook serves one when `--headless --inspect` asks, and the product
receiver mounts one only when `--inspect` asks.

**6. The clients** speak through a `sigil::protocol::Caller`: `call`
sends a method and its parameters' JSON text and hands the answer back
once, `listen` hands every event of a method to a listener, and
`refused` is handed the refusal for every event whose text does not read
as its table. `sigil::protocol::clock::ClockClient` is the generated C++
client, one member per command answering through a reply and one per
event. The client refuses with codes of its own and words that open
with `client:` and the method, made by
`sigil::protocol::clientRefusal`: `sigil::protocol::callWith` refuses
with `notSent` parameters their table cannot hold, or a caller with
nowhere to send, before anything is sent, and with `unreadable` an
answer that does not read as the result; `sigil::protocol::listenFor`
refuses with `unreadable` an event that does not read as its table,
which no listener hears, and with `notSent` a caller that listens
nowhere, handing either to the caller's `refused` through
`sigil::protocol::reportRefusal` — or to the standard error where it
has none, so no event is lost unseen. An error the host answered is handed on as it
came. The Python client, `sigil.protocol`, is the same shape in the
package's own spelling: one class per domain, one snake_case method per
command — `Clock.set_policy` sends `clock.setPolicy` — taking the
parameter table's fields as keywords and answering the result as a
frozen dataclass, and one `on_…` method per event; its refusals are
`ProtocolError`s carrying the same codes, and a table's `from_json`
refuses a member the table does not declare, as the C++ reading does.
Both clients write the same JSON for every table, which a self-check
holds, so a test in the same process and a script over the socket
exercise one path. The Python package's transport is written by hand
beside the generator, `definition/client/connection.py`, and carried into
the package as it stands: `Envelopes`, a caller made of one exchange of
envelope text; `connect`, which attaches to a host at its `ws://` address
or through the state directory whose address file names it, reads the
definition it serves at `/protocol` and refuses one whose breaking
revision differs; and `launch`, which starts a headless Sketchbook under
a state directory and connects once its address file is written.

**7. The panels** are Seer's, over the same clients, and come after the
runtime; the first a client asks for is `host.describe`, which answers
the version, the domains mounted, the clock's policy, the state root,
the sessions open and the clients attached.

## The envelope

On the socket every message is one JSON object in one text frame, as
the Chrome DevTools protocol's are, so a browser page or a command-line
client reads it as a string. A request is
`{"id", "session", "method", "parameters"}`: the id a number or text the
answer carries back, the session optional and refused where it is not
the client's own, the parameters the command's table and absent for
the empty one. An answer is `{"id", "session", "result"}` or
`{"id", "session", "error"}`, the error `{"code", "message"}` with the
code by its name. An event is `{"session", "method", "parameters"}`. The
parameter table is chosen by the method's name, so no union of every
command caps how many there are, and a caller in the same process
passes the plain values with no envelope at all. Images never cross as
text: a still is written under the state root and its path answered.

## The errors

`sigil::protocol::ErrorCode` names the seam that refused, and every
`sigil::protocol::values::Error` carries it beside the seam's own words;
the client's words open with `client:`, so its refusal is never taken
for the host's even where only the message is shown.

| Code | Seam | When |
| --- | --- | --- |
| `invalidRequest` | the dispatcher | the message is no request: not a JSON object, without its id or method, or naming a session other than its client's own |
| `methodNotFound` | the dispatcher | the definition declares no such command |
| `notMounted` | the dispatcher | the definition declares it, and this host mounts no agent for its domain |
| `invalidParameters` | the dispatcher, then the handler | the parameters do not fit the command's table, naming the parameter; the agent is not asked |
| `failed` | the agent | the agent was asked and could not do it, or let its reply go without answering |
| `notSent` | the client | nothing was sent: the parameters' table cannot hold them, or the client has nowhere to send |
| `unreadable` | the client | what the host sent does not read as its table — an answer as the result, an event as its table: the two were built from different definitions |

## Virtual time is the one determinism seam

The `clock` domain is how a client makes the session it drives
repeatable. `Wall` is the clock a person watches; under `Advance` nothing
moves but by `clock.step`, which takes frames or seconds at a stated
rate — whole frames, and what is left as one shorter frame — and answers
once the last frame is drawn; `Pause` freezes the clock and the
recordings a hub plays; `PauseWhileLoading` is the wall clock held while
a page, a font or a resource is still arriving. A budget set with the
policy sends `budgetExpired` once it has run, which is how a client
asks whether a session has settled at its declared moment. A sketch
reads only whether its clock is the wall's, never which client drives
it.

What holds the clock under `PauseWhileLoading` is the host's open: a
session opened under any policy but the wall's has everything its setup
asked for — a page settled, a face or a fetched resource read whole —
before the open is answered, so no later frame finds anything on its
way. The clock domain drives the session a protocol host holds; a
host's other ways to a picture — a sweep, a written still, a window —
run on the host's own clock, stepped as their flags say, and no client
reaches them through this domain.

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
- `definition/Call.h` — `Caller`, `clientRefusal`, `reportRefusal`,
  `callWith`, `listenFor`: the caller's side.
- `definition/Definition.h` — `definition`: the reflected definition's
  bytes.
- `dispatch/Program.h` — `Program`: what only the program knows about
  itself.
- `dispatch/Dispatcher.h` — `Dispatcher`, `Attachment`: handlers,
  sessions, events and the refusals no agent is asked for, and how each
  client is reached.
- `dispatch/HostDomain.h` — `HostDomain`: the host domain this library
  answers itself.
- `dispatch/InProcess.h` — `InProcess`: one client attached with no
  socket.
- `endpoint/Endpoint.h` — `Endpoint`, `EndpointPolicy`: a dispatcher on
  a loopback socket.

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

- the generator refuses each rule of the definition's shape broken, one
  case a rule, over a definition of a few lines parsed in the test;
- the reflected definition keeps its eight services, every part's
  documentation and every mark, and is the same bytes flatc wrote;
- every table the definition declares — `sigil::protocol::Tables` —
  crosses JSON text, value and JSON text again byte for byte, from the
  value made with nothing set and from a sample of every field shape;
- every domain's `wire()` mounts exactly the commands its agent
  answers, a handler refuses parameters that do not fit before the agent
  is asked, an asynchronous command answers when its reply is called,
  an event goes out as its table's text, and a client reads a result
  back as its table and refuses, with its own codes, an answer or an
  event that is not one and a command with nowhere to go;
- `protocol_drift`: the committed Python client — its transport
  included, copied from `definition/client/` — and reference pages are
  what `generate.py` writes from the description the build wrote;
- `protocol_python_client`: every table of the Python client reads and
  writes back the very JSON the C++ tables write, and refuses a member it
  does not declare; its methods send the definition's names and refuse
  what the C++ client refuses; `protocol_python_types`, where
  basedpyright is installed, holds the package to its strict mode;
- the dispatcher answers `host.describe` on a dispatcher no agent was
  mounted on, and with what the program and the dispatcher each know;
  refuses a method the definition does not declare apart from one no
  agent is mounted for whatever its parameters, then parameters their
  table cannot hold naming the parameter — an enumeration's name or
  number it does not declare among them — a message that is no request,
  and a reply let go unanswered; sends events only between a client's
  enable and disable; takes what a detaching client set with it; and
  refuses, saying which, a client let go, one detached and one moved
  from;
- the endpoint, on a real socket, writes its address file before the
  first frame and takes it back, serves the definition at `/protocol`,
  answers a command over the wire only inside the hub's dispatch, sends
  an event to the client that enabled its domain, stands a socket client
  and one in process on one dispatcher, detaches a peer that leaves,
  tells a client that enabled `host` that it is closing, refuses a
  stated peer that is no address, and cannot be reached from beyond
  loopback unless a peer was stated — the last only where the machine
  has such an address;
- the documentation probe compiles every qualified name this README and
  the reference pages spell.

`reference/README.md` lists the pages.
