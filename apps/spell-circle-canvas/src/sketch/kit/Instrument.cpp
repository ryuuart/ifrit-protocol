#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Document.h>
#include <sigilsketch/kit/Cells.h>
#include <sigilsketch/kit/Heading.h>
#include <sigilsketch/kit/Instrument.h>

#include <utility>

namespace sigil::sketch::kit {

compose::Element instrument(const Instrument& specification,
                            compose::Element picture,
                            compose::Element readings) {
  using namespace compose;
  const Theme& look = theme();
  const float scale =
      specification.pictureSize.width() > 0
          ? specification.pictureWidth / specification.pictureSize.width()
          : 1;
  const float height = specification.pictureSize.height() * scale;
  picture.width(specification.pictureSize.width())
      .height(specification.pictureSize.height())
      .absolute()
      .left(0)
      .top(0)
      .transformOrigin(pct(0), pct(0))
      .scale(scale);
  Element preview =
      box()
          .column()
          .width(specification.pictureWidth)
          .shrink(0)
          .gap(14)
          .children({sectionHeader({.label = specification.pictureLabel}),
                     well({.width = Dimension(specification.pictureWidth),
                           .height = Dimension(height),
                           .corners = look.spacing.panelCorners,
                           .keyline = Fill::color(look.palette.rule),
                           .placed = true},
                          stack().children({std::move(picture)}))});
  if (!specification.note.empty())
    preview.children({document::caption(specification.note)});
  Element detail = box().column().basis(0).grow().gap(14).children(
      {sectionHeader({.label = specification.readingsLabel}),
       well(
           {.padding = 18, .clip = false, .corners = look.spacing.panelCorners},
           std::move(readings))});
  return page(specification.page,
              box()
                  .row()
                  .gap(24)
                  .alignItems(Align::Start)
                  .children({std::move(preview), std::move(detail)}));
}

}  // namespace sigil::sketch::kit
