# SigilProtocol reference

One page per domain, each WRITTEN BY `definition/generate.py` from the
definition's own documentation and never edited here: the domain's
account of itself, where its agent, its C++ client, its events and its
Python client are, every command with the table it takes and the table
it answers, every event with the table it carries, and the enumerations
and tables the domain declares that no command shows. Edit
`definition/protocol.fbs` and regenerate; `protocol_drift` fails while a
page says something the definition no longer does.

| Page | What it holds |
| --- | --- |
| `domains/host.md` | the host itself: what it is, which domains it mounts, where it keeps its state, and describe, the first command a client sends |
| `domains/clock.md` | the clock a session's frames are drawn at: its four policies, stepping, pausing, the time scale and the budget |
| `domains/session.md` | the one running sketch: opening it, pinning its device and promotion, stills and sequences, timing, measured values and the composite-count plane |
| `domains/registry.md` | the sketches a host can open, and the rows a browser shows for them |

The tables every domain shares — `Error`, `Empty`, `Revision` and the
error codes — are the library README's, under "The errors".
