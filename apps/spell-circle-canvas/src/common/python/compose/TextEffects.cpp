#include <pybind11/chrono.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/typography/Entrance.h>
#include <sigilcompose/typography/Presets.h>
#include <sigilcompose/typography/TextEffect.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/TextUnit.h>
#include <sigilcompose/typography/Track.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/motion/Registration.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace sigil::python {

namespace {
namespace py = pybind11;
using compose::GlyphModifier;
using compose::Phase;
using compose::TextEffect;
using compose::Track;
using compose::textFx::Displaced;
using GlyphModifierKeyframe = motion::Keyframe<GlyphModifier>;

/** The four-character tag a variable axis is named by, held as the
 *  terminated array the native doors take. */
struct AxisTag {
  char name[5];
};
AxisTag axisTag(const std::string& tag) {
  if (tag.size() != 4)
    throw py::value_error("A variable axis tag is four characters.");
  return {{tag[0], tag[1], tag[2], tag[3], '\0'}};
}

/** The track a text leaf's glyphs run on: which of them, by what unit,
 *  on what schedule, and the master progress the whole track is read
 *  at. Its effect is the entrance it was made by. */
void bindTrack(py::module_& composition) {
  py::enum_<compose::Beats>(
      composition, "Beats",
      "What a track's stagger counts over: the glyphs it selects, or every "
      "glyph of the text.")
      .value("Selection", compose::Beats::Selection)
      .value("Text", compose::Beats::Text);
  auto track =
      bindRecord<Track>(composition, "Track", "A track has no field named ");
  track.doc() =
      "Which glyphs of a text leaf move, by what unit, on what schedule, "
      "and how far along they are: `progress` is the master reading, one "
      "for every glyph at rest, and binds a motion value.";
  track.def_readwrite("where", &Track::where)
      .def_readwrite("effect", &Track::effect)
      .def_readwrite("tween", &Track::tween)
      .def_readwrite("unit", &Track::unit)
      .def_readwrite("innerUnit", &Track::innerUnit)
      .def_readwrite("beatsOver", &Track::beatsOver)
      .def_property(
          "progress", [](const Track& self) { return self.progress; },
          [](Track& self, py::handle value) {
            self.progress = motionAnimatable(value);
          })
      .def_readwrite("progressOverSpan", &Track::progressOverSpan,
                     "Whether a progress described as a motion runs over the "
                     "track's schedule span instead of the duration it "
                     "names. An entrance sets it beside the mount entrance it "
                     "writes.")
      .def_readwrite("reach", &Track::reach)
      .def_readwrite("continuous", &Track::continuous)
      .def(
          "__eq__",
          [](const Track& left, const Track& right) { return left == right; },
          py::arg("other"));
}

/** What a composer reads back about a laid-out text: one beat of a track's
 *  schedule where the text put it, and one unit a selector addressed. */
void bindReadings(py::module_& composition) {
  auto beat = py::class_<compose::Beat, motion::Beat>(
      composition, "Beat",
      "One beat of a track's resolved schedule and where the text put it: "
      "the schedule's own reading, and the rect of the glyphs the beat "
      "addresses in the composer's space.");
  beat.def_readonly("rect", &compose::Beat::rect)
      .def("copy", [](const compose::Beat& value) { return value; })
      .def(
          "__eq__",
          [](const compose::Beat& left, const compose::Beat& right) {
            return left == right;
          },
          py::arg("other"));
  copyProtocol(beat);
  auto unit = py::class_<compose::TextUnit>(
      composition, "TextUnit",
      "One unit a selector addressed in a laid-out text: its rect in the "
      "composer's space, its ordinal, the baseline or column axis it stands "
      "on, the band it occupies, and the line it landed on.");
  unit.def_readonly("rect", &compose::TextUnit::rect)
      .def_readonly("index", &compose::TextUnit::index)
      .def_readonly("axis", &compose::TextUnit::axis)
      .def_readonly("pitch", &compose::TextUnit::pitch)
      .def_readonly("ascent", &compose::TextUnit::ascent)
      .def_readonly("descent", &compose::TextUnit::descent)
      .def_readonly("writingMode", &compose::TextUnit::writingMode)
      .def_readonly("verticalForm", &compose::TextUnit::verticalForm)
      .def_property_readonly(
          "range",
          // The range's class is registered with the paragraph layout,
          // after the composer that answers these units.
          [](const compose::TextUnit& value) -> py::object {
            return py::cast(value.range);
          },
          "The text the unit covers, as a weave.CharRange of UTF-16 units "
          "into the node's paragraph.")
      .def_readonly("style", &compose::TextUnit::style)
      .def_readonly("lineIndex", &compose::TextUnit::lineIndex)
      .def("copy", [](const compose::TextUnit& value) { return value; })
      .def(
          "__eq__",
          [](const compose::TextUnit& left, const compose::TextUnit& right) {
            return left == right;
          },
          py::arg("other"));
  copyProtocol(unit);
}

void bindDisplaced(py::module_& effects) {
  auto displaced = bindRecord<Displaced>(effects, "Displaced",
                                         "A displacement has no field named ");
  displaced.doc() =
      "Where a glyph stands away from home: offset by dx and dy pixels, "
      "leaning rotateDegrees degrees, at scale, with a seeded scatter of "
      "scatterPx and scatterLeanDegrees per glyph, fading in over fadeOver of "
      "its way and at opacity, and on a variable axis.";
  displaced.def_readwrite("dx", &Displaced::dx)
      .def_readwrite("dy", &Displaced::dy)
      .def_readwrite("rotateDegrees", &Displaced::rotateDegrees)
      .def_readwrite("scale", &Displaced::scale)
      .def_readwrite("scatterPx", &Displaced::scatterPx)
      .def_readwrite("scatterLeanDegrees", &Displaced::scatterLeanDegrees)
      .def_readwrite("fadeOver", &Displaced::fadeOver)
      .def_readwrite("opacity", &Displaced::opacity)
      .def_readwrite("axis", &Displaced::axis)
      .def(py::self == py::self);
}

/** The effect as a value, and the phase of a sequence one ends at: both
 *  compare by what they were built from, which is what lets a described
 *  track with an unchanged effect prune. */
void bindTextEffect(py::module_& effects) {
  // Both classes are registered before either signature names the other.
  py::class_<Phase> phase(
      effects, "Phase",
      "One phase of textFx.sequence: an effect, where it ends in local "
      "time, and the share of local time it crossfades into the next phase "
      "over.");
  py::class_<TextEffect> effect(
      effects, "TextEffect",
      "What a track does to each glyph it addresses, as a comparable value: "
      "built by the textFx doors, equal to another built the same way.");
  effect.def("name", &TextEffect::name, "The effect's name.")
      .def("reach", &TextEffect::reach,
           "Pixels beyond the element's box this effect may paint.")
      .def("displaces", &TextEffect::displaces,
           "Whether the effect moves its glyphs off their pen positions.")
      .def("until", &TextEffect::until, py::arg("endsAt"),
           "This effect as a phase of textFx.sequence ending at local time "
           "`endsAt`.")
      .def(py::self == py::self);
  copyProtocol(effect);
  phase
      .def(py::init([](TextEffect effect, float endsAt, float crossfade) {
             Phase made(std::move(effect), endsAt);
             made.crossfade(crossfade);
             return made;
           }),
           py::arg("effect"), py::arg("endsAt") = 1.0f, py::kw_only(),
           py::arg("crossfade") = 0.0f)
      .def(
          "crossfade",
          [](Phase& self, float fraction) -> Phase& {
            return self.crossfade(fraction);
          },
          py::arg("fraction"), py::return_value_policy::reference_internal,
          "Lerps this phase's deviation into the next one's over the last "
          "`fraction` of local time before the joint.")
      .def_property_readonly("effect", &Phase::effect)
      .def_property_readonly("endsAt", &Phase::endsAt)
      .def_property_readonly("overlap", &Phase::overlap)
      .def(py::self == py::self);
  copyProtocol(phase);
  py::implicitly_convertible<TextEffect, Phase>();
}

/** One glyph's deviation from rest, whose three colour terms are each a
 *  colour, the ink in force or a custom property. */
void bindGlyphModifier(py::module_& effects) {
  auto modifier = bindRecord<GlyphModifier>(
      effects, "GlyphModifier", "A glyph modifier has no field named ");
  modifier.doc() =
      "One glyph's deviation from rest: offset by dx and dy pixels, scaled, "
      "turned rotateDegrees degrees, at alpha, its colour multiplied by "
      "colorMultiplier, added to by colorAdd and screened by colorScreen — "
      "each a colour, the ink in force (Fill.currentInk()) or a custom "
      "property (Fill.var(name)) read where the glyph is painted — "
      "stretched by scaleX and scaleY, sheared by skewXDegrees and "
      "skewYDegrees, "
      "drawn at a variable axis coordinate, or as another code point of "
      "equal advance.";
  const auto colorLane = [&modifier](const char* name,
                                     compose::ColorTerm GlyphModifier::* lane) {
    modifier.def_property(
        name, [lane](const GlyphModifier& self) { return (self.*lane).fill(); },
        [lane](GlyphModifier& self, py::handle value) {
          self.*lane = colorTerm(value);
        });
  };
  modifier.def_readwrite("dx", &GlyphModifier::dx)
      .def_readwrite("dy", &GlyphModifier::dy)
      .def_readwrite("scale", &GlyphModifier::scale)
      .def_readwrite("rotateDegrees", &GlyphModifier::rotateDegrees)
      .def_readwrite("alpha", &GlyphModifier::alpha);
  colorLane("colorMultiplier", &GlyphModifier::colorMultiplier);
  colorLane("colorAdd", &GlyphModifier::colorAdd);
  colorLane("colorScreen", &GlyphModifier::colorScreen);
  modifier.def_readwrite("scaleX", &GlyphModifier::scaleX)
      .def_readwrite("scaleY", &GlyphModifier::scaleY)
      .def_readwrite("skewXDegrees", &GlyphModifier::skewXDegrees)
      .def_readwrite("skewYDegrees", &GlyphModifier::skewYDegrees)
      .def_readwrite("axis", &GlyphModifier::axis)
      .def_property(
          "codepoint",
          [](const GlyphModifier& self) {
            return (std::uint32_t)self.codepoint;
          },
          [](GlyphModifier& self, std::uint32_t value) {
            self.codepoint = (char32_t)value;
          },
          "The code point drawn in the glyph's place; 0 draws the glyph.")
      .def(py::self == py::self);

  py::class_<GlyphModifierKeyframe> keyframe(
      effects, "GlyphModifierKeyframe",
      "One stop of a keyframed deviation: where it goes, its share of the "
      "path, and its curve.");
  keyframe
      .def(py::init([](GlyphModifier to,
                       std::optional<motion::Duration> duration,
                       py::handle ease) {
             GlyphModifierKeyframe step{std::move(to), duration, {}};
             if (!ease.is_none()) step.ease = motionEase(ease);
             return step;
           }),
           py::arg("to"), py::arg("duration") = py::none(),
           py::arg("ease") = py::none())
      .def_readwrite("to", &GlyphModifierKeyframe::to)
      .def_readwrite("duration", &GlyphModifierKeyframe::duration)
      .def_property(
          "ease",
          [](const GlyphModifierKeyframe& step) -> py::object {
            if (!step.ease) return py::none();
            return easingReading(step.ease);
          },
          [](GlyphModifierKeyframe& step, py::handle value) {
            step.ease = value.is_none() ? motion::Easing{} : motionEase(value);
          })
      .def(
          "__eq__",
          [](const GlyphModifierKeyframe& left,
             const GlyphModifierKeyframe& right) { return left == right; },
          py::arg("other"));
  copyProtocol(keyframe);
}

/** A keyframe of a deviation from what Python hands one: a keyframe, or a
 *  bare deviation taking an equal share. */
GlyphModifierKeyframe glyphModifierKeyframe(py::handle value) {
  if (py::isinstance<GlyphModifierKeyframe>(value))
    return value.cast<GlyphModifierKeyframe>();
  return {value.cast<GlyphModifier>(), std::nullopt, {}};
}

/** The phases of a sequence from what Python hands it: phases, or bare
 *  effects that each end at the close of local time. */
std::vector<Phase> phasesOf(const py::args& phases) {
  std::vector<Phase> out;
  for (py::handle item : phases)
    out.push_back(py::isinstance<Phase>(item) ? item.cast<Phase>()
                                              : Phase(item.cast<TextEffect>()));
  return out;
}

/** A track over @p effect, its other fields the keywords a caller named;
 *  None leaves a field at the track's own default. */
Track trackOver(TextEffect effect, const py::dict& named) {
  py::kwargs fields;
  for (const auto& [key, value] : named)
    if (!value.is_none()) fields[key] = value;
  Track track = keywordValue<Track>(fields, "A track has no field named ");
  track.effect = std::move(effect);
  return track;
}

}  // namespace

