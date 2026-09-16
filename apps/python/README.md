# Python packages

This uv workspace owns two independently installable distributions:

| Project | Import | Purpose |
| --- | --- | --- |
| `ifrit-protocol-apps`, in this directory | `SpellCircle` | Record vector scenes, serialize FlatBuffers and send them over UDP |
| `sigil-sketch`, in `sigil/` | `sigil` | Use native Sigil libraries, render Python sketches and launch Sketchbook |

## SpellCircle transport

The root project requires Python 3.11 or newer and the `flatbuffers` package.
It installs with a normal Python package installer, without CMake, a graphics
device or the native Sigil extension. From this directory, install it into an
existing environment:

```sh
uv pip install --python /path/to/environment/bin/python .
```

```python
from SpellCircle import SceneSender, SpellCircleCanvas

canvas = SpellCircleCanvas(width=1000, height=1000)
canvas.circle("outer", center_x=500, center_y=500, radius=400)
with SceneSender("127.0.0.1", 27015) as sender:
    sender.send(canvas.to_bytes())
```

The canvas records ordinary Python values. Serialization belongs to
`SceneBuilder`; `SceneSender` owns a standard UDP socket. Importing `SpellCircle`
does not import `sigil` or load native drawing libraries. TouchDesigner consumes
this root project independently and can retain its Python 3.11 environment.
Workspace-wide dependency resolution also includes the native member's Python
requirements; use the independent install for an environment intended only for
the transport package.

The generated schema modules in `SpellCircle/` are committed. Run the native
application's `scripts/sigil.py flatbuffers` command after changing the schema;
it replaces only generated table modules and preserves the handwritten API.

## Native Sigil authoring

The `sigil/` project contains the public package, type declarations, examples,
contract tests and wheel tooling. Its compiled bindings live under
`apps/spell-circle-canvas/src/common/python/`; the native sketch adapter lives
under `apps/spell-circle-canvas/src/sketch/python/`. The package requires Python
3.12 or newer. Wheels target a particular interpreter ABI and platform.

The workspace selects this distribution with `--package sigil-sketch`. Build
its source archive from this directory:

```sh
uv build --package sigil-sketch --sdist
```

Running `uv build` from `sigil/` selects the same project. A wheel build also
needs the native toolchain and dependency settings documented in
[`sigil/README.md`](sigil/README.md); uv does not inherit the application's CMake
presets. Neither command builds the lightweight transport distribution into
the native wheel. The native project's README describes interpreter matching,
standalone rendering, live authoring and the supported binding surface.
