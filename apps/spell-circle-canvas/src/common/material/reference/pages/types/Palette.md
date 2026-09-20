---
kind: type
library: SigilMaterial
name: Palette
qualified: sigil::material::Palette
header: sigilmaterial/color/Color.h
group: Colour
python: sigil.material.Palette
status: stable
---

# Palette

AN ORDERED TABLE OF COLOURS READ BY INDEX — the fixed palette, which is a
different thing from a ramp and is not a ramp with more stops.

A ramp says what lies BETWEEN its stops; a palette says there is nothing
between its entries. An indexed picture's colour IS entry n, and blending
entry n with entry n+1 makes a colour the palette does not contain —
which is the one thing a fixed palette exists to prevent, and what a
linear-filtered lookup silently does at every boundary. So every read
here is EXACT.

## Anatomy

`Palette::entries` is the table, in order. `Palette::size` and
`Palette::empty` are its extent.

`Palette::at` takes an INDEX and answers that entry exactly.
`Palette::nearest` takes a unit position and answers the entry it falls
IN — the table divided into equal bands, with 1 landing on the last one.
That is the reading a normalised parameter is quantised through: a
height, a heat, a depth.

Out of range CLAMPS rather than wrapping. An index past the end is a
mistake somewhere upstream, and answering the last entry keeps the
mistake visible as a flat band instead of hiding it as a plausible colour
from the other end of the table. An empty palette answers transparent
black, which is the only colour a table with no entries can honestly
give.

## Make one

| Spelling | Language | What it gives |
| --- | --- | --- |
| `Palette{.entries = {...}}` | C++ | the table written out |
| `palette(ramp, entries)` | C++ | a continuous ramp posterised: N colours read at the centres of N equal bands |
| `palette(pixels, options)` | C++ | the table a run of pixels is made of — the colours extracted from an image's own pixels |
| `harmony(base, scheme, spreadDegrees)` | C++ | the colours around one hue, base first |
| `skia::palette(image, options)` | C++ | the same extraction from a Skia image |
| `material.Palette(entries=[...])` | Python | direct |
| `material.palette(ramp, entries)`, `material.harmony(...)` | Python | the same two derivations |

## Pass it to

| Where | Kind | Library |
| --- | --- | --- |
| `ramp` | function | SigilMaterial — the way back to a continuous reading |
| `closestEntry` | function | SigilMaterial — the entry a colour is nearest to |
| `skia::paletteImage` | function | SigilMaterial — the table as an N by 1 image, which is how a fixed palette reaches a shader |
| `skia::paletteLookup` | function | SigilMaterial — the same table as a paint a slot takes, sampled nearest at texel centres |

## Also returned by

| What | Kind | Library |
| --- | --- | --- |
| `palette` | function | SigilMaterial — from a ramp, and from a run of pixels |
| `harmony` | function | SigilMaterial |
| `skia::palette` | function | SigilMaterial |

## Description

The two readings are separate members on purpose. `Palette::at` is the
indexed read an indexed picture needs; `Palette::nearest` is the
quantising read a continuous parameter needs. Neither blends, and there
is no third member that does — a caller who wants a blend is asking for a
ramp and should say so by building one.

Reaching a shader is the same rule one layer down. `skia::paletteImage`
writes the table as one texel per entry, straight rather than
premultiplied so an entry's own alpha survives, and the shader samples it
NEAREST at texel centres — because an index texture read with linear
filtering samples a blend of two unrelated entries, which is the exact
failure the type exists to prevent.

## See also

- `color/Color.h` — the header: `Palette`, `Color`, `RampStop`,
  `sampleRamp`
- `color/Extract.h` — the header: `palette`, `closestEntry`
- `color/Harmony.h` — the header: `harmony`, `rotateHue`, `Scheme`
- [Ramp](Ramp.md) — what says there IS something between two colours
- [Color](Color.md) — the entries
