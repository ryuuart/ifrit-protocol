// Winamp 2's base skin (Nullsoft) on a Windows 98 desktop: the main window,
// the equaliser and the playlist docked in one stack, playing Daft Punk.
// The skin is pixel art and is drawn as pixel art — every mark a whole skin
// pixel, every bevel one pixel light and one pixel dark, every word in the
// skin's own bitmap fonts — on a 440 x 649 grid shown at exactly three
// canvas pixels per skin pixel.
//
// `data/skin.json` is the palette, VISCOLOR.TXT's twenty-four colours,
// TEXT.BMP's 5 x 6 font and the segments of NUMBERS.BMP's digits;
// `data/playlist.csv` is the list; `data/session.json` is what happens —
// the track and where it starts, its tempo, when the visualiser changes
// mode and how the equaliser is moved.
//
// THE PICTURE: one texture holds everything that stands still — the
// desktop, the taskbar and the three windows' skins. Over it, small paint
// programs draw what the player itself redraws: the time, the spectrum
// analyser or the oscilloscope, the eleven faders with their curve, and
// the playlist's clock. The scrolling title and the seek thumb are
// drawings moved by a bound offset.
//
// TAGS: Interfaces/Desktop

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/PixelType.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigilcore/compute/Noise.h>
#include <sigildata/decode/Json.h>
#include <sigildata/table/Table.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace data = sigil::data;
namespace draw = sigil::draw;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;
using Colour = sigil::material::Color;

namespace {

/** Canvas pixels per skin pixel: an integer, so every skin pixel is a
 *  whole block of canvas pixels and nothing is resampled. */
constexpr float kSkinPixel = 3;
/** The desktop, in skin pixels. */
constexpr float kDesktopWidth = 440, kDesktopHeight = 649;

/** Where each window of the docked stack stands on the desktop. The three
 *  are one column 275 pixels wide, each docked to the foot of the last. */
struct Origin {
  float x, y;
};
constexpr Origin kMain{147, 18};
constexpr Origin kEqualizer{147, 134};
constexpr Origin kPlaylist{147, 250};
constexpr float kWindowWidth = 275;
constexpr float kPlaylistHeight = 348;
constexpr float kRowPitch = 13;

// ---------------------------------------------------------------------------
// WHAT IS READ FROM data/

/** A palette read by name, so the painters say which colour they mean. */
struct Palette {
  std::map<std::string, Colour, std::less<>> named;
  Colour operator()(std::string_view name) const {
    const auto found = named.find(name);
    return found == named.end() ? Colour{1, 0, 1, 1} : found->second;
  }
};

/** TEXT.BMP: uppercase glyphs on a fixed advance, one sprite each. */
struct BitmapFont {
  std::map<char, kit::Sprite> glyphs;
  float advance = 5;

  float width(std::string_view words) const {
    return advance * (float)words.size() - 1;
  }
  void print(const kit::PixelInk& ink, float x, float y, std::string_view words,
             Colour colour) const {
    for (char letter : words) {
      const auto found = glyphs.find((char)std::toupper((unsigned char)letter));
      if (found != glyphs.end())
        for (const kit::SpriteRun& run : found->second.runs)
          ink.rect(x + run.x, y + run.y, run.w, run.h, colour);
      x += advance;
    }
  }
};

/** NUMBERS.BMP: the seven segments of a 9 x 13 cell, and which of them
 *  each digit lights. */
struct SegmentDigits {
  std::map<char, std::array<float, 4>> segments;
  std::array<std::string, 10> lit;

  void draw(const kit::PixelInk& ink, float x, float y, std::string_view which,
            Colour colour) const {
    for (char segment : which) {
      const auto found = segments.find(segment);
      if (found == segments.end()) continue;
      const auto& [left, top, width, height] = found->second;
      ink.rect(x + left, y + top, width, height, colour);
    }
  }
  void digit(const kit::PixelInk& ink, float x, float y, int value,
             Colour colour) const {
    draw(ink, x, y, lit[(size_t)std::clamp(value, 0, 9)], colour);
  }
};

struct Track {
  std::string title;
  int seconds = 0;
};

struct Drag {
  int slider = 0;
  double at = 0, duration = 1;
  float to = 0;
};

struct ModeChange {
  double at = 0;
  bool oscilloscope = false;
};

std::string clockTime(int seconds) {
  return kit::formatted("%d:%02d", seconds / 60, seconds % 60);
}

std::string upper(std::string words) {
  for (char& letter : words) letter = (char)std::toupper((unsigned char)letter);
  return words;
}

/** A hashed coin in [0, 1): the same pixel always lands the same way. */
float coin(int x, int y, int seed = 0) {
  return (float)(sigil::core::noise::lattice((uint32_t)seed, x, y, 0) &
                 0xffffff) /
         (float)0x1000000;
}

float smoothstep01(float value) {
  value = std::clamp(value, 0.0f, 1.0f);
  return value * value * (3 - 2 * value);
}

// ---------------------------------------------------------------------------
// THE PIXEL VERBS every painter below is made of

/** A raised plate: its face, one pixel of light along the top and left, one
 *  of shadow along the bottom and right. */
void raised(const kit::PixelInk& ink, float x, float y, float width,
            float height, Colour face, Colour light, Colour shadow) {
  ink.rect(x, y, width, height, face);
  ink.rect(x, y, width, 1, light);
  ink.rect(x, y, 1, height, light);
  ink.rect(x, y + height - 1, width, 1, shadow);
  ink.rect(x + width - 1, y, 1, height, shadow);
}

/** The same plate the other way up: a well, a trough, a pressed key. */
void sunken(const kit::PixelInk& ink, float x, float y, float width,
            float height, Colour face, Colour light, Colour shadow) {
  raised(ink, x, y, width, height, face, shadow, light);
}

/** A solid triangle on the pixel grid pointing right (+1) or left (-1),
 *  @p height rows tall. */
void arrow(const kit::PixelInk& ink, float x, float y, int height,
           int direction, Colour colour) {
  const int reach = (height + 1) / 2;
  for (int row = 0; row < height; ++row) {
    const int width = std::min(row, height - 1 - row) + 1;
    ink.row(direction > 0 ? x : x + (float)(reach - width), y + (float)row,
            (float)width, colour);
  }
}

/** A triangle pointing up, @p rows tall, centred on @p middle. */
void peak(const kit::PixelInk& ink, float middle, float y, int rows,
          Colour colour) {
  for (int row = 0; row < rows; ++row)
    ink.row(middle - (float)row, y + (float)row, (float)(2 * row + 1), colour);
}

// ---------------------------------------------------------------------------

struct WinampBase {
  Palette palette;
  std::array<Colour, 24> visual{};  // VISCOLOR.TXT
  BitmapFont text;
  SegmentDigits numbers;
  kit::Sprite bolt, cursor;
  kit::PixFont listFont, desktopFont, desktopBold;
  std::vector<Track> tracks;

