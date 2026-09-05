#pragma once

/** @file
 * SigilCompose typography — the feature umbrella over the text
 * vocabulary: the laid-out unit, the two selector forms that address a
 * compose description, the effect and the effects the runtime evaluates
 * by structure, the track and the beat, the reading beside the type, and
 * the path a run's baseline can be.
 *
 * The TEXT'S OWN vocabulary is SigilWeave's and is included from there:
 * mixed text and the story (`weave::rich`, `weave::Story`), the
 * granularity (`weave::unit::Word`), the selector value and every form
 * that addresses the text itself (`weave::sel::`), a style's own numbers
 * and the face behind them (`weave::textStyle`,
 * `weave::ports::pickTypeface`). The stock effects over the seam are the
 * kit's (`kit/Kinetic.h`).
 */

#include "sigilcompose/typography/Annotation.h"
#include "sigilcompose/typography/Selector.h"
#include "sigilcompose/typography/TextEffect.h"
#include "sigilcompose/typography/TextPath.h"
#include "sigilcompose/typography/TextUnit.h"
#include "sigilcompose/typography/Track.h"
