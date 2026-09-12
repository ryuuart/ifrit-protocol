#include <sigilcompose/core/Factories.h>
#include <sigilsketch/kit/Scrollbar.h>

#include <algorithm>
#include <utility>

namespace sigil::sketch::kit {

using compose::box;
using compose::Dimension;
using compose::Element;
using compose::Fill;

Thumb Scrolled::thumb() const {
  if (track <= 0) return {};
  // Everything showing fills the track, which is the one reading a
  // division cannot give: a content of nothing has no share to take.
  if (content <= view || content <= 0) return {track, 0};
  const float share = track * view / content;
  const float length = std::clamp(share, std::min(minLength, track), track);
  return {length, track - length};
}

compose::Element scrollbar(Scrollbar bar) {
  const Theme& look = theme();
  const bool down = bar.column;

  Element shell = box();
  if (down)
    shell.column();
  else
    shell.row();
  if (bar.leading) shell.child(std::move(*bar.leading));

  Element rail = box().grow(1);
  bar.track.value_or(Fill::color(look.palette.cellGround)).apply(rail);

  const Thumb reading = bar.scrolled.thumb();
  Dimension length = bar.thumbLength;
  if (length.unit == Dimension::Unit::Auto && reading.length > 0)
    length = Dimension(reading.length);
  if (length.unit != Dimension::Unit::Auto) {
    Element slider = bar.thumb ? std::move(*bar.thumb)
                               : box().fill(Fill::color(look.palette.figure));
    // The bar owns the whole of the thumb's geometry: its length, its
    // place along the track, and how far it stands in from the two long
    // edges. What the caller handed over is the chrome.
    slider.absolute();
    const Dimension offset =
        Dimension(reading.travel * std::clamp(bar.at, 0.0f, 1.0f));
    if (down) {
      slider.left(Dimension(bar.thumbInset)).right(Dimension(bar.thumbInset));
      slider.top(offset).height(length);
      if (bar.position) slider.translateY(*bar.position);
    } else {
      slider.top(Dimension(bar.thumbInset)).bottom(Dimension(bar.thumbInset));
      slider.left(offset).width(length);
      if (bar.position) slider.translateX(*bar.position);
    }
    rail.child(std::move(slider));
  }

  shell.child(std::move(rail));
  if (bar.trailing) shell.child(std::move(*bar.trailing));
  return shell;
}

}  // namespace sigil::sketch::kit
