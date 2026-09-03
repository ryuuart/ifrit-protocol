/** @file
 * routes_probe — the two questions a composer answers about the tree it
 * has already drawn.
 *
 * `routesAt(nodeKey)` is the edge store's BACK-INDEX: the keys of every
 * `connector()` and `rail()` anchored on that node, in tree order. It is
 * the graph query — which edges touch this node — that a hover highlight
 * or a pruned update needs, and it exists because the routes were
 * resolved against layout geometry the author never held. Keyless routes
 * are anchored but unaddressable, so they are simply absent: give a
 * route a key to see it here.
 *
 * `profile()` is the per-node companion to `stats()`. Where the stats say
 * the tree is re-rasterising, a profile row says WHICH node is doing it
 * and, through `promotionReason`, what refused it a bake. Every refusal
 * names a condition under which a bake would produce different pixels,
 * which is the one thing promotion may never do. The five probes here
 * each wear one such property, and both the reason and the refusal mask
 * are read off the composer rather than written down: the verdict is a
 * FIRST MATCH, so a cheap node reports `Cheap` however many other
 * conditions also refuse it, and the mask under it is all of them.
 *
 * Neither query answers before the frame it describes has been drawn, so
 * the diagram is composed on a composer of its own, drawn once onto a
 * scratch surface, and asked there; the sheet prints what came back
 * beside the same diagram described again.
 *
 * EDIT THESE FIRST
 *   kProbe — the node whose routes are listed.
 *   The five probe nodes' verdicts: rotate, opacity, Cache::None and
 *   Cache::Texture are what earn four of the five reasons.
 */

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 470};
constexpr float kDiagram = 330;
constexpr float kPicture = 300;
constexpr float kNode = 74;

constexpr const char* kProbe = "hub";  // whose routes are listed

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};
constexpr SkColor4f kWire{0.42f, 0.62f, 0.78f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

kit::Caption voice(float measure) {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = measure};
}

/** A promotion outcome as the one word the enum names — for the refusal
 *  MASK, where the sentence a reason spells would not fit. */
const char* promotionWord(Composer::Promotion p) {
  switch (p) {
    case Composer::Promotion::Cheap: return "Cheap";
    case Composer::Promotion::Warming: return "Warming";
    case Composer::Promotion::Promoted: return "Promoted";
    case Composer::Promotion::AskedFor: return "AskedFor";
    case Composer::Promotion::OptedOut: return "OptedOut";
    case Composer::Promotion::Volatile: return "Volatile";
    case Composer::Promotion::Composited: return "Composited";
    case Composer::Promotion::Transformed: return "Transformed";
    case Composer::Promotion::Filtered: return "Filtered";
    case Composer::Promotion::ReadsBackdrop: return "ReadsBackdrop";
    case Composer::Promotion::TooBig: return "TooBig";
    case Composer::Promotion::SplitBaked: return "SplitBaked";
    case Composer::Promotion::HostsSpace: return "HostsSpace";
  }
  return "";
}

/** How a node produced its pixels, as the one word the enum names. */
const char* stateWord(Composer::CacheState state) {
  switch (state) {
    case Composer::CacheState::Live:
      return "Live";
    case Composer::CacheState::Picture:
      return "Picture";
    case Composer::CacheState::Texture:
      return "Texture";
    case Composer::CacheState::Promoted:
      return "Promoted";
    case Composer::CacheState::SplitOwn:
      return "SplitOwn";
    case Composer::CacheState::Group:
      return "Group";
  }
  return "";
}

}  // namespace

struct RoutesProbe final : sketch::Sketch {
  std::vector<std::string> routes;    // what routesAt() answered
  std::vector<std::string> verdicts;  // one line per probe, from profile()

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // the readouts are taken before the sheet is built

