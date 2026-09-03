#pragma once

/** @file
 * SigilCompose KIT — the umbrella include.
 *
 * The kit is a layer ON TOP of SigilCompose that changes none of it: no
 * new `ElementNode` state, no new reconciler equality, no new paint path.
 * Every component is a free function or a designated-init aggregate over
 * the public API.
 *
 * That boundary is enforced by the build, not by convention. The kit is
 * its own CMake library, `SigilComposeKit`, whose only include path is
 * SigilCompose's public headers — so a kit component cannot reach a
 * library internal even by accident, and the public headers are proven
 * sufficient by the fact that the kit compiles against nothing else.
 *
 * The two tiers also differ in what a mistake costs. A wrong entry in the
 * library's own API is effectively permanent, because callers depend on
 * its spelling; a wrong kit component is simply not `#include`d by the
 * next caller. So the bar here is lower on taste and specific on evidence:
 * a component is built when the same few lines have been written by hand
 * several times over, not when it would be nice to have.
 *
 * | header | component |
 * |---|---|
 * | `kit/Frame.h` | `disc()` — a node about a centre, or at a frame's |
 * | `kit/Frame.h` | `at()` — a box pinned at absolute coordinates |
 * | `kit/Sprites.h` | `dotSprite()` — the stamp a point sink draws with |
 * | `kit/Gel.h` | `aquaGel()`, `aquaOrb()` — one era's look |
 * | `kit/Chrome.h` | `y2kChrome()` — another |
 * | `kit/Gloss.h` | `gloss()` and the two chrome type ramps |
 * | `kit/PixelType.h` | the aliased bitmap-font bake |
 * | `kit/Legibility.h` | halo / shade / scrim |
 * | `kit/Instruments.h` | `trackMeter()` and `restGhost()` — a cascade seen |
 * | `kit/Annotations.h` | `annotate()` — an element per unit, `Beside` it |
 * | `kit/Annotations.h` | …or `Anchored`, where the caller says |
 * | `kit/Typeset.h` | ruby, kenten, drop cap, `NestedStyle` |
 * | `kit/Typeset.h` | bullets, block rules |
 *
 * **`kit/Strokes.h` AND `kit/Plate.h` ARE NOT INCLUDED HERE, AND THIS
 * UMBRELLA DOES NOT GIVE YOU THEM.** Both are spelled in the Brush tier's
 * types and ship with it: `kit/Strokes.h` carries the stroke grammar —
 * `kit::braid`, `spans::brackets` and the `brush::presets` — and
 * `kit/Plate.h` the
 * bordered feed plate (`kit::plate`, `kit::tinted`). Including this file
 * brings in none of those names; include either header directly.
 *
 * Already shipped elsewhere and deliberately NOT duplicated here:
 * `measure::check` / `measure::failures` (SigilMeasure's `check/Check.h`)
 * and `test::report` (`testing/Checks.h`, the SigilComposeTesting target)
 * are the verification plate; `hex`
 * (`Paint.h`), `weave::textStyle`, `weave::ports::pickTypeface` and
 * `motion::ramp` are the prelude; `stroke` (`Decorations.h`) is the value
 * spelling. The figure's own coordinate systems and its division ladders are
 * SigilGeometry's — `geometry::path::Frame`, `geometry::path::Grid`,
 * `geometry::path::centred`, `geometry::shapes::ticks` and
 * `geometry::shapes::chords`.
 */

#include "sigilcompose/kit/Annotations.h"
#include "sigilcompose/kit/Chrome.h"
#include "sigilcompose/kit/Frame.h"
#include "sigilcompose/kit/Gel.h"
#include "sigilcompose/kit/Gloss.h"
#include "sigilcompose/kit/Instruments.h"
#include "sigilcompose/kit/Legibility.h"
#include "sigilcompose/kit/PixelType.h"
#include "sigilcompose/kit/Sprites.h"
#include "sigilcompose/kit/Typeset.h"
