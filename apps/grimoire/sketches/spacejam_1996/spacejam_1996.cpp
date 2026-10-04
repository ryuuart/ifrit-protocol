// SPACE JAM, www.spacejam.com, 1996 — the page as Netscape Navigator 3.0
// showed it on a Windows 95 desktop over a 28.8k modem.
//
// Reload is pressed and the page comes down the line: the document, then
// the tiled star field all at once, then the sixteen GIFs four at a time,
// the interlaced ones in Netscape's blocky passes and the rest top to
// bottom; the throbber's meteors fly and the status bar counts the bytes.
// When the last GIF lands — the visitor counter, from a counter service
// that is always slow — Fast Break's ball starts to bounce, and the
// pointer visits the planets: each one lights under it, the pointer
// becomes the hand and the status bar reads the link. Then it goes back
// to Reload and presses it.
//
// THE RECORD, in data/: `page.csv` is the page's requests in the order it
// makes them, with every file's size as served, where the browser placed
// each image, its label and its link. `tour.csv` is where the pointer
// rests and when. THIS STUDY'S OWN: the chrome, drawn from memory of the
// browser; which GIFs were saved interlaced; the rollover images, which
// the page never had; the counter.
//
// Everything is authored in 1996 pixels and shown at twice that. Each GIF
// is drawn once, then decoded as the browser decoded it: one bit of
// transparency, matted on black, each colour put onto Netscape's
// 216-colour cube; so it is shown as whole 1996 pixels. What stands still
// — the window, the star field, the page's text, each GIF once it has
// arrived — is a texture or an image, and only what moves is live: the
// pointer, the ball's frame, a planet's rollover, the progress bar and,
// while loading, the arriving GIFs and the meteors.

// TAGS: Interfaces/Web

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPicture.h>
#include <sigildata/table/Table.h>
#include <sigildraw/Pen.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/kit/Page.h>

#include <cstdio>
#include <cstring>

#include "Artwork.h"
#include "Browser.h"

namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
using namespace spacejam;

namespace {

// The line: 28.8k gives about 3.5K a second; the study runs it two and a
// half times faster, over Netscape's four connections.
constexpr double kLineBytesPerSecond = 3600;
constexpr double kBytesPerSecond = kLineBytesPerSecond * 2.5;
constexpr int kConnections = 4;
constexpr double kConnectSeconds = 0.8;  // contacting the host, then waiting
constexpr float kZoom = 2;               // canvas pixels to a 1996 pixel
constexpr int kFirstVisitor = 482913;

/** ONE REQUEST: what the page asks for, and what came back. */
struct Gif {
  std::string name;
  double bytes = 0;
  bool interlaced = false;
  float left = 0, top = 0, width = 0, height = 0;
  Label label;
  std::string href;
  /** When each byte count was reached, seconds into the load. */
  struct Mark {
    double seconds, bytes;
  };
  std::vector<Mark> transfer;
  sk_sp<SkImage> plain, lit;  // as it stands, and as its rollover
  motion::Animatable<float> received = motion::animatable(0.0f);  // fraction
  motion::Animatable<float> rollover = motion::animatable(0.0f);

