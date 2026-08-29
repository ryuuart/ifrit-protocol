#pragma once

/** @file
 * SigilCompose kernel — data-driven, cacheable, animated drawable
 * components for any SkCanvas.
 *
 * The kernel is: Element descriptions built by fluent value builders,
 * component functions over plain data (+ memo), and a Composer that
 * reconciles by key, lays out via Yoga (SigilWeave-measured text leaves),
 * paints with explicit stacking, caches provably-static subtrees as
 * SkPictures automatically, and animates through Choreograph driven by
 * an sigil::motion::Ticker.
 *
 * THE TWO WRITE PATHS, and the reason they are separate. Structure and
 * discrete state arrive by DESCRIBE — `Composer::render()` /
 * `renderSlot()`, reconciled by key. Per-frame values arrive by BIND — a
 * non-owning pointer to a live `choreograph::Output` the host steps —
 * and a binding is paint-only: it never relayouts. Because both paths are
 * DECLARED, the library can decide, without running anything, which
 * subtrees cannot have changed, and cache exactly those. Every automatic
 * cache in this library rests on that property. The corollary is an author
 * obligation: anything that changes without a re-describe must say so,
 * because nothing introspects a type-erased value.
 */

// The kernel in dependency order. Each header stands on its own; include
// the one a translation unit needs, or this file for all of them.
#include "sigilcompose/Composer.h"
#include "sigilcompose/Derive.h"
#include "sigilcompose/Element.h"
#include "sigilcompose/Env.h"
#include "sigilcompose/Layout.h"
#include "sigilcompose/Mask.h"
#include "sigilcompose/Motion.h"
#include "sigilcompose/Paint.h"
#include "sigilcompose/Shape.h"
#include "sigilcompose/Stroke.h"
#include "sigilcompose/Text.h"
