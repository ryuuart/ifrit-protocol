#include <pybind11/stl.h>
#include <sigilpython/compose/Kit.h>
#include <sigilsketch/kit/Heading.h>
#include <sigilsketch/kit/Panel.h>
#include <sigilsketch/kit/Scrollbar.h>
#include <sigilsketch/kit/Ticker.h>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;
namespace sketchKit = sigil::sketch::kit;
using sigil::python::kit::field;
using sigil::python::kit::record;

namespace {

void bindHeadings(py::module_& module) {
  auto header = record<sketchKit::SectionHeader>(module, "SectionHeader");
  field(header, "label", &sketchKit::SectionHeader::label);
  field(header, "note", &sketchKit::SectionHeader::note);
  field(header, "ruled", &sketchKit::SectionHeader::ruled);
  module.def("sectionHeader", &sketchKit::sectionHeader, py::arg("header"));
  module.def("section", &sketchKit::section, py::arg("header"),
             py::arg("content"));

  auto line = record<sketchKit::Line>(module, "Line");
  line.doc() =
      "One line of a title card: its words, its own ink where the register's "
      "will not do, and the beat it rides in on — an opacity, a lift and a "
      "text track.";
  field(line, "words", &sketchKit::Line::words);
  field(line, "ink", &sketchKit::Line::ink);
  field(line, "opacity", &sketchKit::Line::opacity);
  field(line, "lift", &sketchKit::Line::lift);
  field(line, "textFx", &sketchKit::Line::textFx);
  auto card = record<sketchKit::TitleCard>(module, "TitleCard");
  card.doc() =
      "The card a sequence opens on: an eyebrow over a title over a "
      "subtitle, the notes under them, aligned together and ruled under "
      "the title where asked.";
  field(card, "eyebrow", &sketchKit::TitleCard::eyebrow);
  field(card, "title", &sketchKit::TitleCard::title);
  field(card, "subtitle", &sketchKit::TitleCard::subtitle);
  field(card, "notes", &sketchKit::TitleCard::notes);
  field(card, "align", &sketchKit::TitleCard::align);
  field(card, "ruled", &sketchKit::TitleCard::ruled);
  field(card, "key", &sketchKit::TitleCard::key);
  module.def("titleCard", &sketchKit::titleCard, py::arg("card"));
}

void bindRegions(py::module_& module) {
  auto backdrop = record<sketchKit::Backdrop>(module, "Backdrop");
  backdrop.doc() =
      "The ground a scene stands on: its fill, a vignette toward an edge "
      "colour, and a grain over it.";
  field(backdrop, "ground", &sketchKit::Backdrop::ground);
  field(backdrop, "vignette", &sketchKit::Backdrop::vignette);
  field(backdrop, "edge", &sketchKit::Backdrop::edge);
  field(backdrop, "grain", &sketchKit::Backdrop::grain);
  field(backdrop, "grainScale", &sketchKit::Backdrop::grainScale);
  field(backdrop, "key", &sketchKit::Backdrop::key);
  module.def("backdrop", &sketchKit::backdrop, py::arg("ground"));

  auto panel = record<sketchKit::Panel>(module, "Panel");
  panel.doc() =
      "A region of a sheet: an eyebrow, a title and a note over a well the "
      "content sits in.";
  field(panel, "eyebrow", &sketchKit::Panel::eyebrow);
  field(panel, "title", &sketchKit::Panel::title);
  field(panel, "note", &sketchKit::Panel::note);
  field(panel, "ruled", &sketchKit::Panel::ruled);
  field(panel, "body", &sketchKit::Panel::body);
  module.def("panel", &sketchKit::panel, py::arg("region"), py::arg("content"));

  auto frame = record<sketchKit::Frame>(module, "Frame");
  frame.doc() =
      "A device's chrome round a screen: the shell, its corners and bezel, "
      "the screen's fill and corners, a keyline and the plate under it.";
  field(frame, "width", &sketchKit::Frame::width);
  field(frame, "height", &sketchKit::Frame::height);
  field(frame, "shell", &sketchKit::Frame::shell);
  field(frame, "corners", &sketchKit::Frame::corners);
  field(frame, "bezel", &sketchKit::Frame::bezel);
  field(frame, "screen", &sketchKit::Frame::screen);
  field(frame, "screenCorners", &sketchKit::Frame::screenCorners);
  field(frame, "keyline", &sketchKit::Frame::keyline);
  field(frame, "plate", &sketchKit::Frame::plate);
  module.def("frame",
             py::overload_cast<const sketchKit::Frame&, compose::Element>(
                 &sketchKit::frame),
             py::arg("chrome"), py::arg("screen"));
  module.def("frame",
             py::overload_cast<const sketchKit::Frame&>(&sketchKit::frame),
             py::arg("chrome"));
}

void bindScrolling(py::module_& module) {
  auto thumb = record<sketchKit::Thumb>(module, "Thumb");
  thumb.doc() = "How long a scrollbar's thumb is and how far it travels.";
  field(thumb, "length", &sketchKit::Thumb::length);
  field(thumb, "travel", &sketchKit::Thumb::travel);
  thumb.def(
      "__eq__",
      [](const sketchKit::Thumb& left, const sketchKit::Thumb& right) {
        return left == right;
      },
      py::arg("other"));
  auto scrolled = record<sketchKit::Scrolled>(module, "Scrolled");
  scrolled.doc() =
      "What a scrollbar measures: the view's extent over the content's, "
      "along a track, with the shortest thumb allowed.";
  field(scrolled, "view", &sketchKit::Scrolled::view);
  field(scrolled, "content", &sketchKit::Scrolled::content);
  field(scrolled, "track", &sketchKit::Scrolled::track);
  field(scrolled, "minimumLength", &sketchKit::Scrolled::minimumLength);
  scrolled.def("thumb", &sketchKit::Scrolled::thumb)
      .def(
          "__eq__",
          [](const sketchKit::Scrolled& left,
             const sketchKit::Scrolled& right) { return left == right; },
          py::arg("other"));
  auto bar = record<sketchKit::Scrollbar>(module, "Scrollbar");
  bar.doc() =
      "A scrollbar: its arrows, its thumb sized from what it measures, and "
      "where along its travel the thumb stands — `at` as a fraction, or "
      "`position` as a live or described one.";
  field(bar, "column", &sketchKit::Scrollbar::column);
  field(bar, "leading", &sketchKit::Scrollbar::leading);
  field(bar, "trailing", &sketchKit::Scrollbar::trailing);
  field(bar, "thumb", &sketchKit::Scrollbar::thumb);
  field(bar, "scrolled", &sketchKit::Scrollbar::scrolled);
  field(bar, "thumbLength", &sketchKit::Scrollbar::thumbLength);
  field(bar, "at", &sketchKit::Scrollbar::at);
  field(bar, "position", &sketchKit::Scrollbar::position);
  field(bar, "thumbInset", &sketchKit::Scrollbar::thumbInset);
  field(bar, "track", &sketchKit::Scrollbar::track);
  module.def("scrollbar", &sketchKit::scrollbar, py::arg("bar"));

  auto ticker = record<sketchKit::Ticker>(module, "Ticker");
  ticker.doc() =
      "A strip that runs its content past forever: one pass of the content "
      "and its width, the phase it slides on, the gap between passes and "
      "the window it shows through.";
  field(ticker, "content", &sketchKit::Ticker::content);
  field(ticker, "contentWidth", &sketchKit::Ticker::contentWidth);
  field(ticker, "phase", &sketchKit::Ticker::phase);
  field(ticker, "gap", &sketchKit::Ticker::gap);
  field(ticker, "width", &sketchKit::Ticker::width);
  field(ticker, "height", &sketchKit::Ticker::height);
  module.def("ticker", &sketchKit::ticker, py::arg("strip"));

  auto timeline = record<sketchKit::Timeline>(module, "Timeline");
  timeline.doc() =
      "A rail with its marks: ticks at fractions along it and their words "
      "under or over it.";
  // The mark is nested in the class it marks, as it is in C++, and
  // registered before the timeline's own fields name it.
  using Mark = sketchKit::Timeline::Mark;
  py::class_<Mark> mark(timeline, "Mark");
  mark.def(py::init([](py::kwargs fields) {
        return sigil::python::keywordValue<Mark>(fields,
                                                 "Unknown kit property: ");
      }))
      .def("copy", [](const Mark& value) { return value; });
  sigil::python::copyProtocol(mark);
  mark.doc() =
      "One mark on a timeline: where it stands as a fraction, its word, "
      "whether it is a major tick, and its own ink.";
  field(mark, "at", &Mark::at);
  field(mark, "label", &Mark::label);
  field(mark, "major", &Mark::major);
  field(mark, "ink", &Mark::ink);
  field(timeline, "marks", &sketchKit::Timeline::marks);
  field(timeline, "width", &sketchKit::Timeline::width);
  field(timeline, "height", &sketchKit::Timeline::height);
  field(timeline, "tick", &sketchKit::Timeline::tick);
  field(timeline, "rail", &sketchKit::Timeline::rail);
  field(timeline, "ink", &sketchKit::Timeline::ink);
  field(timeline, "below", &sketchKit::Timeline::below);
  module.def("timeline", &sketchKit::timeline, py::arg("scale"));
}

}  // namespace

void bindSketchKitPanels(py::module_& root) {
  auto module = root.attr("sketch").attr("kit").cast<py::module_>();
  bindHeadings(module);
  bindRegions(module);
  bindScrolling(module);
}

}  // namespace sigil::sketch::python