  double receivedAt(double seconds) const {
    if (transfer.empty() || seconds <= transfer.front().seconds) return 0;
    for (size_t index = 1; index < transfer.size(); ++index)
      if (seconds < transfer[index].seconds) {
        const Mark from = transfer[index - 1], to = transfer[index];
        return from.bytes + (to.bytes - from.bytes) * (seconds - from.seconds) /
                                (to.seconds - from.seconds);
      }
    return bytes;
  }
};

/** A GIF AS THE BROWSER SHOWED IT: the drawing rasterised at its own 1996
 *  size, then one bit of transparency — a pixel at least half covered is
 *  opaque, over the black the artist matted it on — and each channel put
 *  onto the 216-colour cube, the six levels a Netscape on an 8-bit screen
 *  mapped every image to as it decoded it. */
sk_sp<SkImage> decodeGif(const Element& drawing, float width, float height,
                         weave::FontContext& fonts, bool opaque = false) {
  const sk_sp<SkPicture> picture =
      snapshot(box().children({stack()
                                   .width(width)
                                   .height(height)
                                   .overflow(Overflow::Clip)
                                   .children({drawing})}),
               fonts, {width, height});
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::Make(
      (int)width, (int)height, kRGBA_8888_SkColorType, kPremul_SkAlphaType));
  pixels.eraseColor(opaque ? SK_ColorBLACK : SK_ColorTRANSPARENT);
  SkCanvas(pixels).drawPicture(picture);
  auto cube = [](uint8_t channel) {
    return (uint8_t)((channel + 25) / 51 * 51);
  };
  for (int row = 0; row < pixels.height(); ++row)
    for (int column = 0; column < pixels.width(); ++column) {
      // Premultiplied colour is the colour over black, which is the matte.
      uint8_t* pixel = static_cast<uint8_t*>(pixels.getAddr(column, row));
      const bool covered = pixel[3] >= 128;
      for (int channel = 0; channel < 3; ++channel)
        pixel[channel] = covered ? cube(pixel[channel]) : 0;
      pixel[3] = covered ? 255 : 0;
    }
  pixels.setImmutable();
  return pixels.asImage();
}

sk_sp<SkImage> pointerImage(std::span<const char* const> mask) {
  int width = 0;
  for (const char* row : mask) width = std::max(width, (int)std::strlen(row));
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::Make(
      width, (int)mask.size(), kRGBA_8888_SkColorType, kPremul_SkAlphaType));
  pixels.eraseColor(SK_ColorTRANSPARENT);
  for (size_t row = 0; row < mask.size(); ++row)
    for (size_t column = 0; mask[row][column]; ++column)
      if (mask[row][column] != ' ') {
        const uint8_t level = mask[row][column] == '#' ? 0 : 255;
        uint8_t* pixel =
            static_cast<uint8_t*>(pixels.getAddr((int)column, (int)row));
        pixel[0] = pixel[1] = pixel[2] = level;
        pixel[3] = 255;
      }
  pixels.setImmutable();
  return pixels.asImage();
}

/** AN ARRIVING GIF, drawn as far as its bytes go. A plain one fills from
 *  the top; an interlaced one arrives as every eighth row, then the
 *  fourth, the second and the rest, and Netscape stretched each row it
 *  had down over the rows it did not have yet — the blocky first pass
 *  that sharpens. @p frameWidth is how much of the image one frame is. */
Element arriving(const Gif& gif, float frameWidth) {
  return custom([image = gif.plain, received = gif.received,
                 interlaced = gif.interlaced,
                 frameWidth](sigil::draw::Pen& pen, const PaintContext&) {
           if (!image) return;
           const int rows = image->height();
           const int decoded = (int)std::floor(received.value() * (float)rows);
           std::vector<int> source(rows, -1);  // the decoded row each row shows
           if (!interlaced) {
             for (int row = 0; row < std::min(decoded, rows); ++row)
               source[row] = row;
           } else {
             std::vector<int> order;
             for (auto [first, step] :
                  {std::pair{0, 8}, {4, 8}, {2, 4}, {1, 2}})
               for (int row = first; row < rows; row += step)
                 order.push_back(row);
             std::vector<bool> have(rows, false);
             for (int index = 0; index < std::min(decoded, rows); ++index)
               have[order[index]] = true;
             int latest = -1;
             for (int row = 0; row < rows; ++row) {
               if (have[row]) latest = row;
               if (latest >= 0 && row - latest < 8) source[row] = latest;
             }
           }
           SkCanvas& canvas = *pen.canvas();
           const SkSamplingOptions nearest(SkFilterMode::kNearest);
           for (int row = 0; row < rows;) {
             int end = row + 1;
             while (end < rows && source[end] == source[row]) ++end;
             if (source[row] >= 0)
               canvas.drawImageRect(
                   image,
                   SkRect::MakeXYWH(0, (float)source[row], frameWidth, 1),
                   SkRect::MakeXYWH(0, (float)row, frameWidth,
                                    (float)(end - row)),
                   nearest, nullptr, SkCanvas::kStrict_SrcRectConstraint);
             row = end;
           }
         })
      .cache(Cache::None);
}

}  // namespace

