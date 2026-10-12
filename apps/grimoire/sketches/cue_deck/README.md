# Cue deck

A page that draws a show's cues as a TouchDesigner patch sends them: the
show's name sliding across a marquee, a meter for every channel's level,
the bar with its markers and the playhead on the beat, and the doors'
own account of themselves. The same cue can arrive through three doors,
and each hands the page the same `data.Json`:

| Door | Wire | Read |
| --- | --- | --- |
| `udp://:27025` | JSON text, one datagram a cue | as it arrives, with no schema |
| `osc://:27026` | OSC messages, one value each | by address pattern, gathered into a cue |
| `udp://:27027` | a `Cue` FlatBuffer, one datagram a cue | through `cue_deck.bfbs`, into the schema's JSON form |

The page draws the newest cue, whichever door it came through, and names
that door. The canvas is 1280 × 800.

[The guide](../../../spell-circle-canvas/docs/guides/sigilio-flatbuffers-touchdesigner.md)
walks through every file here, the TouchDesigner patch and the Syphon
return, step by step.

## The files

| File | What it is |
| --- | --- |
| `cue_deck.py` | The sketch: three doors through `sigil.data.connect`, the OSC door's handlers, and the page drawn from the newest cue. |
| `sender.py` | Ten cues a second on `--wire json` (the default), `osc` or `flatbuffers`, or `--record` to write that wire's recording under `data/`. The JSON and OSC wires need the standard library alone. |
| `data/cue.feed` | Six seconds of JSON cues, the recording a capture replays at the JSON door. |
| `data/silence.feed` | A recording with nothing in it, which a capture replays at the other two doors so a still opens no socket. |
| `cue_deck.fbs` | The schema: a `Cue` holds a show name, a `Channel` (name, level) per meter, a `Color`, a `Marker` (name, beat) list and the beat. |
| `cue_deck.bfbs` | The binary schema, written by the build from `cue_deck.fbs`. It is not committed and not edited. |
| `CueDeck/` | The Python modules `flatc --python` generates from `cue_deck.fbs`. They are committed, so a FlatBuffer sender needs no build, and never edited. |

`touchdesigner/scripts/cue_deck.py` at the repository root is the
TouchDesigner side: an Execute DAT that sends the patch's cue as JSON
every frame through a UDP Out DAT, with the FlatBuffer route beside it
and the OSC route, an OSC Out CHOP and no script, described in its
docstring.

## Run it

From this directory:

```sh
python3 sender.py &
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire cue_deck.py --publish "Cue Deck"
```

The window listens on all three ports and draws each cue as it lands;
Syphon clients see the canvas under `Cue Deck`. `python3 sender.py --wire
osc` or `--wire flatbuffers` sends the same cues through the other
doors. A still replays the recordings instead of opening the ports, so
it is the same picture every run:

```sh
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire cue_deck.py --frame cue_deck.png
```

## When the cue changes

A field added to the JSON cue is read by name in the sketch, with
nothing regenerated. Write the recording again with
`python3 sender.py --record`.

Once the schema carries it, edit `cue_deck.fbs`, then regenerate the
Python modules:

```sh
../../../spell-circle-canvas/build/vcpkg_installed/arm64-osx/tools/flatbuffers/flatc --python -o . cue_deck.fbs
```

and build the `SigilSketchSchemas` target, which writes `cue_deck.bfbs`
again. `flatc` writes an empty `CueDeck/__init__.py` with the modules;
it is what makes `CueDeck` a package and is kept.

## What it puts under load

A Python sketch reading one value through three dialects of one
connection: JSON text read as it arrives, OSC messages dispatched to
handlers by address pattern, and FlatBuffers read through a schema the
build compiled from a `.fbs` beside it, with arrivals that do not fit
counted as undecodable. The newest message is kept whole between
arrivals, the doors are compared by when their newest message arrived,
and a recording is replayed at each live URI. The marquee is one
measured pass of the name repeated and slid on a binding of one clock,
so nothing is described again between two cues.