  // the session
  int playing = 8, selected = 13;  // zero-based rows
  double startSecond = 180, tempo = 120, scrollSpeed = 22, scrollFrom = 1;
  std::string bitrate = "192", sampleRate = "44";
  std::vector<ModeChange> modes;
  std::vector<std::string> bandNames;
  std::array<float, 11> preset{};
  double presetAt = 1, presetFor = 0.35;
  std::vector<Drag> drags;
  std::string desktopClock = "9:41 PM";
  /** Where the pointer goes and when, in desktop pixels: derived from the
   *  session, so the hand that moves a slider is where that slider is. */
  struct Waypoint {
    double at;
    float x, y;
    bool follow;  // the leg into this point rides a drag's own curve
  };
  std::vector<Waypoint> pointer;

  // ---- the state the player redraws ----
  int second = 0;
  bool oscilloscope = false;
  std::array<float, 11> gain{};  // preamp, then the ten bands, in [-1, 1]
  static constexpr int kBars = 19;
  std::array<float, kBars> bar{}, cap{}, capSpeed{};
  std::array<float, 76> wave{};
  double lastStep = 0;
  /** Where the scrolling title stands and where the seek thumb stands, in
   *  whole skin pixels: the only two drawings that move. */
  motion::Animatable<float> scrollOffset = motion::animatable(0.0f);
  motion::Animatable<float> seekOffset = motion::animatable(0.0f);
  motion::Animatable<float> pointerX = motion::animatable(0.0f);
  motion::Animatable<float> pointerY = motion::animatable(0.0f);

  const Track& nowPlaying() const { return tracks[(size_t)playing]; }
  std::string marquee() const {
    return upper(std::to_string(playing + 1) + ". " + nowPlaying().title +
                 " (" + clockTime(nowPlaying().seconds) + ")  ***  ");
  }

  // =========================================================================
  // READING

  void read(sketch::SketchContext& context) {
    const auto json = [&](const char* name) {
      return context.assets.hub().load<data::Json>(
          context.local(std::string("data/") + name));
    };
    if (const auto skin = json("skin.json")) {
      for (const auto& [name, value] : (*skin)["palette"].object())
        if (value.kind() != data::Json::Kind::Array)
          palette.named[name] = material::parseColor(value.string());
      for (size_t index = 0; index < visual.size(); ++index)
        visual[index] =
            material::parseColor((*skin)["viscolor"][index].string());
      for (const auto& [name, value] : (*skin)["desktop"].object())
        if (name == "clock")
          desktopClock = value.string();
        else
          palette.named["desktop." + name] =
              material::parseColor(value.string());

      const std::array<Colour, 2> ink{Colour{0, 0, 0, 0}, Colour{1, 1, 1, 1}};
      text.advance = (float)(*skin)["text"]["advance"].number(5);
      for (const auto& [letter, grid] : (*skin)["text"]["glyphs"].object()) {
        std::vector<std::string> rows;
        for (const data::Json& row : grid.array())
          rows.emplace_back(row.string());
        if (auto sprite = kit::pixelMap(rows, {".#", ink}))
          text.glyphs[letter[0]] = *sprite;
      }
      for (const auto& [name, rect] : (*skin)["numbers"]["segments"].object())
        numbers.segments[name[0]] = {
            (float)rect[0].number(), (float)rect[1].number(),
            (float)rect[2].number(), (float)rect[3].number()};
      for (size_t digit = 0; digit < 10; ++digit)
        numbers.lit[digit] = (*skin)["numbers"]["digits"][digit].string();

      std::vector<std::string> rows;
      for (const data::Json& row : (*skin)["bolt"].array())
        rows.emplace_back(row.string());
      const std::array<Colour, 3> boltInk{Colour{0, 0, 0, 0}, palette("bolt"),
                                          palette("boltDark")};
      if (auto sprite = kit::pixelMap(rows, {".#o", boltInk})) bolt = *sprite;
      rows.clear();
      for (const data::Json& row : (*skin)["cursor"].array())
        rows.emplace_back(row.string());
      const std::array<Colour, 3> cursorInk{
          Colour{0, 0, 0, 0}, Colour{0, 0, 0, 1}, Colour{1, 1, 1, 1}};
      if (auto sprite = kit::pixelMap(rows, {".#o", cursorInk}))
        cursor = *sprite;
      const auto& fader = (*skin)["palette"]["fader"];
      for (size_t step = 0; step < fader.size(); ++step)
        palette.named["fader." + std::to_string(step)] =
            material::parseColor(fader[step].string());
    }
    if (const auto list = context.assets.hub().load<data::Table>(
            context.local("data/playlist.csv"))) {
      const auto titles = list->column<std::string>("title");
      const auto seconds = list->column<double>("seconds");
      for (size_t row = 0; row < titles.size(); ++row)
        tracks.push_back({titles[row], (int)seconds[row]});
    }
    if (tracks.empty()) tracks.push_back({"Nullsoft - Winamp", 60});
    if (const auto session = json("session.json")) {
      const data::Json& the = *session;
      playing = std::clamp((int)the["playing"].number(9) - 1, 0,
                           (int)tracks.size() - 1);
      selected = (int)the["selected"].number(14) - 1;
      startSecond = the["startSecond"].number(180);
      bitrate = the["bitrate"].string("192");
      sampleRate = the["sampleRate"].string("44");
      tempo = the["tempo"].number(120);
      scrollSpeed = the["scrollSpeed"].number(22);
      scrollFrom = the["scrollFrom"].number(1);
      for (const data::Json& change : the["visualiser"].array())
        modes.push_back(
            {change["at"].number(), change["mode"].string() == "oscilloscope"});
      const data::Json& equalizer = the["equalizer"];
      for (const data::Json& band : equalizer["bands"].array())
        bandNames.emplace_back(band.string());
      for (size_t slider = 0; slider < preset.size(); ++slider)
        preset[slider] = (float)equalizer["preset"][slider].number();
      presetAt = equalizer["loadAt"].number(1);
      presetFor = equalizer["loadFor"].number(0.35);
      for (const data::Json& drag : equalizer["drags"].array())
        drags.push_back({(int)drag["slider"].number(), drag["at"].number(),
                         drag["for"].number(1), (float)drag["to"].number()});
      const data::Json& rest = the["pointerRest"];
      plan({(float)rest[0].number(100), (float)rest[1].number(300)});
    }
  }

