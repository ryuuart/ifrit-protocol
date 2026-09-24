# SigilSketch — the protocol's host side

The chapter on what this library answers when a client drives a sketch
host through SigilProtocol: the agents for the `registry`, `session` and
`clock` domains, the in-process host and the harness a test drives one
through, and what Sketchbook mounts. `README.md` beside this file is the
library; SigilProtocol's own README is the canon for the definition, the
dispatcher, the endpoint and the clients, and this chapter spells nothing
of theirs it does not need.

## The agents

Each domain's agent lives beside the part of this library that owns the
domain, in a leaf of its own, so the core a sketch is written against and
the host tier a window stands on link no protocol:

- `sigil::sketch::RegistryAgent` (target `SigilSketchRegistryAgent`,
  beside `core/`) answers `registry.list` with `registryRows()` and
  `registry.catalog` with `catalog()`, each row put into the
  definition's table and nothing added.
- `sigil::sketch::SessionAgent` (target `SigilSketchSessionAgent`, beside
  `live/`) holds the one running sketch a protocol host holds — a `Host`
  opened by registry name or by path — and the policy clock its frames
  are drawn at, SigilMotion's `sigil::motion::PolicyClock`.
- `sigil::sketch::ClockAgent`, in the same leaf, answers the clock
  domain over that session's clock and sends `clock.budgetExpired` when
  a budget runs out.
- `sigil::sketch::HostAgents` makes all three and mounts them on a
  host's one dispatcher, which is how every host here mounts them.

**A session is opened for its clock.** `Host::Options::clock` is the
`sigil::motion::ClockPolicy` a host opens its sessions for, and under any
policy but the wall's a session is a repeatable run: what the sketch
measured about its own execution is pinned and the runtime's own
re-baking is held off. `SketchContext::deterministic` is the one reading
a sketch gets of it — the clock is not the wall's — and it never learns
which client drives it. The agent opens a session for the policy that
stands, draws no frame before a client's first step unless the clock is
the wall's, and opens it again at its own zero when the policy changes
between the wall's and any other.

**A step** takes seconds as whole frames of one over the rate and what is
left as one shorter frame; zero seconds is one frame that moves nothing,
which runs the sketch where it stands. Only the Advance policy steps.

**A still** under a moving clock is taken as a plate is: the runtime's
own still on a raster surface of the canvas times the density, cleared to
the declared ground, with every raster the session bakes taken at that
density. A canvas re-renders its still one frame on, which
`Session::stillStep` says and the clock counts. Under a held clock — the
Pause policy, a person's pause, or PauseWhileLoading while something is
arriving — a still is the host's own, `Host::still`: the frame the clock
holds as it was last drawn, with nothing new drawn, so two stills under
it are one picture. Every still is written under the state root; a path
outside it is refused.

**What a client sets goes with it**: the policy and its budget, the hold,
the speed and the promotion pin are each kept against the client that set
them, and when it detaches the session is opened again for the wall's
clock and the runtime's own promotion.

`session.measured` is refused, naming why: a value a sketch passes
through `SketchContext::measured` carries no name to answer it by.
`session.pinDevice` refuses the GPU: the agent takes its stills on the
CPU alone.

## The in-process host and the harness

The testing tier is two targets under `testing/`. `SigilSketchTesting`
holds `sigil::sketch::testing::InProcessHost` — a dispatcher, the three
agents and one client attached with no socket, each verb one command
answered before it returns by turning the host's loop — and
`sigil::sketch::testing::compare`, which holds a still against a picture
on disk through SigilImage's pixel difference. It carries no test
framework, so Python binds it as it stands. `SigilSketchTestingHarness`
holds `sigil::sketch::testing::Harness`, the GoogleTest fixture:

```cpp
#include <sigilsketch/testing/Harness.h>

using sigil::sketch::testing::Harness;
namespace protocol = sigil::protocol;

TEST_F(Harness, ThePlateAtOneSecond) {
  ASSERT_TRUE(open("cascade"));
  ASSERT_TRUE(clock(protocol::clock::Policy_Advance));
  ASSERT_TRUE(step(1.0));
  const auto picture = still(2.0);
  ASSERT_TRUE(picture);
  EXPECT_TRUE(compare(picture.result(), "expected.png").identical());
}
```

Every case has a state directory of its own, named for the case, removed
after a pass and kept after a failure. The hooks run in this order:
`Harness::SetUpStateDirectory` with the directory standing and empty,
`Harness::SetUpHostOptions` with the options the host is about to be made
with, `Harness::SetUpOnHost` once it stands, the body, then
`Harness::TearDownOnHost`. **A failing case prints first** what the host
says about itself — `host.describe`'s answer, `clock.current`'s and the
last still's path, `Harness::readout` — ahead of the failure's own text,
so a red case is read from the layer below.

In Python the same host is `sigil.testing.InProcess(state)`, spoken to by
the generated domain classes exactly as `sigil.protocol.connect` is, and
`sigil.sketch.render_file` is a harness session over one file: opened
under Advance, stepped to its moment, held there and photographed, which
is the still `--frame` writes. `sigil.protocol.launch(state=…)` starts
the served Sketchbook below and connects to it.

## What Sketchbook mounts

`--inspect` and `--inspect=PORT` are the one flag this adds, beside
`--state`:

- **The window** mounts an endpoint on loopback whether or not it was
  asked — any free port, or the one named — answering `host` and
  `registry`, with `host.describe` listing the session the window shows.
  Its event loop dispatches it; with no client attached a dispatch runs no
  handler. The window's frames are its render thread's, so the session
  and clock domains are not mounted there.
- **`Sketchbook --headless --inspect`**, naming no plate directory, no
  sketch and no kind, is a host a client drives: no window and no plates,
  the three agents mounted, the address written to
  `<state>/protocol-address` before the first frame, and a loop at the
  window's rate until an interrupt or a termination signal.
- **A sweep** with `--inspect` answers `host` and `registry` between its
  sketches; nothing a plate holds depends on it.
- **Every other lane refuses `--inspect`**: it ends once its output is
  written, and a client would have nothing to drive.

The state root is `--state`, or the platform's own location for
Sketchbook. `--deterministic` and `--no-deterministic` name the clock
policy a written still's session is opened for — Advance or the wall's —
and a still is taken under Advance, a measurement under the wall's clock,
when neither is given.

## Headers

- `core/agent/RegistryAgent.h` — `RegistryAgent`: the registry domain.
- `live/agent/SessionAgent.h` — `SessionAgentOptions`, `SessionAgent`:
  the session domain and the policy clock its frames are drawn at.
- `live/agent/ClockAgent.h` — `ClockAgent`, `policyOf`: the clock
  domain.
- `live/agent/HostAgents.h` — `HostAgents`: the three, mounted together.
- `testing/InProcessHost.h` — `InProcessHostOptions`, `InProcessHost`: a
  host in the test's own process.
- `testing/Comparison.h` — `Comparison`, `compare`: a still held against
  a picture on disk.
- `testing/Harness.h` — `Harness`: the fixture.
