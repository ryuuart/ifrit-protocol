/** @file
 * kumiko_asanoha — an asanoha ranma, the transom lattice over a pair of
 * sliding doors, assembled board by board against the far room's lamp,
 * with the shop drawing of its one cell under it.
 *
 * A RANMA IS SEEN AGAINST THE LIGHT. The lattice stands in the kamoi's
 * opening with washi behind it and the next room's andon behind that, so
 * the paper is a lit field that breathes with the flame and the fretwork
 * reads by its silhouette and by the one raking light the room gives its
 * arrises.
 */
// TAGS: Patterns/Tiling

#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Time.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>

#include <algorithm>
#include <initializer_list>

#include "Joinery.h"
#include "ShopDrawing.h"

namespace sketch = sigil::sketch;
using namespace kumiko;

namespace {

const material::Color kNight = hexColor(0x0D0906);

}  // namespace

struct KumikoAsanoha {
  Panel panel;
  TimberBank bank;
  /** The shop drawing's words, and the figures its sentences quote from
   *  the panel as built. */
  sketch::kit::Document doc;
  /** The washi's two faces, held so each keeps one identity across
   *  describes: its FORMATION, the cloud of thick and thin the sheet was
   *  couched with, and its FIBRE, the long kozo strands laid in it. */
  material::Paint formation, fibre;
  /** Seconds into one assembly, wrapping: the one value that moves, and
   *  every entrance, the lamp and the drawing's beat are bindings over it. */
  motion::Animatable<float> seconds = motion::animatable(0.0f);

  /** The lamp's beat: 0 until it is lit, easing to 1. */
  motion::Bound lit() const {
    return motion::bind(seconds, {.from = {kLampAt, kLampAt + kLampFor}, .clampFrom = true, .ease = motion::ease::outCubic});
  }

  /** The flame once it is lit: up on the lamp's beat, then breathing on a
   *  slow two-octave noise for the rest of the loop, so everything it
   *  lights glows brighter and dimmer with it rather than strobing. */
  motion::Bound breath() const {
    return motion::bind(seconds, {.from = {kLampAt, kPeriod}, .clampFrom = true, .envelope = motion::envelope::trapezoid(0, kLampFor / (kPeriod - kLampAt), 1, 1), .ease = motion::ease::outCubic, .to = {0.0f, 0.75f}, .wiggle = {.amount = 0.25f, .frequency = 4.5f, .seed = 11, .octaves = 2, .falloff = 0.45f}, .clamp = {0, 1}});
  }

  /** The pieces of @p roles, in that order, each entering on its beat. */
  Element pieces(std::initializer_list<Role> roles) {
    Element group = box().inset(0);
    for (const Role role : roles)
      for (const Piece& piece : panel.pieces)
        if (piece.role == role)
          group.children({pieceElement(piece, bank, seconds)});
    return group;
  }

  /** THE LATTICE, in paint order: ha under jigumi, since a butt joint
   *  reads as the ha stopping at the jigumi's face, and jigumi under the
   *  register. Every piece carries a bound opacity and scale for its
   *  entrance, so no cache above them can hold the panel whole until the
   *  last board has landed; a group cache composites the settled lattice
   *  once and holds it while every binding beneath still reads what it
   *  read last frame, and the entrance is paid a board at a time by each
   *  piece's own bake. A texture cache on this box would bake nothing,
   *  since a container with no fill has no paint of its own. */
  Element lattice() {
    return pieces({Role::Diagonal, Role::Filler, Role::Lock,
                   Role::HorizontalJigumi, Role::VerticalJigumi,
                   Role::Register})
        .cache(Cache::Group);
  }

  /** The joint pass: every half-lap seam and every tenon head arrives on
   *  one beat, the craftsman's seating tap. */
  Element joinery() {
    Element group = box().inset(0).opacity(
        motion::bind(seconds, {.from = {kSeatAt, kSeatAt + kSeatFor}, .clampFrom = true, .ease = motion::ease::outCubic}));
    group.children(
        {pathFigure(panel.lapShadows, 2)
             .stroke(stroke(1.5f, Fill::color(kLapShadowInk))),
         pathFigure(panel.lapHighlights, 2)
             .stroke(stroke(0.7f, Fill::color(kLapArrisInk)))});
    for (const Piece& tenon : panel.tenons)
      group.children({pieceElement(tenon, bank, nullptr)});
    return group;
  }