  /** A slider's gain at @p time: flat, then the preset glides in once
   *  PRESETS is clicked, then each drag carries it to where it is let go. */
  float gainAt(size_t slider, double time) const {
    float value =
        preset[slider] * smoothstep01((float)((time - presetAt) / presetFor));
    for (const Drag& drag : drags)
      if (drag.slider == (int)slider)
        value += (drag.to - value) *
                 smoothstep01((float)((time - drag.at) / drag.duration));
    return std::clamp(value, -1.0f, 1.0f);
  }

  /** The middle of a fader's thumb, on the desktop, at a gain. */
  static std::array<float, 2> thumbCentre(int slider, float value) {
    return {kEqualizer.x + faderLeft(slider) + 7,
            kEqualizer.y + 38 + std::round((1 - value) / 2 * 51) + 6};
  }

  /** THE POINTER'S ERRANDS: a click on PRESETS, each drag from where its
   *  thumb stands to where it is let go, and a click on the analyser at
   *  every change of mode — then back to rest. */
  void plan(std::array<float, 2> rest) {
    struct Errand {
      double at;
      std::array<float, 2> from, to;
      double duration;
    };
    std::vector<Errand> errands;
    errands.push_back({presetAt,
                       {kEqualizer.x + 239, kEqualizer.y + 24},
                       {kEqualizer.x + 239, kEqualizer.y + 24},
                       0});
    for (const Drag& drag : drags)
      errands.push_back(
          {drag.at,
           thumbCentre(drag.slider, gainAt((size_t)drag.slider, drag.at)),
           thumbCentre(drag.slider, drag.to), drag.duration});
    for (const ModeChange& change : modes)
      if (change.at > 0)
        errands.push_back({change.at,
                           {kMain.x + 62, kMain.y + 51},
                           {kMain.x + 62, kMain.y + 51},
                           0});
    std::sort(errands.begin(), errands.end(),
              [](const Errand& one, const Errand& other) {
                return one.at < other.at;
              });
    pointer = {{0, rest[0], rest[1], false}};
    for (const Errand& errand : errands) {
      pointer.push_back(
          {errand.at - 0.05, errand.from[0], errand.from[1], false});
      pointer.push_back({errand.at + errand.duration, errand.to[0],
                         errand.to[1], errand.duration > 0});
    }
    pointer.push_back({pointer.back().at + 1.2, rest[0], rest[1], false});
  }

  /** Where the pointer is at @p time: it holds at each point and sets off
   *  for the next with at most 0.6 s to go, so it travels, arrives, acts. */
  std::array<float, 2> pointerAt(double time) const {
    if (pointer.empty()) return {-20, -20};
    for (size_t leg = 1; leg < pointer.size(); ++leg) {
      const Waypoint& from = pointer[leg - 1];
      const Waypoint& to = pointer[leg];
      if (time >= to.at) continue;
      const double start = to.follow ? from.at : std::max(from.at, to.at - 0.6);
      const float along =
          smoothstep01((float)((time - start) / std::max(1e-3, to.at - start)));
      return {std::round(from.x + (to.x - from.x) * along),
              std::round(from.y + (to.y - from.y) * along)};
    }
    return {pointer.back().x, pointer.back().y};
  }

  // =========================================================================
  // THE STILL SKIN — painted once, into one texture

  /** A window's body: the skin's slate with its fine speckle, and the
   *  one-pixel frame that lifts it off the desktop. */
  void body(const kit::PixelInk& ink, Origin at, float width,
            float height) const {
    ink.rect(at.x, at.y, width, height, palette("body"));
    for (int y = 0; y < (int)height; ++y)
      for (int x = 0; x < (int)width; ++x) {
        const float toss = coin(x + (int)at.x, y + (int)at.y);
        if (toss < 0.16f)
          ink.px(at.x + (float)x, at.y + (float)y, palette("bodyLight"));
        else if (toss > 0.86f)
          ink.px(at.x + (float)x, at.y + (float)y, palette("bodyDark"));
      }
    ink.rect(at.x, at.y, width, 1, palette("edgeLight"));
    ink.rect(at.x, at.y, 1, height, palette("edgeLight"));
    ink.rect(at.x, at.y + height - 1, width, 1, palette("edgeDark"));
    ink.rect(at.x + width - 1, at.y, 1, height, palette("edgeDark"));
  }

  /** A window's title bar: the wordmark between two grips of cream rules,
   *  the option key at the left and the window keys at the right. Only
   *  the focused window's wordmark and grips are lit. */
  void titleBar(const kit::PixelInk& ink, Origin at, float height,
                std::string_view words, bool focused, bool minimise) const {
    const float width = kWindowWidth;
    ink.rect(at.x + 1, at.y + 1, width - 2, height - 1, palette("title"));
    ink.rect(at.x + 1, at.y + 1, width - 2, 1, palette("titleLight"));
    ink.rect(at.x + 1, at.y + height - 1, width - 2, 1, palette("edgeDark"));
    const float middle = at.y + std::floor(height / 2);
    const float wordWidth = text.width(words);
    const float wordLeft = at.x + std::floor((width - wordWidth) / 2);
    const Colour grip = palette(focused ? "grip" : "gripDim");
    const auto grips = [&](float from, float to) {
      for (int rule = -2; rule <= 2; rule += 2)
        ink.row(from, middle + (float)rule - 1, to - from, grip);
    };
    grips(at.x + 18, wordLeft - 5);
    grips(wordLeft + wordWidth + 5, at.x + width - (minimise ? 36 : 26));
    text.print(ink, wordLeft, middle - 3, words,
               palette(focused ? "wordmark" : "wordmarkDim"));

    const auto key = [&](float x, int glyph) {
      const float top = middle - 4;
      raised(ink, at.x + x, top, 9, 9, palette("titleLight"),
             palette("edgeLight"), palette("edgeDark"));
      const Colour mark = palette(focused ? "wordmark" : "wordmarkDim");
      if (glyph == 0) ink.rect(at.x + x + 2, top + 4, 5, 1, mark);  // options
      if (glyph == 1) ink.rect(at.x + x + 2, top + 6, 5, 1, mark);  // minimise
      if (glyph == 2) {                                             // shade
        ink.rect(at.x + x + 2, top + 2, 5, 1, mark);
        ink.rect(at.x + x + 2, top + 4, 5, 1, mark);
      }
      if (glyph == 3)  // close
        for (int step = 0; step < 5; ++step) {
          ink.px(at.x + x + 2 + (float)step, top + 2 + (float)step, mark);
          ink.px(at.x + x + 6 - (float)step, top + 2 + (float)step, mark);
        }
    };
    key(6, 0);
    if (minimise) key(width - 31, 1);
    key(width - 21, 2);
    key(width - 11, 3);
  }

