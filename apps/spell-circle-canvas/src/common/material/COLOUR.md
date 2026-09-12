# SigilMaterial — colour

The chapter on the colour leaf, which is the library's bottom: the
colour value a parameter struct holds, the perceptual round trips, the ramp
as one value, the harmonies read around a hue, the dither threshold and
the table a run of pixels is made of. `README.md` beside this file is
the library; `PAINT.md` is the Skia paint the colours are painted with.

## Colour

**THE BOUNDARY: this feature is colour and nothing else.** It links
nothing of this project's and no renderer, so every value here is stated
over colours and numbers — a table of pixels rather than an image, a
function pointer rather than a curve object, a stop list rather than a
gradient. Where a value has to meet a picture, the crossing lives with
the renderer that owns the picture (`skia::palette`) and not here.

`Color` is four straight (not premultiplied) sRGB floats, uploaded as one
float4; `rgb(0xRRGGBB)` is its packed spelling. `Color.h` also holds the
sRGB transfer function both ways and the OKLab round trip — `toOklab`,
`fromOklab`, `lerpOklab` — which every perceptual interpolation in the
codebase runs through, plus OKLCH, its polar form, where the two numbers
a harmony and a tone ladder are stated in — a chroma and a hue — are
named.

**Out of gamut, two answers, and the difference is which fact survives.**
`fromOklab` and `fromOklch` cut the channels to the range, which moves
the hue and the lightness both, because three channels are cut by three
different amounts. `fitToSrgb` reduces the CHROMA until the colour is
displayable and touches nothing else, so a set built by turning one hue
comes back at the hues and the weights it was asked for, duller in the
directions sRGB is narrow in. `inSrgbGamut` and `linearOf` are the two
readings underneath, and they are separate from the conversion because
the numbers BEFORE the clamp are the ones that say whether a colour is a
colour at all.

**ONE RAMP VALUE.** `Ramp` is the stops plus the four decisions a caller
otherwise re-spells at every site: the `space` the walk between two stops
happens in (`Srgb` for what a gradient draws, `Linear` for a quantity of
light, `Oklab` for even steps, `Oklch` for a walk around the hue circle
with `arc` saying which way), an `easing` curve, `reverse`, and the
`domainLow`/`domainHigh` the caller's own numbers are read on. The
easing is a `core::curve::Curve` — SigilCore's shaped curve value, the
same one an animation eases with and a keyed track shapes a segment
with, so a look chosen once walks the same shape wherever it is read.
It carries its own parameters and compares by them, which is what keeps
an eased ramp a value two of which can be proved the same. It answers
`at(v)` and it is CALLABLE, so a ramp is an interpolator: anything that
hands a unit position to one — a data scale's `through()`, a legend, a
table — takes a ramp with no adapter. `sampleRamp` is still the ladder
underneath, for a caller holding bare stops. The named ramps are stock
values over it in the kit (`kit::viridis` and the rest), not types: take
one, move its domain, reverse it, and it is still a ramp.

`palette(ramp, entries)` reads a ramp at BAND CENTRES into a fixed table
— the posterising crossing — and `ramp(palette)` is the way back, which
is a decision the caller makes by asking rather than an inverse.

**A harmony is one function whose scheme is a prop.** `harmony(base,
Scheme, spreadDegrees)` answers a `Palette` with the base first;
`Complement`, `SplitComplement`, `Analogous`, `Triad` and `Tetrad` are
sets of angles on the OKLCH circle, and the spread is read by the three
whose flanks move. `rotateHue` is the one turn underneath, and it is in
OKLCH rather than on the HSV wheel because rotating an HSV hue holds the
largest channel, which is not a brightness — a scheme built that way
lands a yellow and a blue at wildly different weights.

**A palette from a picture is chosen from PIXELS.** `palette(pixels,
options)` works in OKLab, is deterministic with no seed anywhere — the
starting entries are the farthest-first extremes rather than a random
draw — and answers darkest first. The `method` prop is the whole
difference between the two: `KMeans` moves its entries until each is the
average of the pixels closest to it, so its colours are the ones the
picture holds and a small vivid area survives; `MedianCut` divides the
colours into equal-population boxes, so its entries cover the range
whether or not the picture dwells there. `closestEntry` is the read back
the other way, for an indexed picture.

**A dither is a threshold read at a pixel.** `Dither{kind, matrix,
levels, amount}` answers `threshold(x, y)`, the rounded `at(colour, x,
y)`, and the one-bit `on(value, x, y)`. `Ordered` is the recursive Bayer
matrix, where every threshold appears once per tile, so a flat is exactly
the average asked for and the pattern holds still under motion; `Noise`
is a screen-space hash with no period to see. `amount` at zero is plain
rounding, which is the same value saying "no dither" rather than a second
code path.

**Three mixes, and the drawing says which one it means.** `mixToward`
walks the numbers a file stores; `mixLinear` walks the light they stand
for, which is the answer whenever the question is about QUANTITIES — how
much pigment, how much exposure — and is why half way between black and
white is near #BCBCBC there and #808080 in the other; `lerpOklab` walks
what an eye reports. `mixLinear` reads two equal channels as the channel
itself: one quantity of light mixed with itself is that light, and the
transfer function either side of the mix is where the whole cost of the
walk is, so a grey ladder, a single-hue ramp and an alpha-only fade pay
none of it. `luminance()` is what shows the difference: the
code-value midpoint carries a fifth of white's light, the linear one
half. **Two Lab spaces, for two jobs.** OKLab is where colour is
INTERPOLATED, CIELAB (`toLab`, `fromLab`) is where it is MEASURED — it
is the space a published difference is quoted in, and `deltaE` is that
difference, with about 2.3 the point where a side-by-side pair stops
matching. `sampleRamp` reads a `RampStop` ladder on the CPU exactly as a
renderer's gradient draws it, for the caller that needs one colour out of
a ramp rather than a shader.