    // NEITHER QUERY ANSWERS BEFORE THE FRAME IT DESCRIBES HAS BEEN DRAWN,
    // so the diagram is composed once on its own, drawn onto a scratch
    // surface, and asked there — and the sheet then prints the answers
    // beside the same diagram, described again.
    Composer probe(ctx.ticker, *ctx.fonts);
    probe.setSize({kDiagram, kPicture});
    probe.setProfiling(true);
    probe.render(diagram());
    if (sk_sp<SkSurface> scratch = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
            (int)kDiagram, (int)kPicture)))
      probe.draw(*scratch->getCanvas());

    routes = probe.routesAt(kProbe);
    for (const char* key : {"hub", "spun", "glass", "live", "baked"})
      for (const Composer::NodeCost& row : probe.profile())
        // A row's label is the node's key() and the kind and size that
        // make it actionable, so the key is a prefix of it.
        if (row.label.starts_with(std::string(key) + " (")) {
          // The verdict is FIRST MATCH, so a cheap node reports Cheap
          // however many other conditions also refuse it; the mask carries
          // every one of them, which is the line under the reason.
          std::string refused;
          for (int bit = 0; bit < 13; ++bit) {
            const auto p = (Composer::Promotion)bit;
            if (!row.refused(p)) continue;
            if (!refused.empty()) refused += ", ";
            refused += promotionWord(p);
          }
          verdicts.push_back(
              row.label + "  \xc2\xb7  " + stateWord(row.cacheState) +
              "\n      " + Composer::promotionReason(row.promotion) +
              "\n      refusals \xc2\xb7 " +
              (refused.empty() ? std::string("none") : refused));
          break;
        }

    ctx.composer.render(sheetFor());
  }

  /** One probe node: a small plate whose only job is to earn a verdict. */
  Element probe(const char* key, float x, float y) const {
    return box()
        .key(key)
        .inset(Dim(x), Dim(y), Dim(), Dim())
        .width(Dim(kNode))
        .height(Dim(34))
        .fill(Fill::color(kCellGround))
        .child(text(toU8(key), mono(10, kFigure))
                   .absolute()
                   .inset(9, 9, 0, 0));
  }

  Element diagram() const {
    PathFormat wire;
    wire.width = 1.2f;
    wire.strokeFill = Fill::color(kWire);

    Element nodes =
        stack()
            .inset(0)
            // The hub every listed route is anchored on.
            .child(probe("hub", 128, 128).width(Dim(80)).height(Dim(44)))
            // Four probes, each wearing one promotion verdict.
            .child(probe("spun", 16, 24).rotate(-8))
            .child(probe("glass", 220, 24).opacity(0.55f))
            .child(probe("baked", 16, 232).cache(Cache::Texture))
            .child(probe("live", 220, 232)
                       .cache(Cache::None)
                       .child(custom("routes.live",
                                     [](SkCanvas& canvas,
                                        const PaintContext& pc) {
                                       SkPaint paint;
                                       paint.setColor4f(kWire);
                                       canvas.drawRect(
                                           {0, 0, pc.size.width() *
                                                      (float)(0.3 + 0.5 *
                                                              pc.elapsedSeconds),
                                            3},
                                           paint);
                                     })
                                 .absolute()
                                 .inset(0, 26, 0, 0)));

    // Keyed routes: only a keyed route is addressable, and routesAt lists
    // exactly these.
    Element wires =
        stack()
            .inset(0)
            .child(connector("spun", kProbe, routers::orthogonal(
                                                 routers::Bend::VFirst, 8))
                       .key("wire-spun")
                       .inset(0)
                       .foreground(wire))
            .child(connector("glass", kProbe, routers::arc(0.18f))
                       .key("wire-glass")
                       .inset(0)
                       .foreground(wire))
            .child(connector(kProbe, "baked", routers::straight())
                       .key("wire-baked")
                       .inset(0)
                       .foreground(wire))
            // …and one with no key at all: anchored, drawn, unlistable.
            .child(connector(kProbe, "live", routers::arc(-0.18f))
                       .inset(0)
                       .foreground(wire));

    return box()
        .width(Dim(kDiagram))
        .height(Dim(kPicture))
        .clip()
        .fill(Fill::color({0.085f, 0.09f, 0.10f, 1}))
        .child(nodes)
        .child(wires);
  }

  /** A readout: one line per string, in the sheet's own mono. */
  Element lines(const std::vector<std::string>& rows, float measure,
                const char* empty) const {
    Element column = box().column().gap(7);
    if (rows.empty())
      column.child(text(toU8(empty), mono(10, kAsh)).width(Dim(measure)));
    for (const std::string& row : rows)
      column.child(text(toU8(row), mono(10, kFigure)).width(Dim(measure)));
    return column;
  }

  Element sheetFor() const {
    constexpr float kList = 260;
    constexpr float kTable = 430;
    return kit::sheet(
               {.title = toU8("ROUTES AND COSTS \xc2\xb7 "
                              "Composer::routesAt, Composer::profile"),
                .subtitle = toU8("dials \xc2\xb7 the probed node (\"hub\") "
                                 "\xc2\xb7 which routes carry a key \xc2\xb7 "
                                 "the property each probe wears: rotate, "
                                 "opacity, Cache::None, Cache::Texture"),
                .footer = toU8("a profile row's reason names a condition "
                               "under which a bake would produce DIFFERENT "
                               "pixels \xe2\x80\x94 which is the one thing "
                               "promotion may never do, and the reason an "
                               "expensive node stays live"),
                .titleStyle = label(14, kInk, 2.4f),
                .subtitleStyle = label(11.5f, kAsh, 0.8f),
                .footerStyle = label(11, kAsh, 0.4f),
                .marginX = 24,
                .marginTop = 20,
                .marginBottom = 16,
                .ground = Fill::color(kGround),
                .rule = Fill::color(kRule)},
               kit::cells(
                   {.cells =
                        {kit::cell(voice(kDiagram),
                                   toU8("connector(from, to, router)"
                                        ".key(\xe2\x80\xa6)"),
                                   toU8("four routes on one hub \xc2\xb7 "
                                        "three carry keys and the fourth "
                                        "does not"),
                                   diagram()),
                         kit::cell(voice(kList),
                                   toU8("composer.routesAt(\"hub\")"),
                                   toU8("in tree order \xc2\xb7 the keyless "
                                        "route is anchored and drawn, and "
                                        "not in this list"),
                                   lines(routes, kList,
                                         "\xe2\x80\x94 nothing yet: the "
                                         "first describe has not been "
                                         "drawn")),
                         kit::cell(voice(kTable),
                                   toU8("composer.profile() \xe2\x86\x92 "
                                        "label \xc2\xb7 cacheState \xc2\xb7 "
                                        "promotionReason"),
                                   toU8("each probe looked up by its own key "
                                        "\xc2\xb7 the milliseconds are on "
                                        "these same rows and are not printed, "
                                        "because a plate that carries a "
                                        "timing differs from itself"),
                                   lines(verdicts, kTable,
                                         "\xe2\x80\x94 empty until a frame "
                                         "has been drawn with profiling "
                                         "on"))},
                    .gap = 18}))
        .absolute()
        .inset(0);
  }
};

SIGIL_SKETCH(RoutesProbe, "Kit \xc2\xb7 API",
             "the edge store's back-index for one hub, and the profile row "
             "each of four bake refusals produces, both read off the "
             "composer after the frame they describe")