  /** A skin key: the steel face under the doubled edge — light and deep
   *  shadow outside, a second line of shadow one pixel in. */
  void key(const kit::PixelInk& ink, Origin at, float x, float y, float width,
           float height) const {
    raised(ink, at.x + x, at.y + y, width, height, palette("keyFace"),
           palette("keyLight"), palette("keyDeep"));
    ink.rect(at.x + x + 1, at.y + y + height - 2, width - 2, 1,
             palette("keyShadow"));
    ink.rect(at.x + x + width - 2, at.y + y + 1, 1, height - 2,
             palette("keyShadow"));
  }

  /** A key with a lamp and its word: the window toggles and play modes. */
  void lampKey(const kit::PixelInk& ink, Origin at, float x, float y,
               float width, float height, std::string_view words,
               bool on) const {
    key(ink, at, x, y, width, height);
    const float middle = at.y + y + std::floor(height / 2);
    ink.rect(at.x + x + 3, middle - 2, 3, 3,
             palette(on ? "lampOn" : "lampOff"));
    text.print(ink, at.x + x + 8, middle - 3, words, palette("glyph"));
  }

  void labelKey(const kit::PixelInk& ink, Origin at, float x, float y,
                float width, float height, std::string_view words) const {
    key(ink, at, x, y, width, height);
    text.print(ink, at.x + x + std::floor((width - text.width(words)) / 2),
               at.y + y + std::floor((height - 5) / 2), words,
               palette("glyph"));
  }

  /** A dark well of the display, sunk into the body. */
  void well(const kit::PixelInk& ink, Origin at, float x, float y, float width,
            float height, Colour ground) const {
    sunken(ink, at.x + x, at.y + y, width, height, ground, palette("edgeLight"),
           palette("edgeDark"));
  }

  void paintMain(const kit::PixelInk& ink) const {
    const Origin at = kMain;
    body(ink, at, kWindowWidth, 116);
    titleBar(ink, at, 14, "WINAMP", true, true);

    // the clutter bar and the display
    well(ink, at, 10, 22, 8, 43, palette("screenEdge"));
    const char* clutter = "OAIDV";
    for (int letter = 0; letter < 5; ++letter)
      text.print(ink, at.x + 12, at.y + 25 + 8 * (float)letter,
                 std::string_view(clutter + letter, 1), palette("captionDim"));
    well(ink, at, 21, 21, 84, 40, palette("screen"));
    arrow(ink, at.x + 26, at.y + 28, 9, 1, palette("lampOn"));
    // the unlit segments behind the time, and its colon
    for (float x : {48.0f, 60.0f, 78.0f, 90.0f})
      numbers.digit(ink, at.x + x, at.y + 26, 8, palette("ghost"));
    ink.rect(at.x + 72, at.y + 29, 2, 2, palette("lit"));
    ink.rect(at.x + 72, at.y + 35, 2, 2, palette("lit"));
    // the analyser's dot grid, VISCOLOR's second colour on every other pixel
    for (int y = 1; y < 16; y += 2)
      for (int x = 1; x < 76; x += 2)
        ink.px(at.x + 24 + (float)x, at.y + 43 + (float)y, visual[1]);

    // the song title and the stream's numbers
    well(ink, at, 109, 23, 158, 11, palette("screen"));
    well(ink, at, 109, 41, 19, 9, palette("screen"));
    text.print(ink, at.x + 111, at.y + 43, bitrate, palette("lit"));
    text.print(ink, at.x + 130, at.y + 43, "KBPS", palette("caption"));
    well(ink, at, 152, 41, 14, 9, palette("screen"));
    text.print(ink, at.x + 154, at.y + 43, sampleRate, palette("lit"));
    text.print(ink, at.x + 168, at.y + 43, "KHZ", palette("caption"));
    text.print(ink, at.x + 212, at.y + 43, "MONO", palette("captionDim"));
    text.print(ink, at.x + 239, at.y + 43, "STEREO", palette("lit"));

    // volume at 78 %, balance at the centre: each bar is the one flat
    // colour its sprite frame is
    const float volume = 0.78f;
    well(ink, at, 107, 57, 68, 13, palette("screenEdge"));
    ink.rect(at.x + 109, at.y + 62, 64, 3,
             visual[(size_t)(17 - (int)std::lround(volume * 15))]);
    key(ink, at, 107 + std::round(volume * 54), 58, 14, 11);
    well(ink, at, 177, 57, 38, 13, palette("screenEdge"));
    ink.rect(at.x + 179, at.y + 62, 34, 3, visual[17]);
    key(ink, at, 189, 58, 14, 11);
    lampKey(ink, at, 219, 58, 23, 12, "EQ", true);
    lampKey(ink, at, 242, 58, 23, 12, "PL", true);

    // the seek trough (its thumb is live)
    well(ink, at, 16, 72, 248, 10, palette("screenEdge"));

    // the transport, each glyph drawn on its key's own pixels
    const Colour glyph = palette("glyph");
    key(ink, at, 16, 88, 23, 18);
    ink.rect(at.x + 21, at.y + 93, 2, 8, glyph);
    arrow(ink, at.x + 23, at.y + 93, 8, -1, glyph);
    arrow(ink, at.x + 28, at.y + 93, 8, -1, glyph);
    key(ink, at, 39, 88, 23, 18);
    arrow(ink, at.x + 47, at.y + 92, 10, 1, glyph);
    key(ink, at, 62, 88, 23, 18);
    ink.rect(at.x + 69, at.y + 93, 3, 8, glyph);
    ink.rect(at.x + 74, at.y + 93, 3, 8, glyph);
    key(ink, at, 85, 88, 23, 18);
    ink.rect(at.x + 93, at.y + 93, 8, 8, glyph);
    key(ink, at, 108, 88, 22, 18);
    arrow(ink, at.x + 112, at.y + 93, 8, 1, glyph);
    arrow(ink, at.x + 117, at.y + 93, 8, 1, glyph);
    ink.rect(at.x + 122, at.y + 93, 2, 8, glyph);
    key(ink, at, 136, 89, 22, 16);
    peak(ink, at.x + 147, at.y + 93, 4, glyph);
    ink.rect(at.x + 144, at.y + 98, 7, 2, glyph);
    lampKey(ink, at, 164, 89, 47, 15, "SHUFFLE", false);
    lampKey(ink, at, 210, 89, 28, 15, "REP", true);
    kit::drawSprite(ink.pen, bolt, {at.x + 253, at.y + 91});
  }

