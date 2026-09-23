/** @file
 * Connectivity and cache decisions read from a composed diagram. A wire is
 * keyed by the pair it joins, so connectivity is a question about keys:
 * bounds() answers where each wire came to rest, and answers nothing for a
 * pairing no operator drew. profile() reports the cache state, first
 * promotion reason and complete refusal mask for each selected node. The
 * queries run on a separate composer after its first draw; the displayed
 * timings are deliberately omitted from plates.
 */

// TAGS: Geometry/Diagrams

#include <include/core/SkSurface.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Connect.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace draw = sigil::draw;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 760};
constexpr float kDiagram = 330;
constexpr float kPicture = 300;
constexpr float kNode = 74;

constexpr const char* kProbe = "hub";  // whose wires are listed

/** The pairings the readout asks about, the last of them one no operator
 *  draws — the silence an unrouted pair answers with. */
constexpr const char* kPairings[] = {"spun->hub", "glass->hub", "hub->baked",
                                     "hub->live", "hub->nowhere"};

constexpr material::Color kWire{0.42f, 0.62f, 0.78f, 1};

/** The sheet's classes are the theme's: the probe plates and the answer
 *  lines are both set in `readout`, which is already the call register in
 *  the figure ink. */
sigil::compose::StyleSheet sheetClasses(const sketch::kit::Theme& look) {
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

struct RoutesProbe {
  std::vector<std::string> routes;  // the pairings that answered
  struct Verdict {
    std::string key, state, reason, refusals;
  };
  std::vector<Verdict> verdicts;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
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

    for (const char* pairing : kPairings)
      if (probe.bounds(pairing)) routes.push_back(pairing);
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
          verdicts.push_back({key, kStateWords[(size_t)row.cacheState],
                              Composer::promotionReason(row.promotion),
                              refused.empty() ? "none" : refused});
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
        .children({text(key).styleClass("readout").absolute().inset(9, 0, 0, 9)});
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
                                    .inset(26, 0, 0, 0)})});

    // THE WIRES, each keyed by the pair it joins: four operators of the
    // plate that holds the nodes, so each is routed from where its own two
    // nodes settled.
    struct Pairing {
      const char* from;
      const char* to;
      Router router;
    };
    const Pairing pairings[] = {
        {"spun", kProbe, routers::orthogonal(routers::Bend::VFirst, 8)},
        {"glass", kProbe, routers::arc(0.18f)},
        {kProbe, "baked", routers::straight()},
        {kProbe, "live", routers::arc(-0.18f)}};
    std::vector<Operator> wires;
    for (const Pairing& one : pairings)
      wires.push_back(connect::Between{
          .from = one.from, .to = one.to, .router = one.router, .wire = wire});

    // The diagram is composed on a probe of its own before it stands on
    // the sheet, so its classes are stated on the diagram itself.
    return sketch::kit::well({.width = kDiagram,
                              .height = kPicture,
                              .ground = Fill::color({0.085f, 0.09f, 0.10f, 1})})
        .applyStyleSheet(sheetClasses(sketch::kit::theme()))
        .operators(std::move(wires))
        .children({nodes});
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
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    Element costs = box().column().gap(12).width(560).children(
        {each(verdicts, [](const Verdict& row) {
          return sketch::kit::well({.width = 560, .padding = 16})
              .row()
              .gap(20)
              .children({box().width(108).column().gap(8).children(
                             {document::label(row.key),
                              text(row.state).styleClass("readout")}),
                         box().width(400).column().gap(8).children(
                             {text(row.reason).width(400),
                              document::caption("Refusals: " + row.refusals)
                                  .width(400)})});
        })});
    return sketch::kit::page(
               {.title = "Ask the tree what happened",
                .subtitle =
                    "A route query describes connectivity. A profile describes "
                    "why each drawn node kept its cache state.",
                .footer = "Both queries run after a frame is drawn. The reason "
                          "is the first matching condition; the refusal mask "
                          "includes every matching condition."},
               box().row().gap(32).children(
                   {box().column().gap(18).width(kDiagram).children(
                        {sketch::kit::sectionHeader(
                             {.label = "CONNECTIVITY", .note = "hub"}),
                         diagram(),
                         text("bounds(\"<from>-><to>\")").styleClass("readout"),
                         lines(routes, kDiagram, "No wires answered"),
                         text("A wire is keyed by the pair it joins, so the "
                              "question is asked of that key. The pairing "
                              "hub->nowhere names a node the operator's scope "
                              "does not hold, so nothing was routed and "
                              "nothing answers.")
                             .width(kDiagram)}),
                    box().column().gap(18).width(560).children(
                        {sketch::kit::sectionHeader(
                             {.label = "CACHE VERDICTS",
                              .note = "composer.profile()"}),
                         std::move(costs)})}))
        .applyStyleSheet(sheetClasses(sketch::kit::theme()));
  }
};

SIGIL_SKETCH(RoutesProbe, "Kit · API",
             "the wires that answer for one hub, and the profile row each "
             "of four bake refusals produces, both read off the composer "
             "after the frame they describe")
