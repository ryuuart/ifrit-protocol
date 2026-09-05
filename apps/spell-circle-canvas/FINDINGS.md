# Findings

Defects found while working. Each entry states what the code does, what
it was evidently intended to do, and what a test should assert once
intent is restored. Delete an entry when it is fixed; delete the file
when it is empty.

## A texture-cached fx track draws nothing for a face read from a file

**What the code does.** A `text()` leaf carrying both `.cache(Cache::Texture)`
and an `.fx(track)` paints no pixel at all when its style names a typeface
loaded through `SkFontMgr::makeFromFile` (the instrument faces under
`src/test/assets/`). The same leaf with the cache alone, or the track
alone, paints; the same leaf with all three in the machine's default face
paints. Observed on a raster host at 36, 40 and 41 px with a one-glyph run.

**What it was intended to do.** Paint the run through the cached texture
with the track applied, whatever face the style names — the texture path
and the fx path each already do for that face.

**What a test should assert.** `ComposeTextFx.ATrackReachKeepsAWideThrowInsideTheCull`
(`src/common/compose/typography/test/ComposeTestContentText.cpp`) set in
`whiteStyle(40)` — the instrument face — instead of `machineStyleAt(40)`,
with the `fonts` label and its `SUITES` entry in
`src/common/compose/typography/CMakeLists.txt` removed.
