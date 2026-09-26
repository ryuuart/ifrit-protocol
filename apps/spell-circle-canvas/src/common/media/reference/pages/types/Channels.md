---
kind: type
library: SigilMedia
name: Channels
qualified: sigil::media::Channels
group: Images
status: stable
---

# Channels

The raw decoded colour data: every channel the source carries, as named
interleaved float planes — the format-neutral bridge between decoders
and consumers, Skia and any library that wants numbers rather than a
picture.

## Description

LDR sources — PNG, JPEG and the rest through Skia's codecs — arrive as
premultiplied R, G, B, A normalised to 0..1. Float sources — EXR, float
TIFF — keep their full HDR range and their channel names, such as
`glow.R` or `depth.Z`. Multi-part EXR parts matching the base
dimensions merge in with their part name as the prefix. It is
`hub.load<sigil::media::Channels>(uri)`, or `sigil::media::decode` over
bytes in hand.

`Channels::width`, `Channels::height` and `Channels::floatingPoint`
describe the raster; `Channels::names` is the channels in source order
and `Channels::data` the interleaved planes, width times height times the
channel count.

`Channels::index` is a channel's position by exact name, and -1 when
absent. `Channels::at` is one channel of one texel: the caller states a
texel inside the raster and a channel this data carries, and anything
else is a programming error caught in a debug build.

## Back into a picture

`Channels::image` composites channels into a one-frame `Image`. The layer
overload selects a channel group exactly as `ImageOptions::layer` does:
empty is plain R, G, B, A; a luminance channel repeats; a missing alpha
is 1. Float data lands as 32-bit float RGBA and LDR as the platform's
native 32-bit type. It answers null when the layer names nothing.

The `ChannelPick` overload composites explicit channel indices instead —
`ChannelPick::red`, `ChannelPick::green`, `ChannelPick::blue`,
`ChannelPick::alpha`. An index this data does not carry — negative, or
past the channels it holds — is missing: alpha fills with 1, and green
and blue repeat red.

## See also

- `image/Channels.h` — the header: `Channels`, `ChannelPick`
- [Image](Image.md) — the decoded document, for a caller who wants
  pixels rather than numbers