struct SpaceJam1996 {
  std::vector<Gif> gifs;
  struct Rest {
    double arrive, leave;
    std::string target;
  };
  std::vector<Rest> tour;
  double loadEnd = 0, cycle = 0;
  double totalBytes = 0;

  Pattern stars;
  sk_sp<SkImage> arrowPointer, handPointer, ballStrip;
  sk_sp<SkRuntimeEffect> ballProgram;

  // What moves, as bound values.
  motion::Animatable<float> clock =
      motion::animatable(0.0f);  // seconds into the cycle
  motion::Animatable<float> progress = motion::animatable(0.0f);
  motion::Animatable<float> ballFrame = motion::animatable(0.0f);
  motion::Animatable<float> pointerX = motion::animatable(0.0f);
  motion::Animatable<float> pointerY = motion::animatable(0.0f);
  motion::Animatable<float> overLink = motion::animatable(0.0f);

  // What changes the tree, and is described again when it does.
  enum class Toolbar { Idle, Loading, Pressed };
  struct Scene {
    uint32_t started = 0, arrived = 0;
    Toolbar toolbar = Toolbar::Idle;
    long visit = -1;
    bool operator==(const Scene&) const = default;
  } shown;
  std::string status;

  const Gif* find(std::string_view name) const {
    for (const Gif& gif : gifs)
      if (gif.name == name) return &gif;
    return nullptr;
  }

  // ---- the record ---------------------------------------------------------

  void read(sketch::SketchContext& context) {
    const auto load = [&](const char* file) {
      return context.assets.hub().load<sigil::data::Table>(
          context.local(std::string("data/") + file));
    };
    if (const auto page = load("page.csv")) {
      const auto name = page->column<std::string>("name");
      const auto bytes = page->column<double>("bytes");
      const auto interlaced = page->column<double>("interlaced");
      const auto left = page->column<double>("x"),
                 top = page->column<double>("y");
      const auto width = page->column<double>("width"),
                 height = page->column<double>("height");
      const auto label = page->column<std::string>("label");
      const auto ink = page->column<std::string>("ink");
      const auto href = page->column<std::string>("href");
      for (size_t row = 0; row < name.size(); ++row)
        gifs.push_back(
            {.name = name[row],
             .bytes = bytes[row],
             .interlaced = interlaced[row] != 0,
             .left = (float)left[row],
             .top = (float)top[row],
             .width = (float)width[row],
             .height = (float)height[row],
             .label = {label[row],
                       ink[row].empty()
                           ? C5(0xFFFF00)
                           : C5((uint32_t)std::stoul(ink[row], nullptr, 16))},
             .href = href[row]});
    }
    if (const auto rests = load("tour.csv")) {
      const auto arrive = rests->column<double>("arrive"),
                 leave = rests->column<double>("leave");
      const auto target = rests->column<std::string>("target");
      for (size_t row = 0; row < target.size(); ++row)
        tour.push_back({arrive[row], leave[row], target[row]});
    }
    cycle = tour.empty() ? 20 : tour.back().leave;
  }

  /** THE LOAD, worked out once: the document alone, then the images in
   *  the page's order four at a time, the line shared evenly between the
   *  connections open. */
  void scheduleTransfers() {
    std::vector<double> remaining;
    for (const Gif& gif : gifs) {
      remaining.push_back(gif.bytes);
      totalBytes += gif.bytes;
    }
    double seconds = kConnectSeconds;
    while (true) {
      std::vector<size_t> open;
      for (size_t index = 0;
           index < gifs.size() && (int)open.size() < kConnections; ++index) {
        if (remaining[index] <= 0) continue;
        open.push_back(index);
        if (index == 0)
          break;  // no image is asked for before the document is read
      }
      if (open.empty()) break;
      const double share = kBytesPerSecond / (double)open.size();
      double step = 1e9;
      for (size_t index : open) {
        if (gifs[index].transfer.empty())
          gifs[index].transfer.push_back({seconds, 0});
        step = std::min(step, remaining[index] / share);
      }
      seconds += step;
      for (size_t index : open) {
        remaining[index] = std::max(0.0, remaining[index] - share * step);
        gifs[index].transfer.push_back(
            {seconds, gifs[index].bytes - remaining[index]});
      }
    }
    loadEnd = seconds;
  }

