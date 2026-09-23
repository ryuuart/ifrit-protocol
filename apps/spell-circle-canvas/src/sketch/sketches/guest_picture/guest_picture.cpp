/** @file
 * guest_picture — a page whose body is ANOTHER APPLICATION'S PICTURE.
 *
 * Everything else in this directory draws what it computes. This one
 * draws what somebody else computed: a publication on this machine — a
 * second Sketchbook, a VJ program, a projection mapper, anything that
 * offers its frames under a name — worn as the body of a page, in a
 * rounded frame with the publication and the application it is drawn in
 * captioned under it.
 *
 * THE FRAME NEVER LEAVES THE GPU. What arrives is the texture the other
 * application drew into, carried with its first row at the image's
 * bottom; the guest draws it once into a target of its own on the
 * recorder this canvas is being drawn on, the right way up, and that is
 * the image sampled where it stands. It is turned over once for each
 * frame that arrives and never read back to the CPU. The guest is asked
 * for one every frame, which is also what opens onto a publisher that
 * appeared after this sketch was set up, or came back after one stopped.
 *
 * IT KEEPS THE GUEST'S OWN SHAPE. The picture is fitted into the frame
 * rather than filled out to it, so what is on the page is the whole of
 * what the other application drew and a publication of any proportion is
 * shown whole.
 *
 * NOTHING ARRIVES IN A CAPTURE, and the waiting card is the honest plate:
 * what another application happens to be publishing while a still is
 * taken is not a function of this file, so a guest opened for a capture
 * subscribes to nothing at all. The picture is the window's.
 *
 * SEE IT MOVE. From the build directory, one Sketchbook publishing and
 * one wearing it:
 *
 *     build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
 *         --sketch feed_vitals --publish Guest
 *     build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
 *         --sketch guest_picture
 *
 * `Seer --list-textures` says what is being offered under what name, which is
 * what to check when the card keeps waiting.
 *
 * EDIT THESE FIRST
 *   kPublication  the name this page waits on
 *   kApplication  the one application it will take it from; empty is any
 *   kFrame        how big the body is
 */

// TAGS: Media/Video, Data/Sources

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Guest.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Cells.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace compose = sigil::compose;

namespace {

/** The name a second Sketchbook publishes under with `--publish Guest`,
 *  and the application it may be taken from — empty takes whichever
 *  application answers to the name. */
const char* kPublication = "Guest";
const char* kApplication = "";

constexpr SkSize kCanvas = {1280, 900};
constexpr material::Color kGround = {0.05f, 0.052f, 0.06f, 1};
/** The body: the page's whole content width — the canvas less the
 *  theme's two side margins — at 16:9. */
constexpr SkSize kFrame = {1232, 693};
constexpr float kCorner = 18.0f;    // how round the body is
constexpr double kCaptureAt = 0.0;  // a still of this page is the card

/** @p picture at its own proportion, centred in a box @p into across —
 *  the whole of the guest's frame, letterboxed where its shape is not
 *  this page's. */
SkRect fitted(const SkImage& picture, SkSize into) {
  if (picture.height() <= 0 || into.isEmpty()) return SkRect::MakeSize(into);
  const float shape = (float)picture.width() / (float)picture.height();
  const float room = into.width() / into.height();
  const float width = shape > room ? into.width() : into.height() * shape;
  const float height = shape > room ? into.width() / shape : into.height();
  return SkRect::MakeXYWH((into.width() - width) * 0.5f,
                          (into.height() - height) * 0.5f, width, height);
}

struct GuestPicture {
  /** The publication, held by its name: made once per reload and kept by
   *  the paint program that reads it, which is why it is shared. */
  std::shared_ptr<sketch::Guest> guest;
  /** What the description standing now was written from. */
  bool showing = false;
  std::string drawnIn;

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(
        ctx, {.size = kCanvas, .captureAt = kCaptureAt, .background = kGround});
    guest = std::make_shared<sketch::Guest>(ctx, kPublication, kApplication);
    read();
    describe(ctx);
  }

