# Python packages

This virtual uv workspace groups independently installable Python projects.
The workspace root owns the member list and lockfile; each member owns its
package, dependencies, examples, tests and build backend.

| Project | Import | Purpose |
| --- | --- | --- |
| `sigil-sketch`, in `sigil/` | `sigil` | Use native Sigil libraries, render Python sketches and launch Grimoire |

## Native Sigil authoring

The `sigil/` project contains the public package, the table and fragments
its declarations are generated from, examples, contract tests and wheel
tooling. The declarations themselves are a build product: the pass that
writes them runs when the extension links and puts them in the build tree,
so nothing generated is a file here. Its compiled bindings live under
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
presets. The native project's README describes interpreter matching,
standalone rendering, live authoring and the supported binding surface.

## Workspace development

Select a project explicitly for workspace commands:

```sh
uv run --package sigil-sketch sigil render sketch.py
```

Workspace resolution uses the intersection of its members' Python requirements,
currently Python 3.12 or newer. Adding a project means creating another member
directory with its own `pyproject.toml` and listing it in the workspace; it does
not make that project a runtime dependency of the existing members.