  Element drawing(const Gif& gif, weave::FontContext& fonts, long visit) const {
    const std::string& name = gif.name;
    if (name == "fast") return wordmark(fonts, gif.label, true);
    if (name == "break") return wordmark(fonts, gif.label, false);
    if (name == "fastbreak") return ballFilmstrip(ballProgram);
    if (name == "pressbox") return artPressBox(fonts, gif.label);
    if (name == "jamcentral") return artJamCentral(fonts, gif.label);
    if (name == "bball") return artBball(fonts, gif.label, ballProgram);
    if (name == "lunartunes") return artLunarTunes(fonts, gif.label);
    if (name == "lineup") return artLineup(fonts, gif.label);
    if (name == "jamlogo") return artLogo(fonts, gif.label);
    if (name == "jump") return artJump(fonts, gif.label);
    if (name == "junior") return artJunior(fonts, gif.label);
    if (name == "studiostore") return artStudioStore(fonts, gif.label);
    if (name == "souvenirs") return artSouvenirs(fonts, gif.label);
    if (name == "sitemap") return artSitemap(fonts, gif.label);
    if (name == "behind") return artBehind(fonts, gif.label);
    if (name == "counter") {
      char figures[16];
      std::snprintf(figures, sizeof figures, "%07ld", kFirstVisitor + visit);
      return artCounter(fonts, figures);
    }
    return box();
  }

  /** Every GIF decoded; a linked one also as its rollover, the same
   *  drawing brightened with a yellow glow about it, inside the same box. */
  void decode(weave::FontContext& fonts, long visit, bool counterOnly) {
    for (Gif& gif : gifs) {
      if (gif.width <= 0 || gif.name == "bg_stars" ||
          (counterOnly && gif.name != "counter"))
        continue;
      const float width =
          gif.name == "fastbreak" ? kBallFrame * kBallFrames : gif.width;
      const Element art = drawing(gif, fonts, visit);
      gif.plain = decodeGif(art, width, gif.height, fonts);
      if (!gif.href.empty())
        gif.lit = decodeGif(box()
                                .width(width)
                                .height(gif.height)
                                .filter(material::Filter::brightness(1.3f).then(
                                    material::Filter::glow(C5(0xFFFF73), 2.5f)))
                                .children({art}),
                            width, gif.height, fonts);
    }
  }

  // ---- the page -----------------------------------------------------------

  Element window() const {
    Element frame =
        dressed(stack().width(kWindowWidth).height(kWindowHeight).fill(kFace),
                raised());
    // the caption
    frame.children(
        {rect(3, 3, kWindowWidth - 6, 18).fill(kCaption),
         kit::centred(label("N", "caption"))
             .left(5)
             .top(5)
             .width(14)
             .height(14)
             .fill(material::hexColor(0x18086B)),
         label("Netscape - [Space Jam]", "caption").left(23).top(5)});
    const char* glyphs[3] = {"_", "□", "×"};
    for (int index = 0; index < 3; ++index)
      frame.children({dressed(kit::centred(label(glyphs[index]))
                                  .left(kWindowWidth - 57 + (float)index * 16 +
                                        (index == 2 ? 2 : 0))
                                  .top(5)
                                  .width(16)
                                  .height(14)
                                  .fill(kFace),
                              raised())});
    // the menu
    Element menu = box().row().gap(13).left(10).top(24);
    for (const char* item : {"File", "Edit", "View", "Go", "Bookmarks",
                             "Options", "Directory", "Window", "Help"})
      menu.children({label(item)});
    frame.children({std::move(menu)});
    // the bands' etched rules
    for (float rule : {40.0f, 86.0f, 111.0f, 131.0f})
      frame.children({rect(3, rule, kWindowWidth - 6, 1).fill(kShadow),
                      rect(3, rule + 1, kWindowWidth - 6, 1).fill(kHighlight)});
    // the throbber's ground and the location
    frame.children(
        {dressed(stack()
                     .left(kThrobberLeft)
                     .top(kToolbarTop)
                     .width(kThrobberSize)
                     .height(kThrobberSize)
                     .overflow(Overflow::Clip)
                     .children({throbberSky()}),
                 field()),
         label("Location:").left(8).top(93),
         dressed(box()
                     .row()
                     .alignItems(Align::Center)
                     .left(60)
                     .top(89)
                     .width(582)
                     .height(20)
                     .fill(kHighlight)
                     .paddingLeft(4)
                     .children({label("http://www.spacejam.com/")}),
                 sunken()),
         dressed(
             kit::centred(rect(0, 0, 7, 4)
                              .shape(unitPolygon({{0, 0}, {1, 0}, {0.5f, 1}}))
                              .fill(kDark))
                 .left(624)
                 .top(91)
                 .width(16)
                 .height(16)
                 .fill(kFace),
             raised())});
    // the directory buttons
    Element directory =
        box().row().gap(2).left(4).top(114).width(kWindowWidth - 8).height(16);
    for (const char* place : {"What's New?", "What's Cool?", "Destinations",
                              "Net Search", "People", "Software"})
      directory.children({dressed(
          kit::centred(label(place)).flexGrow(1).flexBasis(0).fill(kFace),
          raised())});
    frame.children({std::move(directory)});
    // the content well and the status bar
    frame.children({dressed(rect(kPageLeft - 2, kPageTop - 2, kPageWidth + 4,
                                 kPageHeight + 4)
                                .fill(kDark),
                            sunken()),
                    dressed(kit::centred(brokenKey())
                                .left(4)
                                .top(kStatusTop)
                                .width(24)
                                .height(18),
                            field()),
                    dressed(rect(30, kStatusTop, 488, 18), field()),
                    dressed(rect(522, kStatusTop, 124, 18), field())});
    return frame;
  }