  /** The data path: a publisher that arrived or went away is a different
   *  caption and a different card, and nothing else on the page moves by
   *  being described again. The picture itself is never re-described —
   *  it is one leaf drawing whatever the newest frame is. */
  void update(double, sketch::SketchContext& ctx) {
    if (read()) describe(ctx);
  }

  /** Answers whether what the description says about the publication has
   *  changed. */
  bool read() {
    const bool standing = guest->publishing();
    std::string application(guest->application());
    if (standing == showing && application == drawnIn) return false;
    showing = standing;
    drawnIn = std::move(application);
    return true;
  }

  void describe(sketch::SketchContext& ctx) {
    std::vector<compose::Element> body;
    body.push_back(picture());
    if (!showing) body.push_back(waiting(ctx.deterministic));
    ctx.composer.render(sketch::kit::page(
        {.title = "GUEST",
         .subtitle = "another application's frames, worn as a body",
         .footer = "the texture arrives on this machine and is sampled "
                   "where it stands"},
        sketch::kit::caption(
            kFrame.width(), label(), note(),
            sketch::kit::well({.width = kFrame.width(),
                               .height = kFrame.height(),
                               .padding = 0,
                               .corners = kCorner,
                               .keyline = compose::Fill::color(
                                   sketch::kit::theme().palette.rule),
                               .placed = true},
                              compose::stack().children(std::move(body))))));
  }

  /** THE PICTURE, and the ask that keeps the subscription open. It runs
   *  every frame — a published frame is new every frame, so there is
   *  nothing here to cache — and draws nothing at all while nothing is
   *  publishing, which is what the card over it explains. */
  compose::Element picture() {
    return compose::custom(
               "guest.picture",
               [held = guest](SkCanvas& canvas,
                              const compose::PaintContext& paint) {
                 const sk_sp<SkImage> frame = held->frame(canvas.recorder());
                 if (!frame) return;
                 SkPaint how;
                 how.setBlendMode(SkBlendMode::kSrcOver);
                 canvas.drawImageRect(frame, fitted(*frame, paint.size),
                                      SkSamplingOptions(SkFilterMode::kLinear),
                                      &how);
               })
        .cover()
        .cache(compose::Cache::None);
  }

  /** WHAT THE PAGE IS WAITING FOR, where the picture would be: the name
   *  nothing is publishing under, and the one line that would start
   *  somebody publishing under it. */
  compose::Element waiting(bool deterministic) {
    const sketch::kit::Theme& look = sketch::kit::theme();
    return compose::kit::centred()
        .cover()
        .column()
        .gap(look.spacing.contentGap)
        .children(
            {compose::document::h2(
                 compose::kit::formatted("WAITING FOR “%s”", kPublication))
                 .font(look.font({.size = 26, .track = 6}))
                 .ink(look.palette.ink),
             compose::document::paragraph(
                 deterministic ? "Live publications are shown in the window; "
                                 "captures keep this waiting card."
                               : "No publisher is connected under that name.")
                 .font(look.font({.size = 14}))
                 .ink(look.palette.ash),
             compose::document::code(
                 compose::kit::formatted(
                     "Sketchbook --sketch feed_vitals --publish %s",
                     kPublication))
                 .font(look.font({.size = 13, .mono = true}))
                 .ink(look.palette.ash)});
  }

  /** The publication, over the body. */
  std::string label() const { return kPublication; }

  /** The application drawing it, under the body — or what there is to
   *  say instead while nobody is. */
  std::string note() const {
    if (!showing) return "waiting for a publisher";
    return drawnIn.empty()
               ? "published by an application that did not name itself"
               : compose::kit::formatted("drawn in %s", drawnIn.c_str());
  }
};

}  // namespace

SIGIL_SKETCH(GuestPicture, "Media",
             "Another application's published frames worn as the body of a "
             "page: the texture wrapped where it stands, the publication "
             "and its application captioned, and the waiting card while "
             "nobody is offering one.")
