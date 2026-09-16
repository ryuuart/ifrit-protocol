# Python packages

This virtual uv workspace groups independently installable Python projects.
The workspace root owns the member list and lockfile; each member owns its
package, dependencies, examples, tests and build backend.

| Project | Import | Purpose |
| --- | --- | --- |
| `ifrit-protocol-apps`, in `spellcircle/` | `SpellCircle` | Model and author vector scenes, encode FlatBuffers and exchange scene bytes |
| `sigil-sketch`, in `sigil/` | `sigil` | Use native Sigil libraries, render Python sketches and launch Sketchbook |

## SpellCircle transport

The `spellcircle/` project requires Python 3.11 or newer and `flatbuffers`.
It installs with a normal Python package installer, without CMake, a graphics
device or the native Sigil extension. From this directory, install it into an
existing environment:

```sh
uv pip install --python /path/to/environment/bin/python ./spellcircle
```

The package has no Qt or native Sigil dependency. A consumer can use its scene
values and codec without opening a socket. Its installed examples run as
modules:

```sh
python -m SpellCircle.examples.send_spell_circles --seed 1 --output scenes/example.bin
python -m SpellCircle.examples.animate_spell_circles --fps 60
```

[`spellcircle/README.md`](spellcircle/README.md) describes the scene API and
transport options. TouchDesigner depends on this member by path and retains
its own Python 3.11 environment. The generated schema modules in
`spellcircle/SpellCircle/` are committed. The native application's
`scripts/sigil.py flatbuffers` command replaces only generated table modules
and preserves the handwritten API.

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

## Workspace development

Select a project explicitly for workspace commands:

```sh
uv build --package ifrit-protocol-apps
uv run --package ifrit-protocol-apps python -m SpellCircle.examples.send_spell_circles --output scene.bin
```

Workspace resolution uses the intersection of its members' Python requirements,
currently Python 3.12 or newer. To use or test only SpellCircle with Python 3.11,
install `./spellcircle` directly into a separate environment instead of using
the workspace environment. Adding a project means creating another member
directory with its own `pyproject.toml` and listing it in the workspace; it does
not make that project a runtime dependency of the existing members.
