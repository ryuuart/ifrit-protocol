"""Verb: workspace — a folder a sketch lives in, outside this tree.

    sigil.py workspace new <dir>

A sketch does not have to live in this repository: Sketchbook takes a
`.cpp` path wherever it stands, compiles it with the flags this build
captured, and hot-swaps it on every save. What such a folder holds is a
convention and nothing more — this writes that convention once, so a
new one starts from a file that runs rather than from an empty
directory.

It writes files and nothing else: no build tree, no CMake package, no
install step. The folder is bound to THIS checkout at this build time,
because the flags the sketch compiles with are the ones beside the
Sketchbook binary.
"""

import argparse
import re
from pathlib import Path

from sigil import tree

# The scaffold sketch and the folder's own README. @STEM@ is the
# directory's name, which is the sketch's key; @NAME@ the type the
# registration macro is handed.
SKETCH = """// @STEM@.cpp — a sketch that lives beside its own files, outside the
// repository that compiles it. Open it:
//
//     Sketchbook @STEM@.cpp
//
// Then edit and save: the canvas rebuilds and swaps in a couple of
// seconds, and the last good build stays on screen while a build is
// broken. README.md beside this file says what the rest of the folder
// is for.

#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <memory>

namespace sketch = sigil::sketch;

// `kit` is not aliased here: the compose vocabulary below carries a kit
// of its own, and one word for two of them is ambiguous.
using namespace sigil::compose;

namespace {

struct @NAME@ {
  /** Anything the sketch did not generate itself arrives through the
   *  asset store. A file that is not there yet answers with the
   *  placeholder and heals the moment one appears, so this draws before
   *  assets/ holds anything at all. */
  std::shared_ptr<const sigil::image::ImageAsset> reference;

  Element describe(sketch::SketchContext& ctx) {
    // THE LOOK IS THE THEME'S, carried down the tree: the root states
    // the theme's registers as classes, and a leaf names the one it is
    // set in rather than spelling a size and a colour of its own.
    return stack()
        .styleSheet(sketch::kit::theme().styleSheet())
        .children({text(u8"@STEM@").styleClass("title").inset(64, 56, 64, 0),
                   text(u8"edit this file and save")
                       .styleClass("subtitle")
                       .inset(64, 92, 64, 0),
                   image(reference)
                       .width(240)
                       .height(240)
                       .corners({18})
                       .clip()
                       .inset(64, 150, 64, 0)});
  }

  void setup(sketch::SketchContext& ctx) {
    // The canvas this sketch wants, and the moment a still of it is
    // worth taking. The ground is the theme's unless this says another.
    sketch::kit::stage(ctx, {.size = {960, 600}, .captureAt = 0.4});
    // Drop a reference.png into assets/ and it appears here.
    reference = ctx.assets.image("reference.png");
    ctx.composer.render(describe(ctx));
  }
};

}  // namespace

SIGIL_SKETCH(@NAME@, "Workspace", "A sketch that lives beside its own files")
"""