  void paintEqualizer(const kit::PixelInk& ink) const {
    const Origin at = kEqualizer;
    body(ink, at, kWindowWidth, 116);
    titleBar(ink, at, 14, "WINAMP EQUALIZER", false, false);
    lampKey(ink, at, 14, 18, 26, 12, "ON", true);
    lampKey(ink, at, 40, 18, 32, 12, "AUTO", false);
    labelKey(ink, at, 217, 18, 44, 12, "PRESETS");

    // the response graph: a well with dotted rules at +12, 0 and -12
    well(ink, at, 86, 17, 113, 19, palette("graph"));
    for (int rule : {1, 9, 17})
      for (int x = 1; x < 112; x += 2)
        ink.px(at.x + 86 + (float)x, at.y + 17 + (float)rule,
               palette("graphRule"));

    for (int slider = 0; slider < 11; ++slider)
      well(ink, at, faderLeft(slider), 38, 14, 63, palette("screen"));
    const auto caption = [&](float x, float y, std::string_view words,
                             Colour colour) {
      text.print(ink, at.x + x, at.y + y, words, colour);
    };
    caption(73 - text.width("+12DB"), 39, "+12DB", palette("caption"));
    caption(73 - text.width("+0DB"), 67, "+0DB", palette("caption"));
    caption(73 - text.width("-12DB"), 95, "-12DB", palette("caption"));
    caption(28 - std::floor(text.width("PREAMP") / 2), 104, "PREAMP",
            palette("caption"));
    for (size_t band = 0; band < bandNames.size() && band < 10; ++band)
      caption(faderLeft((int)band + 1) + 7 -
                  std::floor(text.width(bandNames[band]) / 2),
              104, bandNames[band], palette("caption"));
  }

  static float faderLeft(int slider) {
    return slider == 0 ? 21.0f : 78.0f + 18.0f * (float)(slider - 1);
  }

  void paintPlaylist(const kit::PixelInk& ink) const {
    const Origin at = kPlaylist;
    const float height = kPlaylistHeight;
    body(ink, at, kWindowWidth, height);
    titleBar(ink, at, 20, "WINAMP PLAYLIST", false, false);

    // the list: black, green, the playing track in white and the selection
    // on the skin's blue
    const float listTop = 20, listHeight = height - 58;
    sunken(ink, at.x + 11, at.y + listTop, 245, listHeight + 1,
           palette("listGround"), palette("edgeLight"), palette("edgeDark"));
    SkCanvas& canvas = *ink.pen.canvas();
    canvas.save();
    canvas.clipRect(
        SkRect::MakeXYWH(at.x + 12, at.y + listTop, 243, listHeight - 1));
    for (size_t row = 0; row < tracks.size(); ++row) {
      const float top = at.y + listTop + kRowPitch * (float)row;
      if ((int)row == selected)
        ink.rect(at.x + 12, top, 243, kRowPitch, palette("listSelected"));
      const Colour colour =
          palette((int)row == playing ? "listCurrent" : "listNormal");
      const std::string length = clockTime(tracks[row].seconds);
      const kit::Blit prose{.track = 0, .tabularDigits = false, .snap = 1};
      const float lengthWidth = kit::widthOf(listFont, length, prose);
      std::string entry = std::to_string(row + 1) + ". " + tracks[row].title;
      const float room = 243 - 8 - lengthWidth - 6;
      if (kit::widthOf(listFont, entry, prose) > room) {
        while (!entry.empty() &&
               kit::widthOf(listFont, entry + "...", prose) > room)
          entry.pop_back();
        entry += "...";
      }
      const float baseline =
          top + std::floor((kRowPitch - (float)listFont.lineHeight) / 2);
      kit::blit(ink.pen, listFont, {at.x + 14, baseline}, entry, colour, prose);
      kit::blit(ink.pen, listFont, {at.x + 252 - lengthWidth, baseline}, length,
                colour, prose);
    }
    canvas.restore();

    // the scroll rail and its grip
    well(ink, at, 259, listTop + 1, 10, listHeight - 2, palette("screenEdge"));
    key(ink, at, 260, listTop + 2, 8, 18);

    // the control strip
    const float strip = listTop + listHeight;
    const char* menus[4] = {"ADD", "REM", "SEL", "MISC"};
    for (int menu = 0; menu < 4; ++menu)
      labelKey(ink, at, 14 + 29 * (float)menu, strip + 8, 22, 18, menus[menu]);
    key(ink, at, 231, strip + 8, 22, 18);
    text.print(ink, at.x + 232, at.y + strip + 11, "LIST", palette("glyph"));
    text.print(ink, at.x + 232, at.y + strip + 18, "OPTS", palette("glyph"));
    int total = 0;
    for (const Track& track : tracks) total += track.seconds;
    const std::string totals =
        clockTime(
            tracks[(size_t)std::clamp(selected, 0, (int)tracks.size() - 1)]
                .seconds) +
        "/" +
        kit::formatted("%d:%02d:%02d", total / 3600, total / 60 % 60,
                       total % 60);
    text.print(ink, at.x + 131, at.y + strip + 10, totals,
               palette("listNormal"));
    // the small transport and the small time's window
    const Colour mark = palette("caption");
    const float row = at.y + strip + 20;
    ink.rect(at.x + 132, row, 1, 5, mark);
    arrow(ink, at.x + 133, row, 5, -1, mark);
    arrow(ink, at.x + 141, row, 5, 1, mark);
    ink.rect(at.x + 148, row, 1, 5, mark);
    ink.rect(at.x + 150, row, 1, 5, mark);
    ink.rect(at.x + 155, row, 5, 5, mark);
    arrow(ink, at.x + 164, row, 5, 1, mark);
    ink.rect(at.x + 167, row, 1, 5, mark);
    peak(ink, at.x + 175, row, 3, mark);
    ink.rect(at.x + 173, row + 4, 5, 1, mark);
    well(ink, at, 184, strip + 18, 30, 10, palette("screen"));
    // the resize grip
    for (int step = 0; step < 4; ++step)
      for (int dot = 0; dot <= step; ++dot)
        ink.px(at.x + 270 - 2 * (float)dot,
               at.y + height - 10 + 2 * (float)step, palette("edgeLight"));
  }

  // ---- the desktop -----------------------------------------------------

  /** Windows 98's control edge: white and soft grey outside, black and
   *  shadow grey inside it. */
  void plate(const kit::PixelInk& ink, float x, float y, float width,
             float height, bool pressed = false) const {
    const Colour face = palette("desktop.face");
    const Colour outerLight =
        palette(pressed ? "desktop.dark" : "desktop.light");
    const Colour outerDark =
        palette(pressed ? "desktop.light" : "desktop.dark");
    const Colour innerLight =
        palette(pressed ? "desktop.shadow" : "desktop.soft");
    const Colour innerDark =
        palette(pressed ? "desktop.soft" : "desktop.shadow");
    raised(ink, x, y, width, height, face, outerLight, outerDark);
    ink.rect(x + 1, y + 1, width - 2, 1, innerLight);
    ink.rect(x + 1, y + 1, 1, height - 2, innerLight);
    ink.rect(x + 1, y + height - 2, width - 2, 1, innerDark);
    ink.rect(x + width - 2, y + 1, 1, height - 2, innerDark);
  }

