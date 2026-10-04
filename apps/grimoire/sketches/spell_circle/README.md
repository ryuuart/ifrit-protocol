# Spell Circle

`spell_circle.py` is the Grimoire entry. This project uses CPython 3.14,
matching the native Grimoire host, and installs `sigil-sketch` from the
local wheel recorded in `pyproject.toml` and `uv.lock`.

From this directory, prepare the environment:

```sh
uv sync --no-install-project
```

Select `.venv/bin/python` as the editor's Python interpreter. The installed
package includes the authoring declarations, native binding stubs and
`py.typed` markers; no Pyright `extraPaths` are needed.

Open this directory with Grimoire's **Open Workspace** action, or run:

```sh
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire --workspace .
```

The wheel lives under `apps/spell-circle-canvas/dist/` and must be built or
copied there before syncing a fresh checkout. Package build instructions
are in [the Sigil package README](../../../python/sigil/README.md).
To select a newly built wheel, run `uv add` with its path.