README = """# @STEM@ — a sketch and the files it stands on

This folder is one sketch. It is not part of the repository that
compiles it, and it needs nothing from that repository but a built
Sketchbook.

```
@STEM@/
@TREE@
```

## Opening it

```sh
@SKETCHBOOK@ \\
    @STEM@.cpp
```

Then **edit the file and save**: it is compiled and swapped into the
running canvas in a couple of seconds, with the last good build still on
screen while a build is broken. A header beside it, reached by a quoted
include, is watched too, and so is every other `.cpp` in this folder —
a `.cpp` standing in a directory that carries its own stem is the ENTRY
of a directory sketch, and every other `.cpp` beside it is a unit
compiled into the same sketch.

**The folder is bound to one checkout at one build time.** What it
compiles against are the flags that build captured, in `sketch_flags.rsp`
beside the Sketchbook binary; after rebuilding that checkout's libraries,
restart Sketchbook before opening this again.

## What it can be asked for

```sh
Sketchbook @STEM@.cpp                      # the window, live
Sketchbook @STEM@.cpp --frame out.png      # one still
      [--at <sec>] [--scale <n>] [--frames <n>] [--gpu]
Sketchbook @STEM@.cpp --bench              # the frame-time gate
Sketchbook @STEM@.cpp --gpu --publish      # the frames, to other applications
```

`--publish` offers every frame this canvas draws to other applications
on the machine, under this folder's name; the window turns it on and off
with Ctrl-P.

**The registry lanes are not for a file opened by path.** `--headless`
and `--video` walk the compiled-in registry, which this file is not in —
a sketch here is photographed with `--frame` and measured with `--bench`,
one file at a time.

## Where its files come from

`assets/` is what mounts at `res://` for this sketch, so
`ctx.assets.image("reference.png")` reads `assets/reference.png`; a file
that is not there answers with a placeholder and heals when one appears.
`--assets <dir>` names another directory instead. A file this sketch
carries as its own data stands under `data/` here and is reached as
`ctx.local("data/rows.csv")`, which the asset store's `table()`,
`image()` and `database()` take as they take any URI.

`captures/` is where the window's Capture writes the frame on screen.
"""


def identifier(stem: str) -> str:
    """The type name the registration macro is handed: the folder's name
    with every run of non-alphanumerics read as a word break."""
    words = [word for word in re.split(r"[^0-9A-Za-z]+", stem) if word]
    return "".join(word[:1].upper() + word[1:] for word in words) + "Sketch"


def tree_block(stem: str) -> str:
    """The folder's listing, its remarks ranged on one column whatever
    the sketch is called."""
    rows = [
        (f"{stem}.cpp", "the sketch: the entry, named for this folder"),
        ("assets/", "what mounts at res:// for this sketch"),
        ("captures/", "where the window's Capture writes"),
        ("README.md", "this file"),
    ]
    column = max(len(name) for name, _ in rows) + 3
    return "\n".join(f"  {name:<{column}}{remark}" for name, remark in rows)


def new(directory: Path) -> int:
    stem = directory.name
    if not stem:
        tree.fail("workspace new wants a directory to write, not a bare path")
    name = identifier(stem)
    if name == "Sketch" or stem[0].isdigit():
        tree.fail(
            f"'{stem}' does not name a sketch: the folder's name is the "
            "sketch's own, and a C++ type is made from it"
        )
    entry = directory / f"{stem}.cpp"
    readme = directory / "README.md"
    for standing in (entry, readme):
        if standing.exists():
            tree.fail(f"{standing} is already there; nothing was written")

    for folder in (directory, directory / "assets", directory / "captures"):
        folder.mkdir(parents=True, exist_ok=True)
    sketchbook = tree.bin_dir("Release") / tree.SKETCHBOOK_IN_BUNDLE
    entry.write_text(SKETCH.replace("@STEM@", stem).replace("@NAME@", name))
    readme.write_text(
        README.replace("@TREE@", tree_block(stem))
        .replace("@SKETCHBOOK@", str(sketchbook))
        .replace("@STEM@", stem)
    )

    print(f"wrote {directory}")
    for written in (entry.name, "assets/", "captures/", readme.name):
        print(f"  {written}")
    print(f"\nopen it with:\n  {sketchbook} {entry}")
    return 0


def main(argv: list) -> int:
    parser = argparse.ArgumentParser(
        prog="sigil.py workspace",
        description="a folder a sketch lives in, outside this tree",
    )
    verbs = parser.add_subparsers(dest="what", required=True)
    scaffold = verbs.add_parser("new", help="write a new workspace folder")
    scaffold.add_argument(
        "directory",
        type=Path,
        help="the folder to write; its name is the sketch's own",
    )
    args = parser.parse_args(argv)
    return new(args.directory.expanduser().resolve())