  void label(const kit::PixelInk& ink, const kit::PixFont& font, float middle,
             float y, std::string_view words, Colour colour) const {
    const float width = kit::widthOf(
        font, words, {.track = 0, .tabularDigits = false, .snap = 1});
    kit::blit(ink.pen, font, {std::floor(middle - width / 2), y}, words, colour,
              {.track = 0, .tabularDigits = false, .snap = 1});
  }

  void icon(const kit::PixelInk& ink, float x, float y, int which) const {
    const Colour face = palette("desktop.face"),
                 light = palette("desktop.light"),
                 shadow = palette("desktop.shadow"),
                 dark = palette("desktop.dark");
    if (which == 0) {  // My Computer: a monitor over its case
      raised(ink, x + 5, y + 1, 22, 19, face, light, dark);
      ink.rect(x + 8, y + 4, 16, 12, dark);
      ink.rect(x + 9, y + 5, 14, 10, palette("desktop.wallpaper"));
      ink.rect(x + 9, y + 5, 14, 2, palette("desktop.navy"));
      ink.rect(x + 13, y + 20, 6, 2, shadow);
      raised(ink, x + 2, y + 22, 28, 8, face, light, dark);
      ink.rect(x + 18, y + 25, 9, 1, dark);
      ink.rect(x + 5, y + 26, 2, 1, palette("lampOn"));
    } else if (which == 1) {  // Recycle Bin: a wire basket
      ink.rect(x + 7, y + 4, 18, 2, dark);
      ink.rect(x + 6, y + 6, 20, 2, face);
      ink.rect(x + 6, y + 6, 20, 1, light);
      for (int row = 0; row < 21; ++row) {
        const float inset = std::floor((float)row / 7);
        const float top = y + 9 + (float)row;
        ink.row(x + 7 + inset, top, 18 - 2 * inset,
                row % 3 == 2 ? shadow : face);
        for (float wire = x + 9 + inset; wire < x + 24 - inset; wire += 3)
          ink.px(wire, top, shadow);
        ink.px(x + 7 + inset, top, dark);
        ink.px(x + 24 - inset, top, dark);
      }
      ink.row(x + 10, y + 30, 12, dark);
    } else if (which == 2) {  // My Documents: a folder with paper in it
      const Colour folder = material::parseColor("#E8C850"),
                   fold = material::parseColor("#A08018");
      ink.rect(x + 3, y + 7, 11, 3, fold);
      ink.rect(x + 8, y + 4, 17, 13, light);
      for (int line = 0; line < 4; ++line)
        ink.row(x + 10, y + 6 + 2 * (float)line, 12, shadow);
      ink.rect(x + 3, y + 10, 26, 18, folder);
      ink.rect(x + 3, y + 10, 26, 1, material::parseColor("#FFF0A0"));
      ink.rect(x + 3, y + 27, 26, 1, fold);
      ink.rect(x + 28, y + 10, 1, 18, fold);
    } else {  // Winamp: the skin's slate with the bolt
      raised(ink, x + 4, y + 3, 25, 26, palette("body"), palette("edgeLight"),
             palette("edgeDark"));
      ink.rect(x + 6, y + 5, 21, 3, palette("title"));
      kit::drawSprite(ink.pen, bolt, {x + 10, y + 10});
    }
  }

  void paintDesktop(const kit::PixelInk& ink) const {
    ink.rect(0, 0, kDesktopWidth, kDesktopHeight, palette("desktop.wallpaper"));
    const char* names[4] = {"My Computer", "Recycle Bin", "My Documents",
                            "Winamp"};
    for (int which = 0; which < 4; ++which) {
      const float top = 12 + 64 * (float)which;
      icon(ink, 22, top, which);
      label(ink, desktopFont, 38, top + 35, names[which],
            palette("desktop.label"));
    }

    // the taskbar
    const float bar = kDesktopHeight - 28;
    ink.rect(0, bar, kDesktopWidth, 28, palette("desktop.face"));
    ink.row(0, bar, kDesktopWidth, palette("desktop.soft"));
    ink.row(0, bar + 1, kDesktopWidth, palette("desktop.light"));
    plate(ink, 2, bar + 4, 54, 22);
    const std::array<Colour, 4> flag{
        material::parseColor("#FF0000"), material::parseColor("#00C000"),
        material::parseColor("#0000FF"), material::parseColor("#FFFF00")};
    for (int quarter = 0; quarter < 4; ++quarter) {
      const float left = 7 + 7 * (float)(quarter % 2),
                  top = bar + 9 + 6 * (float)(quarter / 2);
      for (int row = 0; row < 5; ++row)
        ink.row(left + (row < 2 ? 1 : 0), top + (float)row, 6,
                flag[(size_t)quarter]);
    }
    kit::blit(ink.pen, desktopBold, {23, bar + 9}, "Start",
              palette("desktop.dark"),
              {.track = 0, .tabularDigits = false, .snap = 1});
    ink.rect(60, bar + 5, 1, 20, palette("desktop.shadow"));
    ink.rect(61, bar + 5, 1, 20, palette("desktop.light"));

    // the running task, pressed, on the pressed button's dither
    plate(ink, 65, bar + 4, 170, 22, true);
    for (int y = 2; y < 20; ++y)
      for (int x = 2 + y % 2; x < 168; x += 2)
        ink.px(65 + (float)x, bar + 4 + (float)y, palette("desktop.light"));
    kit::drawSprite(ink.pen, bolt, {70, bar + 8});
    std::string task =
        std::to_string(playing + 1) + ". " + nowPlaying().title + " - Winamp";
    const kit::Blit prose{.track = 0, .tabularDigits = false, .snap = 1};
    while (kit::widthOf(desktopBold, task + "...", prose) > 142 &&
           !task.empty())
      task.pop_back();
    kit::blit(ink.pen, desktopBold, {86, bar + 10}, task + "...",
              palette("desktop.dark"), prose);

    // the tray: a speaker and the clock
    const float tray = kDesktopWidth - 72;
    sunken(ink, tray, bar + 4, 70, 22, palette("desktop.face"),
           palette("desktop.light"), palette("desktop.shadow"));
    ink.rect(tray + 6, bar + 12, 3, 5, palette("desktop.dark"));
    arrow(ink, tray + 9, bar + 9, 11, -1, palette("desktop.dark"));
    const float clockWidth = kit::widthOf(desktopFont, desktopClock, prose);
    kit::blit(ink.pen, desktopFont, {tray + 64 - clockWidth, bar + 9},
              desktopClock, palette("desktop.dark"), prose);
  }