  Element toolbar(Toolbar state) const {
    const bool loading = state == Toolbar::Loading;
    return box()
        .row()
        .gap(2)
        .left(4)
        .top(kToolbarTop)
        .children({toolButton(Tool::Back, "Back", false),
                   toolButton(Tool::Forward, "Forward", false),
                   toolButton(Tool::Home, "Home", true),
                   toolButton(Tool::Reload, "Reload", true,
                              state == Toolbar::Pressed),
                   toolButton(Tool::Images, "Images", false),
                   toolButton(Tool::Open, "Open", true),
                   toolButton(Tool::Print, "Print", true),
                   toolButton(Tool::Find, "Find", true),
                   toolButton(Tool::Stop, "Stop", loading)});
  }

  /** The page's own content: the star tile, the text and the GIFs. */
  Element page() const {
    const Gif& document = gifs.front();
    Element content = stack()
                          .left(kPageLeft)
                          .top(kPageTop)
                          .width(kPageWidth)
                          .height(kPageHeight)
                          .overflow(Overflow::Clip);
    // A tiled background cannot arrive in bands — every repeat would show
    // its own — so the field is black until its last byte, then whole.
    const auto whenComplete = [](const Gif& gif) {
      return motion::bind(gif.received,
                          {.to = {-999.0f, 1.0f}, .clamp = {0.0f, 1.0f}});
    };
    if (const Gif* tile = find("bg_stars"))
      content.children({stack()
                            .inset(0)
                            .opacity(whenComplete(*tile))
                            .children({box()
                                           .inset(0)
                                           .fill(stars.material())
                                           .cache(Cache::Texture)
                                           .key("stars")})});
    // <font size="-1"> in red Times, hard-wrapped by the author's <br>.
    content.children(
        {kit::at(stack(), 0, 757, kPageWidth, 62)
             .opacity(whenComplete(document))
             .children(
                 {box()
                      .column()
                      .alignItems(Align::Center)
                      .left(0)
                      .top(0)
                      .width(kPageWidth)
                      .children(
                          {label(
                               "SPACE JAM, characters, names, and all related",
                               "copy"),
                           label(
                               "indicia are trademarks of Warner Bros. © 1996",
                               "copy")}),
                  label("You are visitor number", "copy").left(214).top(43)})
             .cache(Cache::Texture)
             .key("copy")});
    for (const Gif& gif : gifs) {
      const int index = (int)(&gif - gifs.data());
      if (gif.width <= 0 || gif.name == "bg_stars" ||
          !(shown.started & (1u << index)))
        continue;
      const bool ball = gif.name == "fastbreak";
      const float frameWidth = ball ? kBallFrame : gif.width;
      if (!(shown.arrived & (1u << index))) {
        content.children({kit::at(arriving(gif, frameWidth), gif.left, gif.top,
                                  gif.width, gif.height)});
        continue;
      }
      Element shownGif =
          stack().left(gif.left).top(gif.top).width(gif.width).height(
              gif.height);
      if (ball) {
        // The GIF's frames behind a gate one frame wide, stepped along.
        shownGif.overflow(Overflow::Clip)
            .children({image(gif.plain, material::Fit::Stretch)
                           .left(0)
                           .top(0)
                           .width(kBallFrame * kBallFrames)
                           .height(kBallFrame)
                           .translateX(motion::bind(
                               ballFrame, {.to = {0, -kBallFrame}}))});
      } else {
        shownGif.children({image(gif.plain, material::Fit::Stretch).inset(0)});
        if (gif.lit)
          shownGif.children({image(gif.lit, material::Fit::Stretch)
                                 .inset(0)
                                 .opacity(gif.rollover)});
      }
      content.children({std::move(shownGif)});
    }
    return content;
  }

