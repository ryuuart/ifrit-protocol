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

// TAGS: Geometry/Diagrams

#include <include/core/SkSurface.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/style/Type.h>

#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace draw = sigil::draw;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 470};
constexpr float kDiagram = 330;
constexpr float kPicture = 300;
constexpr float kNode = 74;

constexpr const char* kProbe = "hub";  // whose routes are listed

constexpr SkColor4f kWire{0.42f, 0.62f, 0.78f, 1};

/** The sheet's classes are the theme's: the probe plates and the answer
 *  lines are both set in `readout`, which is already the call register in
 *  the figure ink. */
weave::StyleSheet sheetClasses(const sketch::kit::Theme& look) {
  return look.styleSheet();
}

/** THE WORDS THE TWO ENUMS NAME, each run in its enum's own order, so a
 *  refusal mask's bit and a cache state index straight into them. The
 *  refusal MASK is where a word is wanted rather than the sentence a
 *  reason spells, because thirteen sentences would not fit. */
constexpr const char* kPromotionWords[] = {
    "Cheap",    "Warming",    "Promoted",    "AskedFor", "OptedOut",
    "Volatile", "Composited", "Transformed", "Filtered", "ReadsBackdrop",
    "TooBig",   "SplitBaked", "HostsSpace"};
constexpr const char* kStateWords[] = {"Live",     "Picture",  "Texture",
                                       "Promoted", "SplitOwn", "Group"};

}  // namespace