  void paintSkin(draw::Pen& pen) const {
    const kit::PixelInk ink{pen};
    paintDesktop(ink);
    paintMain(ink);
    paintEqualizer(ink);
    paintPlaylist(ink);
  }

  // =========================================================================
  // WHAT THE PLAYER REDRAWS

  /** The time: four NUMBERS.BMP cells, the tens of minutes dark below ten. */
  void paintTime(draw::Pen& pen) const {
    const kit::PixelInk ink{pen};
    const int minutes = second / 60 % 100, seconds = second % 60;
    if (minutes >= 10) numbers.digit(ink, 0, 0, minutes / 10, palette("lit"));
    numbers.digit(ink, 12, 0, minutes % 10, palette("lit"));
    numbers.digit(ink, 30, 0, seconds / 10, palette("lit"));
    numbers.digit(ink, 42, 0, seconds % 10, palette("lit"));
  }

  /** The analyser well, 76 x 16: nineteen bars three pixels wide, each
   *  pixel in the VISCOLOR entry of its row, a grey cap riding each; or
   *  the oscilloscope's trace, brightest where it crosses the middle. */
  void paintVisualiser(draw::Pen& pen) const {
    const kit::PixelInk ink{pen};
    if (!oscilloscope) {
      for (int column = 0; column < kBars; ++column) {
        const int lit = (int)std::lround(bar[(size_t)column]);
        for (int row = 16 - lit; row < 16; ++row)
          ink.rect(4 * (float)column, (float)row, 3, 1,
                   visual[(size_t)(2 + row)]);
        const int capRow = 16 - (int)std::lround(cap[(size_t)column]);
        if (capRow < 16)
          ink.rect(4 * (float)column, (float)std::max(0, capRow - 1), 3, 1,
                   visual[23]);
      }
      return;
    }
    int previous = (int)std::lround(wave[0]);
    for (int x = 0; x < 76; ++x) {
      const int row = (int)std::lround(wave[(size_t)x]);
      const int from = std::min(previous, row), to = std::max(previous, row);
      for (int y = from; y <= to; ++y) {
        const int away = std::min(4, std::abs(y - 8) / 2);
        ink.px((float)x, (float)y, visual[(size_t)(18 + away)]);
      }
      previous = row;
    }
  }

  /** The eleven faders, each its trough's bar in the flat colour of the
   *  frame its level picks and its thumb at that level. */
  void paintFaders(draw::Pen& pen) const {
    const kit::PixelInk ink{pen};
    for (int slider = 0; slider < 11; ++slider) {
      const float left = faderLeft(slider) - faderLeft(0);
      const float level = (1 - gain[(size_t)slider]) / 2;  // 0 at +12 dB
      const Colour colour =
          palette("fader." + std::to_string((int)std::lround(level * 5)));
      ink.rect(left + 5, 2, 4, 59, colour);
      ink.rect(left + 5, 2, 1, 59, palette("screen"));
      const float thumb = std::round(level * 51) + 1;
      raised(ink, left + 1, thumb, 12, 11, palette("keyFace"),
             palette("keyLight"), palette("keyDeep"));
      ink.rect(left + 3, thumb + 5, 8, 1, palette("keyShadow"));
    }
  }

  /** The response graph: the preamp as a flat rule, and the ten bands as
   *  one Catmull-Rom curve, each pixel in the colour of its height. */
  void paintCurve(draw::Pen& pen) const {
    const kit::PixelInk ink{pen};
    constexpr int kWidth = 111, kMiddle = 8;
    ink.row(0, (float)(kMiddle - (int)std::lround(gain[0] * 7)), kWidth,
            palette("preamp"));
    const auto band = [&](int index) {
      return gain[(size_t)std::clamp(index, 0, 9) + 1];
    };
    int previous = -1;
    for (int x = 0; x < kWidth; ++x) {
      const float along = (float)x / (float)(kWidth - 1) * 9;
      const int index = std::min(8, (int)along);
      const float part = along - (float)index;
      const float p0 = band(index - 1), p1 = band(index), p2 = band(index + 1),
                  p3 = band(index + 2);
      const float value =
          0.5f * (2 * p1 + (-p0 + p2) * part +
                  (2 * p0 - 5 * p1 + 4 * p2 - p3) * part * part +
                  (-p0 + 3 * p1 - 3 * p2 + p3) * part * part * part);
      const int row =
          std::clamp(kMiddle - (int)std::lround(value * 7.5f), 0, 16);
      const int from = previous < 0 ? row : std::min(previous, row);
      const int to = previous < 0 ? row : std::max(previous, row);
      for (int y = from; y <= to; ++y)
        ink.px((float)x, (float)y, visual[(size_t)(2 + y * 15 / 16)]);
      previous = row;
    }
  }

  void paintSmallTime(draw::Pen& pen) const {
    const kit::PixelInk ink{pen};
    text.print(ink, 0, 0,
               kit::formatted("%2d:%02d", second / 60 % 100, second % 60),
               palette("listNormal"));
  }

  // =========================================================================
  // THE SIGNAL — a plausible four-to-the-floor track, and the state the
  // player computes from it once per frame

