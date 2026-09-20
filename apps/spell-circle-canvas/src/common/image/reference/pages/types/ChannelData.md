---
kind: type
library: SigilImage
name: ChannelData
qualified: sigil::image::ChannelData
group: Decoding
status: stable
---

# ChannelData

The raw decoded colour data: every channel the source carries, as named
interleaved float planes — the format-neutral bridge between decoders
and consumers, Skia and any library that wants numbers rather than an
image.

## Description

LDR sources — PNG, JPEG and the rest through Skia's codecs — arrive as
premultiplied R, G, B, A normalised to 0..1. Float sources — EXR, float
TIFF — keep their full HDR range and their channel names, such as
`glow.R` or `depth.Z`. Multi-part EXR parts matching the base
dimensions merge in with their part name as the prefix.

`ChannelData::width`, `ChannelData::height` and
`ChannelData::floatingPoint` describe the raster;
`ChannelData::names` is the channels in source order and
`ChannelData::data` the interleaved planes, width times height times the
channel count.

`ChannelData::index` is a channel's position by exact name, and -1 when
absent. `ChannelData::at` is one channel of one texel: the caller states
a texel inside the raster and a channel this data carries, and anything
else is a programming error caught in a debug build.

## Back into an image

`ChannelData::makeImage` composites channels into an image. The layer
overload selects a channel group exactly as the decode options' layer
does: empty is plain R, G, B, A; a luminance channel repeats; a missing
alpha is 1. Float data lands as 32-bit float RGBA and LDR as the
platform's native 32-bit type. It answers null when the layer names
nothing.

The index overload composites explicit channel indices instead. An index
this data does not carry — negative, or past the channels it holds — is
missing: alpha fills with 1, and green and blue repeat red.

## See also

- `decode/ChannelData.h` — the header: `ChannelData`
- [ImageAsset](ImageAsset.md) — the decoded document, for a caller who
  wants pixels rather than numbers
