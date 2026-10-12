# Cue deck

A page that draws a show's cues as a TouchDesigner patch sends them: the
show's name sliding across a marquee, a meter for every channel's level,
the bar with its markers and the playhead on the beat, and the door's own
account of itself. Every cue arrives as one FlatBuffer on UDP port 27025
and is read through the sketch's own schema. The canvas is 1280 × 800.

[The guide](../../../spell-circle-canvas/docs/guides/sigilio-flatbuffers-touchdesigner.md)
walks through every file here, the TouchDesigner patch and the Syphon
return, step by step.

## The files

| File | What it is |
| --- | --- |
| `cue_deck.fbs` | The schema: a `Cue` holds a show name, a `Channel` (name, level) per meter, a `Color`, a `Marker` (name, beat) list and the beat. |
| `cue_deck.py` | The sketch. It reads `cue_deck.bfbs`, opens `udp://:27025` through `sigil.data.connect` with that schema, and draws the newest cue. |
| `cue_deck.bfbs` | The binary schema, written by the build from `cue_deck.fbs`. It is not committed and not edited. |
| `CueDeck/` | The Python modules `flatc --python` generates from `cue_deck.fbs`. They are committed, so a sender needs no build, and never edited. |
| `sender.py` | A standard-library sender over the generated modules and the `flatbuffers` package: ten cues a second, or `--record` to write `data/cue.feed`. |
| `data/cue.feed` | Six seconds of cues, the recording a capture replays. |

`touchdesigner/scripts/cue_deck.py` at the repository root is the
TouchDesigner side: an Execute DAT that builds the same `Cue` from a
patch's operators every frame and sends it through a UDP Out DAT.

## Run it

From this directory:

```sh
python3 sender.py &
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire cue_deck.py --publish "Cue Deck"
```

The window listens on the port and draws each cue as it lands; Syphon
clients see the canvas under `Cue Deck`. A still replays the recording
instead of opening the port, so it is the same picture every run:

```sh
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire cue_deck.py --frame cue_deck.png
```

## When the schema changes

Edit `cue_deck.fbs`, then regenerate the Python modules and the recording:

```sh
../../../spell-circle-canvas/build/vcpkg_installed/arm64-osx/tools/flatbuffers/flatc --python -o . cue_deck.fbs
python3 sender.py --record
```

and build the `SigilSketchSchemas` target, which writes `cue_deck.bfbs`
again. `flatc` writes an empty `CueDeck/__init__.py` with the modules;
it is what makes `CueDeck` a package and is kept.

## What it puts under load

A Python sketch reading a door through a schema the build compiled from
a `.fbs` beside it: arrivals that are buffers read as the schema's JSON
form, arrivals that are not counted as undecodable, the newest message
kept whole between arrivals, and a recording replayed at the live URI.
The marquee is one measured pass of the name repeated and slid on a
binding of one clock, so nothing is described again between two cues.