  void advance(double time) {
    const double step = std::max(0.0, time - lastStep);
    lastStep = time;
    second = (int)(startSecond + time);

    for (const ModeChange& change : modes)
      if (time >= change.at) oscilloscope = change.oscilloscope;

    for (size_t slider = 0; slider < gain.size(); ++slider)
      gain[slider] = gainAt(slider, time);
    const auto [x, y] = pointerAt(time);
    pointerX = x;
    pointerY = y;

    // the beat: a kick on every beat, a snare on two and four, a hat on
    // every off-beat, all as decaying envelopes
    const double beat = 60.0 / tempo;
    const double sinceBeat = std::fmod(time, beat);
    const long beatIndex = (long)std::floor(time / beat);
    const float kick = (float)std::exp(-sinceBeat * 9.0);
    const float snare =
        beatIndex % 2 == 1 ? (float)std::exp(-sinceBeat * 11.0) : 0.0f;
    const float hat = (float)std::exp(-std::fmod(time + beat / 2, beat) * 16.0);
    const int tick = (int)std::floor(time * 30);
    const int note = (int)std::floor(time / (beat / 2));

    for (int column = 0; column < kBars; ++column) {
      const float along = (float)column / (float)(kBars - 1);
      const float grain = coin(column, tick, 7);
      float level = 0;
      if (column < 5)
        level = 0.56f + 0.44f * kick * (1 - along * 1.2f) +
                0.2f * coin(note, column / 3, 3);
      else if (column < 13)
        level = 0.44f + 0.34f * snare +
                0.3f * coin(note, column, 5) * (1 - along * 0.4f);
      else
        level = 0.26f + 0.52f * hat * (0.6f + 0.4f * along) + 0.24f * snare;
      level = std::clamp(level * (0.86f + 0.28f * grain), 0.0f, 1.0f);
      const float target = level * 16;
      float& height = bar[(size_t)column];
      height = std::max(target, height - (float)(step * 40.0));
      float& top = cap[(size_t)column];
      float& speed = capSpeed[(size_t)column];
      if (height >= top) {
        top = height;
        speed = 0;
      } else {
        speed += (float)(step * 30.0);
        top = std::max(height, top - speed * (float)step);
      }
    }

    // the oscilloscope: the bass line under the kick, a lead above it
    const double phase = time * 0.37;
    for (int x = 0; x < 76; ++x) {
      const double along = (double)x / 75.0;
      const float sample =
          (float)(0.62 * (0.35 + 0.65 * kick) *
                      std::sin(6.2832 * (along * 1.7 + phase)) +
                  0.24 * std::sin(6.2832 * (along * 6.3 + time * 1.9)) +
                  0.14 * hat * (coin(x, tick, 11) * 2 - 1));
      wave[(size_t)x] = std::clamp(8.0f - sample * 7.5f, 0.0f, 15.0f);
    }

    // the title runs in whole pixels, and so does the thumb
    const float titleWidth = text.advance * (float)marquee().size();
    const double run = std::max(0.0, time - scrollFrom) * scrollSpeed;
    scrollOffset = -(float)std::fmod(std::floor(run), (double)titleWidth);
    const double position =
        std::fmod(startSecond + time, (double)nowPlaying().seconds) /
        (double)nowPlaying().seconds;
    seekOffset = (float)std::round(position * (248 - 29));
  }

  // =========================================================================

  /** A live drawing placed on the desktop grid, relative to a window. */
  static Element live(Origin at, float x, float y, float width, float height,
                      Element drawing) {
    return kit::at(at.x + x, at.y + y, width, height)
        .children({std::move(drawing)});
  }

  Element describe() {
    const float titleWidth = text.advance * (float)marquee().size();
    return stack()
        .width(kDesktopWidth * kSkinPixel)
        .height(kDesktopHeight * kSkinPixel)
        .children({
            stack()
                .width(kDesktopWidth)
                .height(kDesktopHeight)
                .transformOrigin(pct(0), pct(0))
                .scale(kSkinPixel)
                .children({
                    pen(
                        "winamp-skin",
                        [this](draw::Pen& pen) { paintSkin(pen); },
                        Cache::Texture),
                    live(kMain, 48, 26, 51, 13,
                         pen([this](draw::Pen& pen) { paintTime(pen); })),
                    live(kMain, 24, 43, 76, 16,
                         pen([this](draw::Pen& pen) { paintVisualiser(pen); })),
                    live(kMain, 111, 26, 154, 7,
                         kit::at(0, 1, 2 * titleWidth, 6)
                             .translateX(scrollOffset)
                             .children({pen(
                                 "winamp-title",
                                 [this, titleWidth](draw::Pen& pen) {
                                   const kit::PixelInk ink{pen};
                                   const std::string words = marquee();
                                   text.print(ink, 0, 0, words, palette("lit"));
                                   text.print(ink, titleWidth, 0, words,
                                              palette("lit"));
                                 },
                                 Cache::Picture)}))
                        .overflow(Overflow::Clip),
                    live(kMain, 16, 72, 29, 10,
                         pen(
                             "winamp-seek-thumb",
                             [this](draw::Pen& pen) {
                               const kit::PixelInk ink{pen};
                               raised(ink, 0, 0, 29, 10, palette("keyFace"),
                                      palette("keyLight"), palette("keyDeep"));
                               ink.rect(1, 8, 27, 1, palette("keyShadow"));
                               ink.rect(13, 2, 1, 6, palette("keyShadow"));
                               ink.rect(15, 2, 1, 6, palette("keyLight"));
                             },
                             Cache::Picture))
                        .translateX(seekOffset),
                    live(kEqualizer, faderLeft(0), 38,
                         faderLeft(10) + 14 - faderLeft(0), 63,
                         pen([this](draw::Pen& pen) { paintFaders(pen); })),
                    live(kEqualizer, 87, 18, 111, 17,
                         pen([this](draw::Pen& pen) { paintCurve(pen); })),
                    live(kPlaylist, 187, kPlaylistHeight - 38 + 20, 25, 6,
                         pen([this](draw::Pen& pen) { paintSmallTime(pen); })),
                    kit::at(0, 0, 12, 19)
                        .translateX(pointerX)
                        .translateY(pointerY)
                        .children({pen(
                            "winamp-pointer",
                            [this](draw::Pen& pen) {
                              kit::drawSprite(pen, cursor, {0, 0});
                            },
                            Cache::Picture)}),
                }),
        });
  }

  void setup(sketch::SketchContext& context) {
    // The capture sits on the spectrum at full swing, a beat after the
    // treble slider was pulled up and before the visualiser changes mode.
    sketch::kit::stage(context, {.size = {kDesktopWidth * kSkinPixel,
                                          kDesktopHeight * kSkinPixel},
                                 .captureAt = 7.4,
                                 .background = material::parseColor("#008080"),
                                 .oversample = 2});
    read(context);
    const auto bake = [&](std::initializer_list<const char*> families,
                          float size, int weight) {
      return kit::bakeFont(
          *context.fonts,
          weave::Type{.face = weave::ports::face(families, weight),
                      .size = size,
                      .color = Colour{1, 1, 1, 1},
                      .aliased = true});
    };
    // The playlist is the one place the skin sets a system face — Arial,
    // unsmoothed, as the list was drawn; the desktop is Windows 98's.
    listFont = bake({"Arial", "Helvetica"}, 11, 400);
    desktopFont = bake({"Tahoma", "Verdana", "Geneva"}, 11, 400);
    desktopBold = bake({"Tahoma", "Verdana", "Geneva"}, 11, 700);

    advance(0);
    context.engine.timer([this](motion::Duration, motion::Duration elapsed) {
      advance(elapsed.count());
    });
    context.composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH(WinampBase, "Study · Screens",
             "Winamp 2's base skin on a Windows 98 desktop, drawn as pixel art "
             "at three to one — playing, with the analyser, the equaliser and "
             "the playlist live")