  Element describe() const {
    const bool loading = shown.toolbar == Toolbar::Loading;
    Element throbber = stack()
                           .left(kThrobberLeft + 2)
                           .top(kToolbarTop + 2)
                           .width(kThrobberSize - 4)
                           .height(kThrobberSize - 4)
                           .overflow(Overflow::Clip);
    if (loading)
      throbber.children(
          {meteors()
               .key("meteors")
               .left(-40)
               .top(-28)
               .translateX(motion::bind(
                   clock, {.from = {0, 0.9f}, .to = {0, 64}, .wrap = 64}))
               .translateY(motion::bind(
                   clock, {.from = {0, 0.9f}, .to = {0, 45}, .wrap = 45}))});
    throbber.children({throbberLetter().key("letter")});

    Element pointer =
        stack()
            .width(16)
            .height(21)
            .translateX(pointerX)
            .translateY(pointerY)
            .children({kit::at(image(arrowPointer, material::Fit::Stretch), 0,
                               0, (float)arrowPointer->width(),
                               (float)arrowPointer->height())
                           .opacity(motion::bind(overLink, {.reverse = true})),
                       kit::at(image(handPointer, material::Fit::Stretch), -5,
                               0, (float)handPointer->width(),
                               (float)handPointer->height())
                           .opacity(overLink)});

    return stack()
        .width(kWindowWidth)
        .height(kWindowHeight)
        .scale(kZoom)
        .transformOrigin(pct(0), pct(0))
        .imageRendering(material::Sampling::Nearest)
        .applyStyleSheet(chromeSheet())
        .children(
            {window().cache(Cache::Texture).key("window"),
             toolbar(shown.toolbar)
                 .cache(Cache::Texture)
                 .key(loading                             ? "toolbar loading"
                      : shown.toolbar == Toolbar::Pressed ? "toolbar pressed"
                                                          : "toolbar"),
             std::move(throbber),
             box()
                 .row()
                 .alignItems(Align::Center)
                 .left(34)
                 .top(kStatusTop)
                 .width(480)
                 .height(18)
                 .overflow(Overflow::Clip)
                 .children({slot("status")}),
             rect(524, kStatusTop + 2, 120, 14)
                 .fill(kCaption)
                 .scaleX(progress)
                 .transformOrigin(pct(0), pct(50)),
             page(), std::move(pointer)});
  }

  // ---- time ---------------------------------------------------------------

  glm::vec2 restPoint(const std::string& target) {
    if (const Gif* gif = find(target))
      return {kPageLeft + gif->left + gif->width * 0.5f,
              kPageTop + gif->top + gif->height * 0.6f};
    return {174, 62};  // the Reload button
  }