struct RoutesProbe final : sketch::Sketch {
  std::vector<std::string> routes;    // what routesAt() answered
  std::vector<std::string> verdicts;  // one line per probe, from profile()

  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sketch::kit::houseTheme());
    // the readouts are taken before the sheet is built
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // NEITHER QUERY ANSWERS BEFORE THE FRAME IT DESCRIBES HAS BEEN DRAWN,
    // so the diagram is composed once on its own, drawn onto a scratch
    // surface, and asked there — and the sheet then prints the answers
    // beside the same diagram, described again.
    Composer probe(ctx.ticker, *ctx.fonts);
    probe.setSize({kDiagram, kPicture});
    probe.setProfiling(true);
    probe.render(diagram());
    if (sk_sp<SkSurface> scratch = SkSurfaces::Raster(
            SkImageInfo::MakeN32Premul((int)kDiagram, (int)kPicture)))
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
          for (size_t bit = 0; bit < std::size(kPromotionWords); ++bit) {
            if (!row.refused((Composer::Promotion)bit)) continue;
            if (!refused.empty()) refused += ", ";
            refused += kPromotionWords[bit];
          }
          verdicts.push_back(row.label + "  ·  " +
                             kStateWords[(size_t)row.cacheState] + "\n      " +
                             Composer::promotionReason(row.promotion) +
                             "\n      refusals · " +
                             (refused.empty() ? std::string("none") : refused));
          break;
        }

    ctx.composer.render(sheetFor());
  }

  /** One probe node: a small plate whose only job is to earn a verdict. */
  Element probe(const char* key, float x, float y) const {
    const sketch::kit::Theme& sheet = sketch::kit::theme();
    return kit::at(x, y, kNode, 34)
        .key(key)
        .fill(Fill::color(sheet.palette.cellGround))
        .children(
            {text(key).styleClass("readout").absolute().inset(9, 9, 0, 0)});
  }

  Element diagram() const {
    PathFormat wire;
    wire.width = 1.2f;
    wire.strokeFill = Fill::color(kWire);

    Element nodes =
        stack()
            .inset(0)
            // The hub every listed route is anchored on.
            .children(
                {probe("hub", 128, 128).width(80).height(44),
                 // Four probes, each wearing one promotion verdict.
                 probe("spun", 16, 24).rotate(-8),
                 probe("glass", 220, 24).opacity(0.55f),
                 probe("baked", 16, 232).cache(Cache::Texture),
                 probe("live", 220, 232)
                     .cache(Cache::None)
                     .children({pen("routes.live",
                                    [](draw::Pen& pen) {
                                      pen.noStroke();
                                      pen.fill(kWire);
                                      pen.rect(
                                          0, 0,
                                          pen.width *
                                              (float)(0.3 +
                                                      0.0005 * pen.millis()),
                                          3);
                                    })
                                    .absolute()
                                    .inset(0, 26, 0, 0)})});

    // KEYED ROUTES: only a keyed route is addressable, and routesAt lists
    // exactly these three.
    struct Keyed {
      const char* from;
      const char* to;
      Router router;
      const char* key;
    };
    const Keyed keyed[] = {
        {"spun", kProbe, routers::orthogonal(routers::Bend::VFirst, 8),
         "wire-spun"},
        {"glass", kProbe, routers::arc(0.18f), "wire-glass"},
        {kProbe, "baked", routers::straight(), "wire-baked"}};
    Element wires = stack().inset(0).children(
        {each(keyed,
              [&wire](const Keyed& one) {
                return connector(one.from, one.to, one.router)
                    .key(one.key)
                    .inset(0)
                    .foreground(wire);
              }),
         // …and one with no key at all: anchored, drawn, unlistable.
         connector(kProbe, "live", routers::arc(-0.18f))
             .inset(0)
             .foreground(wire)});

    // The diagram is composed on a probe of its own before it stands on
    // the sheet, so its classes are stated on the diagram itself.
    return sketch::kit::well({.width = kDiagram,
                              .height = kPicture,
                              .ground = Fill::color({0.085f, 0.09f, 0.10f, 1})})
        .styleSheet(sheetClasses(sketch::kit::theme()))
        .children({nodes, wires});
  }

  /** A readout: one line per string, in the sheet's own mono. */
  Element lines(const std::vector<std::string>& rows, float measure,
                const char* empty) const {
    if (rows.empty())
      return text(empty)
          .styleClass("readout")
          .ink(sketch::kit::theme().palette.ash)
          .width(measure);
    return box().column().gap(7).children(
        {each(rows, [measure](const std::string& row) {
          return text(row).styleClass("readout").width(measure);
        })});
  }

  Element sheetFor() const {
    constexpr float kList = 260;
    constexpr float kTable = 430;
    return sketch::kit::page(
               {.title = "ROUTES AND COSTS · "
                         "Composer::routesAt, Composer::profile",
                .subtitle = "dials · the probed node (\"hub\") "
                            "· which routes carry a key · "
                            "the property each probe wears: rotate, "
                            "opacity, Cache::None, Cache::Texture",
                .footer = "a profile row's reason names a condition "
                          "under which a bake would produce DIFFERENT "
                          "pixels — which is the one thing "
                          "promotion may never do, and the reason an "
                          "expensive node stays live"},
               kit::cells({.cells = {sketch::kit::caption(
                                         kDiagram,
                                         "connector(from, to, router)"
                                         ".key(…)",
                                         "four routes on one hub · "
                                         "three carry keys and the fourth "
                                         "does not",
                                         diagram()),
                                     sketch::kit::caption(
                                         kList, "composer.routesAt(\"hub\")",
                                         "in tree order · the keyless "
                                         "route is anchored and drawn, and "
                                         "not in this list",
                                         lines(routes, kList,
                                               "— nothing yet: the "
                                               "first describe has not been "
                                               "drawn")),
                                     sketch::kit::caption(
                                         kTable,
                                         "composer.profile() → "
                                         "label · cacheState · "
                                         "promotionReason",
                                         "each probe looked up by its own key "
                                         "· the milliseconds are on "
                                         "these same rows and are not printed, "
                                         "because a plate that carries a "
                                         "timing differs from itself",
                                         lines(verdicts, kTable,
                                               "— empty until a frame "
                                               "has been drawn with profiling "
                                               "on"))},
                           .gap = 18}))
        .styleSheet(sheetClasses(sketch::kit::theme()));
  }
};

SIGIL_SKETCH(RoutesProbe, "Kit · API",
             "the edge store's back-index for one hub, and the profile row "
             "each of four bake refusals produces, both read off the "
             "composer after the frame they describe")
