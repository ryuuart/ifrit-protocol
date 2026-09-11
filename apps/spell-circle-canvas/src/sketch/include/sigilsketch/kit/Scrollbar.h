#pragma once

/** @file
 * A WINDOW'S SHARE OF WHAT IT SCROLLS: how long the thumb that says so
 * is, how far it can travel, and the bar it travels in.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilmotion/values/Animated.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>

namespace sigil::sketch::kit {

/** THE THUMB A SCROLL ASKS FOR: how long it is, and how far it may go
 *  before it is at the end of what it scrolls. */
struct Thumb {
  float length = 0;
  /** The track less the thumb — the whole of the distance a position of 1
   *  puts it at. Zero where everything is already showing. */
  float travel = 0;
  bool operator==(const Thumb&) const = default;
};

/** WHAT IS SCROLLED, in the length along the bar.
 *
 *  A THUMB IS A READING, NOT A DECORATION: its length is the window's
 *  share of the strip, so a bar drawn at one length while the content is
 *  scrolled over another is a thumb that slides off its own track — or
 *  one that stops short of the end and says the last rows are not there.
 *  Both are wrong pictures, and both are what a hand-guessed thumb
 *  gives, which is why this arithmetic is stated once. */
struct Scrolled {
  /** How much of the strip the window shows. */
  float view = 0;
  /** The whole strip. Anything at or under `view` is everything showing,
   *  which fills the track. */
  float content = 0;
  /** The length the thumb runs along, px — the bar less its steppers. */
  float track = 0;
  /** The shortest the thumb may become, so a long document still has one
   *  to see and to grab. 0 lets it go as short as its share. */
  float minLength = 0;

  [[nodiscard]] Thumb thumb() const;
  bool operator==(const Scrolled&) const = default;
};

/** THE BAR: a stepper at each end, a track between them, and a thumb
 *  standing somewhere along it.
 *
 *  EVERY PART IS THE CALLER'S ELEMENT, because a scrollbar is drawn in
 *  the vocabulary of whatever it belongs to — a bevelled Motif slider, a
 *  styled IE thumb, a pixel sprite — and what is shared between those is
 *  the arithmetic and the arrangement, not the chrome. What this returns
 *  is the bar's own box, so the shell's fill, its keyline, its padding
 *  and its gutter are chained onto it in the caller's own words. */
struct Scrollbar {
  /** true (default) runs the bar down the page, false along it — the
   *  same polarity every other run in this kit reads. */
  bool column = true;
  /** The stepper at the start of the bar and the one at its end; unset
   *  draws none and spends no room. */
  std::optional<compose::Element> leading;
  std::optional<compose::Element> trailing;
  /** THE THUMB'S OWN CHROME. Unset is a flat patch in the theme's figure
   *  colour. Whatever it is, the bar gives it its length and its place
   *  along the track: a thumb that positioned itself would be the second
   *  opinion this component exists to remove. */
  std::optional<compose::Element> thumb;
  /** What is scrolled, which is what the thumb's length and travel are
   *  read off. `track` must be the bar's own track length in px for
   *  either to be a distance. */
  Scrolled scrolled;
  /** THE THUMB'S LENGTH, stated INSTEAD of read off `scrolled` — px or a
   *  share of the track. For a reconstruction whose thumb was measured
   *  off the original rather than computed from anything it scrolls. */
  compose::Dim thumbLength;
  /** Where the window stands: 0 at the start of what it scrolls, 1 at the
   *  end. It is a fraction rather than a distance because the distance is
   *  the travel, which the bar is the one that knows. */
  float at = 0;
  /** THE LIVE POSITION, px along the track, INSTEAD of `at` — for a bar
   *  that moves every frame. The value is the thumb's offset, so a caller
   *  binding one maps its own envelope onto `scrolled.thumb().travel`:
   *  translating the thumb is paint-only, where a placement would be
   *  layout every frame. */
  std::optional<motion::Animatable<float>> position;
  /** How far the thumb stands in from the two long edges of the bar. */
  float thumbInset = 0;
  /** The track the thumb runs in; unset is the theme's cell ground. */
  std::optional<compose::SurfacePaint> track;
};

/** THE BAR.
 *
 *      sketch::kit::scrollbar({.thumb = slider(),
 *                              .scrolled = {rows, whole, trackH},
 *                              .at = read})
 *          .width(Dim(19))
 *          .padding(2)
 *
 *  The track grows into whatever length the bar is given, so a bar beside
 *  a pane that flexes needs no measurement; `Scrolled::track` is what the
 *  same length is called where the thumb's own distances are wanted in
 *  px. Neither a `thumbLength` nor a `scrolled.track` draws no thumb —
 *  an empty track says the length is unknown, where a full one would say
 *  everything is showing. */
[[nodiscard]] compose::Element scrollbar(Scrollbar bar);

}  // namespace sigil::sketch::kit