  /** THE LIGHT BEHIND THE LATTICE. A shoji diffuses: paper over a lamp is
   *  a lit field, not a point source seen through a hole, so the ramp
   *  falls a third of a stop from the middle of the opening to its
   *  corners and the pattern — the whole subject — reads everywhere
   *  across it. The andon stands on the far room's floor, so its hot core
   *  sits low in the opening, added over the ramp. The paper is laid over
   *  both and MULTIPLIES them: washi seen against a lamp is read by what
   *  it holds back, so its formation shows as clouds of thick and thin and
   *  its kozo as long dark strands, strongest where the light behind is
   *  strongest — the core is where a reader sees the fibre.
   *
   *  The field is painted from the first frame and the lamp's rise is a
   *  veil of night lifting off it, so every bake under it is taken while
   *  the panel is still being assembled rather than on the frame the lamp
   *  comes up. Every layer is a bake of its own; the blend each one adds
   *  or multiplies with rides that bake's blit. */
  Element backlight() {
    const float middleX = kOpening.width() * 0.5f,
                middleY = kOpening.height() * 0.5f;
    // The paper is cut larger than the opening so a turned sheet still
    // covers every corner of it.
    const auto sheet = [&](const material::Paint& face, float degrees,
                           float opacity) {
      return kit::at(-160, -260, kOpening.width() + 320,
                     kOpening.height() + 520)
          .rotate(degrees)
          .opacity(opacity)
          .blendMode(material::BlendMode::Multiply)
          .fill(face)
          .cache(Cache::Texture);
    };
    return box()
        .rect(kOpening)
        .overflow(Overflow::Clip)
        .children({
            box()
                .cover()
                .fill(material::Paint::radialGradient(
                    {middleX, middleY}, 585,
                    {{0.00f, hexColor(0xF7E8C6, 0.88f)},
                     {0.30f, hexColor(0xF2E0B4, 0.85f)},
                     {0.58f, hexColor(0xE6CE9A, 0.79f)},
                     {0.80f, hexColor(0xD3B37C, 0.71f)},
                     {1.00f, hexColor(0xBE9862, 0.66f)}},
                    {.units = material::GradientUnits::Pixels}))
                .cache(Cache::Texture),
            box()
                .cover()
                .blendMode(material::BlendMode::PlusLighter)
                .fill(material::Paint::radialGradient(
                    {middleX, kOpening.height() * 0.64f}, 330,
                    {{0.00f, hexColor(0xFFE6B8, 0.42f)},
                     {0.35f, hexColor(0xF3C98A, 0.22f)},
                     {0.70f, hexColor(0xD9A560, 0.07f)},
                     {1.00f, hexColor(0x000000, 0.00f)}},
                    {.units = material::GradientUnits::Pixels}))
                .cache(Cache::Texture),
            sheet(formation, 0, 0.65f),
            sheet(fibre, 21, 0.38f),
            sheet(fibre, -48, 0.26f),
        });
  }

  /** The paper's halo round the opening, where the lit field bleeds past
   *  its edge onto the frame's shadow. It stands apart from the paper
   *  because a bake that held the paper's layers would resolve their
   *  multiply and add against its own transparent ground rather than the
   *  light under them. */
  Element halo() {
    return box()
        .rect(kOpening)
        .background(styles::OuterGlow{hexColor(0xF4E3B8, 0.34f), 70, 6})
        .cache(Cache::Texture);
  }

  /** Night over the paper until the lamp is lit. */
  Element veil() {
    return box()
        .rect(kOpening.makeOutset(90, 90))
        .fill(Fill::color(kNight))
        .opacity(lit().target(1, 0));
  }

  /** THE ANDON'S FLAME: the lamp's halo added OVER the fretwork, so the
   *  light visibly wraps the pieces it stands behind instead of stopping
   *  dead at their silhouettes, while the paper under it holds still. */
  Element flame() {
    return box()
        .rect(kOpening)
        .opacity(breath())
        .blendMode(material::BlendMode::PlusLighter)
        .fill(material::Paint::radialGradient(
            {kOpening.width() * 0.5f, kOpening.height() * 0.5f}, 380,
            {{0.00f, hexColor(0xFFF2D2, 0.17f)},
             {0.45f, hexColor(0xE6BC7C, 0.09f)},
             {1.00f, hexColor(0x000000, 0.00f)}},
            {.units = material::GradientUnits::Pixels}))
        .cache(Cache::Texture);
  }

  /** THE LAMP'S SPILL on the kamoi: what the lit paper throws into the
   *  near room falls warmest on the face of the beam just under the
   *  opening and fades along it and down it, breathing with the flame. */
  Element spill() {
    const float top = kRoom - 122;
    return kit::at(0, top, kWidth, 122)
        .opacity(breath())
        .blendMode(material::BlendMode::PlusLighter)
        .fill(material::Paint::radialGradient(
            {kCentre.x, kOpening.bottom() - top - 60}, 560,
            {{0.00f, hexColor(0xF6D9A2, 0.55f)},
             {0.22f, hexColor(0xE8BD7A, 0.32f)},
             {0.50f, hexColor(0xC98E4E, 0.10f)},
             {1.00f, hexColor(0x000000, 0.00f)}},
            {.units = material::GradientUnits::Pixels}))
        .cache(Cache::Texture);
  }