  void advance(double elapsed, sketch::SketchContext& context) {
    const long visit = (long)std::floor(elapsed / cycle);
    const double seconds = elapsed - (double)visit * cycle;
    clock = (float)seconds;
    ballFrame = (float)((long)std::floor(elapsed * 10.0) % kBallFrames);

    Scene scene{.visit = visit};
    double received = 0;
    for (size_t index = 0; index < gifs.size(); ++index) {
      Gif& gif = gifs[index];
      const double bytes = gif.receivedAt(seconds);
      received += bytes;
      gif.received = (float)(bytes / gif.bytes);
      if (bytes > 0) scene.started |= 1u << index;
      if (bytes >= gif.bytes) scene.arrived |= 1u << index;
    }
    const bool loading = seconds < loadEnd;
    progress = loading ? (float)(received / totalBytes) : 0.0f;

    // The pointer: resting at a stop, or easing to the next one.
    std::string resting;
    glm::vec2 at = restPoint(tour.front().target);
    for (size_t index = 0; index < tour.size(); ++index) {
      const Rest& rest = tour[index];
      if (seconds >= rest.arrive && seconds <= rest.leave) {
        at = restPoint(rest.target);
        resting = rest.target;
        break;
      }
      if (index + 1 < tour.size() && seconds > rest.leave &&
          seconds < tour[index + 1].arrive) {
        const float along = (float)((seconds - rest.leave) /
                                    (tour[index + 1].arrive - rest.leave));
        at = glm::mix(restPoint(rest.target), restPoint(tour[index + 1].target),
                      along * along * (3 - 2 * along));
        break;
      }
    }
    pointerX = at.x;
    pointerY = at.y;
    const Gif* over = resting.empty() ? nullptr : find(resting);
    overLink = over ? 1.0f : 0.0f;
    for (Gif& gif : gifs) gif.rollover = &gif == over ? 1.0f : 0.0f;

    scene.toolbar = loading                 ? Toolbar::Loading
                    : seconds > cycle - 0.3 ? Toolbar::Pressed
                                            : Toolbar::Idle;
    if (scene.visit != shown.visit)
      decode(*context.fonts, visit, shown.visit >= 0);
    if (!(scene == shown)) {
      shown = scene;
      context.composer.render(describe());
    }

    char line[160];
    if (seconds < 0.35)
      std::snprintf(line, sizeof line,
                    "Connect: Contacting host: www.spacejam.com...");
    else if (seconds < kConnectSeconds)
      std::snprintf(line, sizeof line,
                    "Connect: Host contacted. Waiting for reply...");
    else if (loading)
      std::snprintf(
          line, sizeof line, "%d%% of %dK (at %.1fK/sec, %d secs remaining)",
          (int)(100 * received / totalBytes),
          (int)std::lround(totalBytes / 1024), kLineBytesPerSecond / 1024,
          (int)std::ceil((totalBytes - received) / kLineBytesPerSecond));
    else if (over)
      std::snprintf(line, sizeof line, "http://www.spacejam.com/%s",
                    over->href.c_str());
    else
      std::snprintf(line, sizeof line, "Document: Done.");
    if (status != line) {
      status = line;
      context.composer.renderSlot("status", label(line));
    }
  }

  void setup(sketch::SketchContext& context) {
    // Taken with the page whole and the pointer resting on Jam Central.
    sketch::kit::stage(context, {.size = SkSize::Make(kWindowWidth * kZoom,
                                                      kWindowHeight * kZoom),
                                 .captureAt = 9.5,
                                 .background = kDark});
    read(context);
    scheduleTransfers();
    ballProgram = SkRuntimeEffect::MakeForShader(
                      SkString(context.assets.hub()
                                   .text(context.local("ball.sksl"))
                                   .value_or("")))
                      .effect;
    arrowPointer = pointerImage(kArrowPointer);
    handPointer = pointerImage(kHandPointer);
    const sk_sp<SkImage> tile =
        decodeGif(starTile(), kStarTile, kStarTile, *context.fonts, true);
    stars = Pattern::tile({kStarTile, kStarTile}, [tile](SkCanvas& canvas,
                                                         SkSize, uint32_t) {
              canvas.drawImage(tile, 0, 0);
            }).sampling(material::Sampling::Nearest);
    advance(0, context);
  }

  void update(double elapsed, sketch::SketchContext& context) {
    advance(elapsed, context);
  }
};

SIGIL_SKETCH(SpaceJam1996, "Study · Screens",
             "spacejam.com in Netscape 3 over a 28.8k line — the GIFs "
             "interlacing in, the planets lit "
             "under the pointer, the ball bouncing")
