# SpellCircle

`ifrit-protocol-apps` supplies the typed `SpellCircle` import package. It models
vector scenes, encodes and decodes their FlatBuffers wire format, and sends scene
bytes over UDP. Python 3.11 and FlatBuffers are its only runtime requirements.
Scene values work in offline tools, network clients, TouchDesigner and native
hosts without importing Qt or the Sigil extension.

## Install and run

From this directory, install into an existing environment:

```sh
uv pip install --python /path/to/environment/bin/python .
```

From the parent workspace, uv can select this project and manage its environment:

```sh
uv run --package ifrit-protocol-apps python -m SpellCircle.examples.send_spell_circles --seed 1 --output scene.bin
uv run --package ifrit-protocol-apps spellcircle inspect scene.bin
uv run --package ifrit-protocol-apps spellcircle send scene.bin --host 127.0.0.1 --port 27015
```

Writing and inspecting files need no receiver. `send` validates a saved scene
before transmitting its original bytes as one datagram. `python -m SpellCircle`
provides the same commands as `spellcircle`.

The workspace shares its members' Python requirement of 3.12 or newer. The
individual project remains installable on Python 3.11 outside that environment;
TouchDesigner depends directly on this project.

## Author a scene

The canvas records typed values. Circles, points, edges, boxes and whole scene
snapshots are immutable dataclasses; they do not retain a canvas, socket or
FlatBuffers buffer.

```python
from pathlib import Path
from SpellCircle import SpellCircleCanvas, decode_scene, encode_scene

canvas = SpellCircleCanvas(width=640, height=360)
source = canvas.circle("Source", 180, 180, 96, active=1)
target = canvas.circle("Target", 460, 180, 72)
first = canvas.point(source, 0.25, "out")
second = canvas.point(target, 0.75, "in")
canvas.edge(first, second)
canvas.box("Signal", first, active=0.5)

scene = canvas.snapshot()
Path("scene.bin").write_bytes(encode_scene(scene))
received = decode_scene(Path("scene.bin").read_bytes())
assert received.edges[0].first.circle is received.circles[0]
```

`canvas.to_bytes()` encodes its current snapshot. `canvas.clear()` removes the
recorded geometry and retains the dimensions; existing snapshots remain valid.
The `circles`, `edges` and `boxes` properties return tuples of the corresponding
value types. `edge()` and `box()` return the values they record.

Use `canvas.anchor(x, y)` for a point at an arbitrary location. It creates a
radius-zero circle without adding it to the visible circle list. References to
that anchor are encoded with the scene.

## Work with values directly

The model and codec do not require a canvas. Construct the same values directly
for data pipelines, generated layouts, receivers and transformations:

```python
from dataclasses import replace
from SpellCircle import (
    BoxDefinition,
    CircleDefinition,
    EdgeDefinition,
    PointReference,
    SceneDefinition,
    encode_scene,
)

circle = CircleDefinition("Node", 320, 180, 100)
start = PointReference(circle, 0.0, "top")
end = PointReference(circle, 0.5, "bottom")
scene = SceneDefinition(
    circles=(circle,),
    edges=(EdgeDefinition(start, end),),
    boxes=(BoxDefinition("Ready", start, active=1),),
    width=640,
    height=360,
)
resized = replace(scene, width=1280, height=720)
payload = encode_scene(resized)
```

| Value | Fields |
| --- | --- |
| `CircleDefinition` | `name`, `center_x`, `center_y`, `radius`, `text_start=0`, `active=0` |
| `PointReference` | `circle`, `position=0`, `value=""` |
| `EdgeDefinition` | `first`, `second` |
| `BoxDefinition` | `value`, `point`, `active=0` |
| `SceneDefinition` | `circles=()`, `edges=()`, `boxes=()`, `width=0`, `height=0` |

Circle radius is an unsigned 32-bit integer. Coordinates, positions and dimensions
are finite numbers in the float32 range; dimensions are nonnegative. Activation
is between zero and one. Point positions and circle text positions are fractions
of a turn, clockwise from twelve o'clock. Positive dimensions describe the
authoring space; zero dimensions request the receiving host's native coordinates.
Tuples preserve drawing order. `replace()` changes one value; it does not rewrite
other values that reference the original object.

`encode_scene` preserves shared circle and point identities, including invisible
anchors. Distinct objects that compare equal remain distinct wire tables.
`decode_scene` returns owned Python values, preserving those shared references.
It accepts `bytes`, `bytearray` or `memoryview` and snapshots mutable input before
reading it. Malformed known fields raise `ValueError`. Optional text and circle
coordinates use their schema defaults when absent.

The wire stores floating-point values as float32, so a roundtrip can round Python
floats. Unknown schema fields are not represented in `SceneDefinition` and are
lost if decoded values are re-encoded. Forward exact original bytes when a relay
must retain fields it does not understand.

`SceneBuilder` exposes the lower-level table and offset operations for callers
that need explicit wire construction. One builder finishes one scene; create
another for the next frame. Generated `Circle`, `Point`, `Edge`, `Box`, `Vec2` and
`Scene` modules are the raw schema layer. Public authoring types are annotated
and the distribution includes `py.typed`.

## Transport and native integration

The standard-library sender handles IPv4 and IPv6. It owns one socket and sends
one datagram per call; keep it open while sending a sequence of frames:

```python
from SpellCircle import SceneSender, encode_scene

with SceneSender("127.0.0.1", 27015) as sender:
    sender.send(encode_scene(scene))
```

`send_once(payload, host, port)` opens and closes a sender for a single message.
Receivers can pass bytes from their own socket, file or IO framework directly to
`decode_scene`. The package does not start a thread or event loop. Rendering and
network delivery policy belong to the consuming application. The wire format is
shared by the Qt and native macOS receiver hosts.

The native Sigil package remains an independent optional dependency. With a
matching `sigil-sketch` installation, this example exchanges a scene through
SigilIO UDP feeds and writes and reloads the exact datagram through a file mount:

```sh
python -m SpellCircle.examples.native_io --output scenes/native.bin
spellcircle inspect scenes/native.bin
```

It runs headlessly and closes its feeds on completion or failure. Installing
`ifrit-protocol-apps` does not install or compile Sigil. Native drawing, image
export, publication and sketches use the peer `sigil` package's own APIs.

Installed examples also include `SpellCircle.examples.send_spell_circles` for a
static scene and `SpellCircle.examples.animate_spell_circles` for animation.
Each module accepts `--help`; the static example's `--output` writes a file
instead of opening a socket.

## Development

From this directory with the package installed:

```sh
python -m unittest discover -s test -p 'test_*.py' -v
```

The suite covers model ownership, codec boundaries, shared references, files,
CLI commands and real loopback datagrams. The native IO test skips when `_sigil`
is absent; when present, its interpreter and the `sigil` package must match it.

With basedpyright installed, check valid authoring and expected type errors:

```sh
python test/check_typing.py --checker /path/to/basedpyright --python /path/to/environment/bin/python
```

This checks the installed package from outside the checkout. Add `--extra-path .`
to check the source package instead.

The wire schema belongs to `apps/spell-circle-canvas/src/spellcircle/shared/schema/`.
Run that application's `python3 scripts/sigil.py flatbuffers` after a schema
change. The generator writes only the six generated modules in `SpellCircle/`;
edit the handwritten model and codec separately when the public API grows.

From the parent workspace, `uv build --package ifrit-protocol-apps` builds the
source archive and wheel independently of native Sigil. Each workspace member
owns its own build backend and runtime dependencies.