**A PALETTE IS NOT A RAMP.** A ramp says what lies between its stops; a
`Palette` says there is nothing between its entries, so every read of it
is exact — `at(index)` and `nearest(t)`, clamped at both ends, never a
blend, because a blend of two entries is a colour the table does not
contain and that is the one thing a fixed palette exists to prevent. One
seam, two executors: `Palette::at` is the CPU reading, and
`skia::paletteImage` / `skia::paletteLookup` are the same table crossing
to a shader as an N x 1 texture sampled NEAREST at texel centres, which
is what makes an indexed picture one channel of indices and one child
slot instead of a branch over N literals.

**The arithmetic verbs, and what each of them touches.** `withAlpha`
replaces the alpha and nothing else; `scale` multiplies the three colour
channels and takes the alpha it is given, or keeps the colour's own when
that argument is negative; `lighten` ADDS to the three channels and
saturates at white. The last two are not one verb with a sign: a scale
keeps the hue of what it scales and has no ceiling to hit, an offset
walks every channel toward white and stops there, and a caller
lightening a nearly-white base wants the saturated answer rather than a
channel above 1 that the next blend reads as glow. All three are
constexpr, as `rgb` is, because a palette is a list of constants and the
verbs an authored constant is written through have to fold where it is
written. `skia::withAlpha`, `skia::scale`, `skia::lighten` and
`skia::mixLinear` are the same four answered in `SkColor4f`, for the
consumer whose slots are Skia's — the arithmetic is not restated there,
only crossed.

**Two ways to name a colour, for two different jobs.** `rgb()` is how an
authored palette is typed in; `hsv(hueDegrees, saturation, value)` is how
a palette is WALKED — a wheel, a run of chips on a golden-angle step, one
hue's tone ladder read off saturation and value together. The hue wraps
and the other two clamp, and both folds are in the verb rather than at
the call site because the sextant ladder underneath answers magenta for
any hue it does not recognise, which is exactly what an unwrapped angle
hands it. HSV is not a perceptual space and must not be used as one:
`value` is the largest channel and nothing more, so a full-value yellow
and a full-value blue are nowhere near the same brightness. Anything that
INTERPOLATES goes through `lerpOklab`.

**A Skia colour crosses at one place, and it is `Color` itself.**
`SkColor4f` holds the same four straight sRGB floats in the same order,
so the crossing is a field-for-field copy — no transfer function, no
premultiply, no clamp, so a channel above 1 survives. `Color` is
IMPLICITLY CONSTRUCTIBLE from one, matched by shape rather than by name
(`FourFloatColor`: four float members `fR`, `fG`, `fB`, `fA`), so the
leaf that every parameter struct includes still names no renderer:

```cpp
pattern::stripes(6, 6, kInk);              // kInk is an SkColor4f
sdf::Style style{.fill = kInk, .borderColor = kEdge};
```

`skia::toSkColor` is the way BACK, which a colour cannot carry without
naming Skia, and `skia::toColors` converts a palette in one call;
`skia::toColor` is the same conversion under a name, for a call that
wants to say so (`<sigilmaterial/skia/Color.h>`). The mapping is written
once because a copy of it spelled at a call site is a place where a
channel order or an alpha convention drifts silently.

**A view transform is a baked material with one open slot.**
OpenColorIO's GPU codegen never emits SkSL, so `ocio::viewTransform(
config, display, view)`, `ocio::convert(config, src, dst)` and
`ocio::exponent(gamma)` each build a CPU processor, bake it once (F16,
because F32 textures are not linearly filterable on Apple GPUs), hold
the bake as a texture in the `lut` slot, and apply it through a recipe
whose `content` slot is the layer being transformed and is left to the
renderer. A bad config fails soft to a material with an empty `lut` slot
and the error reported. In a build that found no OpenColorIO the feature
still links: `ocio::available()` is false and every factory answers that
empty material, and `SIGILMATERIAL_ENABLE_OCIO` says which build this is.

**Which recipe depends on whether the transform mixes channels.** A
transform whose channels are INDEPENDENT — an exponent, a gamma, a
contrast, a per-channel display curve — carries no more information than
one response curve per channel, so it bakes to one row of 256 samples
and applies through `responseRecipe()`; a transform that mixes channels
needs the volume and applies through the trilinear `lutRecipe()`, its
slices laid side by side in one image. Independence is ESTABLISHED, not
assumed from the transform's type: the bake reads the three responses off
the grey ramp, then requires a lattice of mixed colours to equal those
three responses composed, to within half an eight-bit code. So `lutSize`
means nothing to a transform that bakes to a row.

**A channelwise recipe does not have to run as a program.**
`Recipe::channelwise(slot)` is the declaration that every output channel
depends on the same input channel and nothing else, with `slot` holding
the response row — and `responseRecipe()` makes it. `skia::Effect::recipe(
material, surface)` is where it is spent: on a surface carrying eight
bits per channel it answers the row as `SkColorFilters::TableARGB`, which
a consumer hangs on a paint and pays a blit for, and on anything else —
a float surface, or `kUnknown_SkColorType`, which is what a canvas backed
by neither raster nor GPU answers — it falls to `recipe(material)` and
the program. The picture is the same either way; only the cost differs,
which is why the surface is a parameter rather than something the effect
guesses. `skia::Effect::filter` takes an `sk_sp<SkColorFilter>` as well
as an `sk_sp<SkImageFilter>`, and `colorFilter()` reads that lane back.