void bindComposeTextEffects(pybind11::module_& module) {
  namespace textFx = compose::textFx;
  auto composition = submodule(module, "compose");
  auto effects = submodule(module, "compose.textFx");
  // The effect is registered before the track that names it, so the
  // track's effect field and parameter are declared by the effect's type.
  bindDisplaced(effects);
  bindMotionDisplacedTween(effects);
  bindTextEffect(effects);
  bindTrack(composition);
  bindReadings(composition);
  bindGlyphModifier(effects);

  effects.def(
      "entrance",
      [](const motion::Tween<Displaced>& tween, std::optional<Track> track) {
        return textFx::entrance(tween, track.value_or(Track{}));
      },
      py::arg("tween"), py::arg("track") = py::none(),
      "THE ENTRANCE, as a track: the tween says what every glyph does — "
      "from_ where it starts — and when, its duration, delay, loop and "
      "alternate the track's schedule. `track` supplies which glyphs, the "
      "unit and the master progress. It plays once when the text mounts: a "
      "track whose progress is left at its default runs 0 to 1 over its "
      "schedule span; a progress the track states stands as written.");
  effects.def("rise", &textFx::rise, py::arg("distancePx") = 26.0f,
              "Each glyph rises from below.");
  effects.def("slide", &textFx::slide, py::arg("distancePx") = -32.0f,
              "Each glyph slides in from the side.");
  effects.def("pop", &textFx::pop, py::arg("fromScale") = 0.35f,
              py::arg("overshoot") = 1.70158f,
              "Each glyph grows from small, past its size and back.");
  effects.def("spinIn", &textFx::spinIn, py::arg("degrees") = 70.0f,
              py::arg("risePx") = 14.0f,
              "Each glyph straightens out of a lean as it rises.");
  effects.def("scatter", &textFx::scatter, py::arg("radiusPx") = 40.0f,
              py::arg("leanDegrees") = 24.0f,
              "Each glyph gathers from a seeded scatter.");
  effects.def("typeOn", &textFx::typeOn,
              "Each glyph appears whole at its turn, as typed.");
  effects.def(
      "variableAxisSweep",
      [](const std::string& tag, float from, float to) {
        const AxisTag axis = axisTag(tag);
        return textFx::variableAxisSweep(axis.name, from, to);
      },
      py::arg("tag"), py::arg("from_"), py::arg("to"),
      "Each glyph sweeps a variable axis from one value to another.");

  effects.def(
      "enter",
      [](const motion::Tween<Displaced>& tween) {
        return textFx::enter(tween);
      },
      py::arg("tween"),
      "THE ENTRANCE, as an effect: the path home the tween describes over "
      "one unit of local progress, for the combinators — the track around "
      "it says when a unit runs.");
  effects.def("waveLoop", &textFx::waveLoop, py::arg("amplitudeEm") = 0.10f,
              py::arg("phaseRadPerGlyph") = 0.5f,
              "Each glyph bobs on a sine, shifted per glyph; read at a "
              "wrapping phase with no stagger.");
  effects.def(
      "scramble",
      [](const std::string& charset, int steps) {
        return textFx::scramble(compose::Utf8(charset), steps);
      },
      py::arg("charset") = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",
      py::arg("steps") = 14,
      "Each glyph churns through the charset, re-rolling `steps` times, "
      "before landing on its own letter by the end of its beat. A "
      "replacement is drawn only where its advance equals the letter's.");
  effects.def(
      "variableAxis",
      [](const std::string& tag, float value) {
        const AxisTag axis = axisTag(tag);
        return textFx::variableAxis(axis.name, value);
      },
      py::arg("tag"), py::arg("value"),
      "Holds an advance-invariant variable axis at one coordinate for every "
      "glyph the track addresses, at draw time.");
  effects.def(
      "tween",
      [](std::optional<GlyphModifier> from, std::optional<GlyphModifier> to,
         py::object keyframes, py::object duration, py::object ease) {
        motion::Tween<GlyphModifier> description;
        if (from) description.from = *from;
        if (to) description.to = *to;
        if (!keyframes.is_none())
          for (py::handle item : py::iter(keyframes))
            description.keyframes.push_back(glyphModifierKeyframe(item));
        if (!duration.is_none())
          description.duration = staggeredDuration(duration);
        if (!ease.is_none()) description.ease = motionEase(ease);
        return textFx::tween(std::move(description));
      },
      py::kw_only(), py::arg("from_") = py::none(), py::arg("to") = py::none(),
      py::arg("keyframes") = py::none(), py::arg("duration") = py::none(),
      py::arg("ease") = py::none(),
      "A KEYFRAMED DEVIATION over one unit of local progress: from_ where "
      "every unit starts, the keyframes it passes through — each a "
      "GlyphModifierKeyframe whose duration is its share of the path, or a "
      "bare GlyphModifier taking an equal share — or `to` alone. Unset, "
      "each segment is straight.");
  effects.def(
      "tint",
      [](py::object from, py::object to, py::object keyframes,
         py::object duration, py::object ease) {
        py::kwargs fields;
        if (!from.is_none()) fields["from_"] = from;
        if (!to.is_none()) fields["to"] = to;
        if (!keyframes.is_none()) fields["keyframes"] = keyframes;
        if (!duration.is_none()) fields["duration"] = duration;
        if (!ease.is_none()) fields["ease"] = ease;
        const py::object tween =
            py::type::of<motion::Tween<material::Color>>()(**fields);
        return textFx::tint(tween.cast<motion::Tween<material::Color>>());
      },
      py::kw_only(), py::arg("from_") = py::none(), py::arg("to") = py::none(),
      py::arg("keyframes") = py::none(), py::arg("duration") = py::none(),
      py::arg("ease") = py::none(),
      "A COLOUR REVEAL: from_ the colour the glyphs read at local 0, `to` "
      "(or the last keyframe) the colour at local 1. Set the leaf in the "
      "colour the tint rests at: the effect multiplies down toward it. "
      "Naming no rest, it comes to rest at the leaf's own ink.");
  effects.def("hold", &textFx::hold, py::arg("effect"),
              "The effect as it is, except that a unit whose beat has not "
              "begun paints nothing.");
  effects.def(
      "sequence",
      [](const py::args& phases) { return textFx::sequence(phasesOf(phases)); },
      "PHASES IN LOCAL TIME: each phase — `effect.until(t)`, or a bare "
      "effect ending at the close — sees its own window as 0 to 1.");
  effects.def(
      "mix",
      [](const py::args& effects) {
        std::vector<TextEffect> operands;
        for (py::handle item : effects)
          operands.push_back(item.cast<TextEffect>());
        return textFx::mix(std::move(operands));
      },
      "BOTH AT ONCE: every effect at the same local time, composed as "
      "stacked tracks compose.");

  // The track a text leaf runs is this feature's, so the verb that hands
  // it one is registered here, after the track's own class.
  py::reinterpret_borrow<py::class_<compose::Text>>(
      py::type::of<compose::Text>())
      .def(
          "textFx",
          [](compose::Text& self, Track track) -> compose::Text& {
            return self.textFx(std::move(track));
          },
          py::arg("track"), py::return_value_policy::reference_internal,
          "Runs this track over the leaf's glyphs: an entrance from "
          "textFx.entrance, its progress the master reading.")
      .def(
          "textFx",
          [](compose::Text& self, TextEffect effect, py::object where,
             py::object tween, py::object unit, py::object innerUnit,
             py::object beatsOver, py::object progress, py::object reach,
             py::object continuous) -> compose::Text& {
            py::dict named;
            named["where"] = where;
            named["tween"] = tween;
            named["unit"] = unit;
            named["innerUnit"] = innerUnit;
            named["beatsOver"] = beatsOver;
            named["progress"] = progress;
            named["reach"] = reach;
            named["continuous"] = continuous;
            return self.textFx(trackOver(std::move(effect), named));
          },
          py::arg("effect"), py::kw_only(), py::arg("where") = py::none(),
          py::arg("tween") = py::none(), py::arg("unit") = py::none(),
          py::arg("innerUnit") = py::none(), py::arg("beatsOver") = py::none(),
          py::arg("progress") = py::none(), py::arg("reach") = py::none(),
          py::arg("continuous") = py::none(),
          py::return_value_policy::reference_internal,
          "Runs `effect` over the leaf's glyphs as a track whose other "
          "fields are the keywords named: which glyphs, the schedule, the "
          "unit, the master progress.")
      .def(
          "variationDrive",
          [](compose::Text& self, const std::string& tag,
             py::handle value) -> compose::Text& {
            const AxisTag axis = axisTag(tag);
            return self.variationDrive(axis.name, motionAnimatable(value));
          },
          py::arg("tag"), py::arg("value"),
          py::return_value_policy::reference_internal,
          "Drives an advance-invariant variable axis from a number or a "
          "motion value at draw time, with no reshape.");
  submodule(module, "core.noise");
}

}  // namespace sigil::python