  /** The mitred keyline draws itself on around the frame's opening — one
   *  continuous reveal, the first beat of the assembly. */
  Element keyline() {
    return box()
        .rect(kOpening.makeOutset(1.5f, 1.5f))
        .stroke(spans::upTo(motion::bind(seconds, {.from = {kFrameAt, kFrameAt + kFrameFor + 0.35f}, .clampFrom = true, .ease = motion::ease::outCubic})),
                PathFormat{.width = 2.2f,
                           .strokeFill = Fill::color(hexColor(0xC79A57, 0.60f)),
                           .align = PathFormat::Align::Center});
  }

  /** A room-side member: the nageshi over the opening or the kamoi under
   *  it, in keyaki two stops down, its shadow falling away from the
   *  opening. */
  Element beam(float top, float height, bool over) {
    return kit::at(0, top, kWidth, height)
        .fill(bank.get(kKeyakiShade, height, !over, 7))
        .foreground(
            styles::InnerShadow{{0, 0, 0, 0.65f}, {0, over ? 8.f : -8.f}, 18})
        .foreground(styles::BevelEmboss{2.5f,
                                        4,
                                        over ? 300.0f : 120.0f,
                                        {1, 0.88f, 0.68f, 0.16f},
                                        {0, 0, 0, 0.60f}});
  }

  /** A post at @p left, its grain running down it. */
  Element post(float left, float width) {
    const bool right = left > kCentre.x;
    return kit::at(left, 118, width, kRoom - 236)
        .fill(bank.get(kKeyakiShade, width, right, 3, /*along=*/true))
        .foreground(
            styles::InnerShadow{{0, 0, 0, 0.60f}, {right ? -7.f : 7.f, 0}, 16});
  }

  Element describe() {
    return stack()
        .fill(Fill::color(kNight))
        .children({halo(), backlight(), veil(), lattice(), joinery(), flame(),
                   pieces({Role::Frame}).cache(Cache::Group), keyline(),
                   post(0, 146), post(kWidth - 146, 146), beam(0, 122, true),
                   beam(kRoom - 122, 122, false), spill(),
                   // The near side of the room, in shadow. It stops at the
                   // room's floor: the drawing under it is a drawing.
                   kit::at(0, 0, kWidth, kRoom)
                       .fill(material::Paint::radialGradient(
                           {700, 500}, 920,
                           {{0.30f, {0, 0, 0, 0}},
                            {0.72f, {0, 0, 0, 0.30f}},
                            {1.0f, {0, 0, 0, 0.62f}}},
                           {.units = material::GradientUnits::Pixels})),
                   shopDrawing(doc, bank, seconds)});
  }

  void setup(sketch::SketchContext& ctx) {
    // The still is taken mid-hold: the panel is complete and lit from the
    // lamp's beat onwards and the loop tears down at its period, so a
    // moment well clear of both cannot catch the plate half-built.
    sketch::kit::stage(ctx, {.size = SkSize::Make(kWidth, kHeight),
                             .captureAt = 4.2,
                             .background = kNight});
    panel = Panel{};
    panel.build();
    doc = sketch::kit::Document(ctx, "data/content.json");
    // The count the drawing quotes is the generator's own, not a product
    // typed beside it.
    const auto leaves = std::count_if(
        panel.pieces.begin(), panel.pieces.end(), [](const Piece& piece) {
          return piece.role == Role::Diagonal || piece.role == Role::Filler ||
                 piece.role == Role::Lock;
        });
    doc.figures({{"incircle", kit::formatted("%.5f", kIncircle)},
                 {"cells", kit::formatted("%d", kColumns * kRows)},
                 {"leaves", kit::formatted("%ld", (long)leaves)}});
    // Both faces are near-white boards, since a multiplied white is the
    // light let through untouched and only what the paper holds back
    // darkens it, and both lean warm because kozo does, so a thick place
    // reads amber rather than grey: the formation all wear and no tooth, the fibre all
    // tooth drawn out into strands and no wear.
    formation = material::Paint::recipe(
        material::kit::board({.paint = hexColor(0xFFEFD8),
                              .tooth = 0.0f,
                              .wear = 0.42f,
                              .wearScale = 0.011f,
                              .seed = 5}));
    fibre = material::Paint::recipe(
        material::kit::board({.paint = hexColor(0xFFF3E4),
                              .tooth = 0.4f,
                              .toothScale = 0.06f,
                              .stretch = 4.0f,
                              .wear = 0.0f,
                              .seed = 9}));
    ctx.engine.add([this](double, double elapsed) {
      seconds = motion::phase(elapsed, kPeriod) * kPeriod;
    });
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(KumikoAsanoha, "Study · Pattern",
             "A hinoki asanoha ranma — 514 mitred boards, per-piece "
             "assembly staggering against a breathing andon, beside the "
             "shop drawing of its one cell")
