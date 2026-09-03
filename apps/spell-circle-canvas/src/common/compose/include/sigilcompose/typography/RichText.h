#pragma once

/** @file
 * SigilCompose typography — MIXED TEXT as one value: `RichText`, the
 * runs-and-styles value `text()` takes and `rich()` starts, and `Story`,
 * a text plus its block styles filled into as many frames as it is
 * given.
 *
 * A paragraph whose words are not all set the same way is a VALUE here,
 * not a document to be marked up. There is no markup language: a run of
 * text carries a style, or the name of one, and that is the whole
 * vocabulary. Whatever else a passage needs — a colour on the numbers, a
 * weight on one phrase — is asked for by SELECTOR after the fact
 * (`Element::spanPaint` / `Element::spanStyle`), so the content stays
 * content and the type treatment stays in one place.
 *
 * The escape hatch is unchanged: `text(std::shared_ptr<Paragraph>,
 * options)` hands the engine a document built by hand, for the passage
 * too custom for either of these.
 */

#include <include/core/SkSize.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/style/StyleSet.h>
#include <sigilweave/style/TextStyle.h>

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::compose {

/** MIXED-STYLE TEXT AS A COMPARABLE VALUE — the builder `text()` takes.
 *
 *      auto p = rich(base)
 *                   .add(u8"Signal ")
 *                   .add(u8"woven", accent)
 *                   .add(u8" through ")
 *                   .add(u8"noise", mono);
 *      text(p).fx({.effect = fx::slide(-60),
 *                  .stagger = {.eachMs = 120}, .over = unit::Word});
 *
 *  `add(utf8)` sets a run in the base style; `add(utf8, style)` sets it in
 *  its own; `add(utf8, name)` sets it in a style looked up by NAME (see
 *  below). Runs are appended in order and concatenate into one paragraph —
 *  nothing is inserted between them, so the spaces are the author's.
 *
 *  WHY IT IS A VALUE, and what that buys over the `shared_ptr<Paragraph>`
 *  overload: two rich texts describing the same runs in the same styles are
 *  EQUAL, so a component that rebuilds its text every describe prunes
 *  exactly like a static leaf. The pointer overload cannot answer that
 *  question — a fresh `make_shared` is a fresh identity and reads as
 *  changed content every time. Internally the runs materialize into one
 *  weave `Paragraph` on the instance, rebuilt only when the described value
 *  changes; the shaping cache is content-addressed, so a rebuild that
 *  changed one run re-shapes one run.
 *
 *  NAMES resolve through a `weave::StyleSet`, which comes from one of two
 *  places: `styles()` supplies one explicitly, and `env::Provide<StyleSet>`
 *  supplies one ambiently to everything described in its scope. AN EXPLICIT
 *  SET ALWAYS WINS, whichever order the two are written in. A name the set
 *  does not register resolves to the base handed to `rich()` — the base is
 *  this text's one default, and a misspelled name shows as content set in
 *  it rather than as content that did not draw.
 *
 *  Resolution happens as the run is added (and again over every named run
 *  when `styles()` arrives), so the finished value holds real styles and
 *  depends on no scope that has since ended. */
class RichText {
 public:
  /** One run of text and the style it is set in — or one INLINE SLOT, which
   *  is a run whose content is the single object-replacement character the
   *  flow anchors a reserved box at. */
  struct Run {
    std::u8string utf8;
    sigil::weave::TextStyle style;  ///< resolved: its own, its name's, or base
    std::string styleName;          ///< the name it was written with, if any
    /** Non-empty on a SLOT run: the name a child of this text node is laid
     *  out into. */
    std::string slotKey;
    SkSize slotSize = {0, 0};    ///< the box the breakers reserve
    float slotBaselineDrop = 0;  ///< the box's bottom, below the baseline
    bool operator==(const Run&) const = default;
  };

  RichText() = default;
  /** Starts a value whose unstyled runs — and unregistered names — are set
   *  in @p baseStyle. */
  explicit RichText(sigil::weave::TextStyle baseStyle)
      : m_base(std::move(baseStyle)) {}

  /** Appends a run in the base style. */
  RichText& add(std::u8string_view utf8);
  /** Appends a run in its own style. */
  RichText& add(std::u8string_view utf8, sigil::weave::TextStyle style);
  /** Appends a run in the style registered under @p styleName. */
  RichText& add(std::u8string_view utf8, std::string_view styleName);

  /** Reserves an INLINE SLOT: `size` px of blank space woven into the flow,
   *  and the name a child Element of this text node is laid out into.
   *
   *      text(rich(body).add(u8"press ").slot("key", {28, 18}).add(u8" now"))
   *          .child(box().key("key").fill(ink).corners({4}))
   *
   *  The reserved box is ONE UNBREAKABLE WORD: a line never breaks inside
   *  it, and a box taller than the type opens the lines of its BLOCK. The
   *  room is in the strut before anything is broken, because bands are
   *  asked of the geometry before anyone knows which words land on them —
   *  a depth found afterwards would be a depth decided after the break it
   *  decides.
   *  `baselineDrop` is how far the box's BOTTOM sits below the baseline —
   *  0 stands it on the baseline like an inline image, and about the face's
   *  descent centres a pill on the x-height.
   *
   *  The child is an ordinary subtree: it animates, caches and hit-tests
   *  like any other element, and it re-lands wherever the placeholder lands
   *  when the text reflows. It is a POSITIONED subtree — the placeholder
   *  rect is its box, so flex layout does not run inside it and its own
   *  children take explicit rects, exactly as under `positioned()`.
   *
   *  A TEXT SLOT IS NOT A MOUNT SLOT. `slot()` and `Composer::renderSlot`
   *  name a hole a HOST fills from outside the description, and those names
   *  live in one registry for the whole composition. These names live in
   *  this rich-text value alone and are matched against the `key()` of this
   *  text node's own children — so two captions may both reserve a slot
   *  called "icon" without colliding, and neither is reachable by
   *  `renderSlot`. A child keyed for a slot the content does not declare
   *  draws nothing, and says so once. */
  RichText& slot(std::string key, SkSize size, float baselineDrop = 0);
  /** Supplies the style set names resolve through, beating any the
   *  environment offers, and re-resolves every named run already added. */
  RichText& styles(sigil::weave::StyleSet set);

  /** The style unstyled runs and unregistered names are set in. */
  [[nodiscard]] const sigil::weave::TextStyle& base() const { return m_base; }
  /** The runs, in the order they were added. */
  [[nodiscard]] std::span<const Run> runs() const { return m_runs; }
  [[nodiscard]] bool empty() const { return m_runs.empty(); }

  /** Equal when the base, the runs, their resolved styles and the names
   *  they were written with all match — the question the prune asks.
   *
   *  The style SET is deliberately not compared: a name is resolved as it
   *  is added, so two values that resolved to the same styles describe the
   *  same paragraph however they got there, and an entry neither of them
   *  names cannot make them differ. */
  bool operator==(const RichText& other) const {
    return m_base == other.m_base && m_runs == other.m_runs;
  }

 private:
  sigil::weave::TextStyle m_base;
  std::vector<Run> m_runs;
  sigil::weave::StyleSet m_styles;
  bool m_hasStyles = false;       // a set is in play (explicit or inherited)
  bool m_stylesExplicit = false;  // styles() gave it; env cannot replace it
};

/** Starts a mixed-text value whose default is @p base — see RichText. */
[[nodiscard]] RichText rich(sigil::weave::TextStyle base = {});

/** A TEXT FILLED INTO AS MANY FRAMES AS IT IS GIVEN.
 *
 *      Story article(rich(body).add(u8"…"));
 *      article.paragraphs({heading, para, para});
 *
 *      root.child(frame(article).key("a").thread("b").width(Dim(300)))
 *          .child(frame(article).key("b").thread("c").width(Dim(300)))
 *          .child(frame(article).key("c").width(Dim(300)).ellipsis(u8"…"));
 *
 *  A story is CONTENT PLUS ITS BLOCK STYLES and nothing else — it holds no
 *  layout, no cursor and no frame. Each frame of a chain fills from where
 *  the one before it stopped, and the cut moves as any frame's measure
 *  moves. The BLOCKS ARE NUMBERED FROM THE STORY'S START, so the third
 *  block is set the same way whichever frame it happens to land in.
 *
 *  Pitch, writing mode and block styles are the story's and a frame cannot
 *  override them: a frame that wants a different pitch is a different
 *  story. What a frame decides is its own geometry — its box, its
 *  exclusions, a silhouette it flows around — and whether it is the last,
 *  which is the one that sets an ellipsis. Overflow on any other frame is
 *  the normal case and draws nothing.
 *
 *  It is a VALUE: two stories describing the same runs in the same styles
 *  are equal, so a component that rebuilds its story every describe prunes
 *  exactly like a static leaf. */
class Story {
 public:
  Story() = default;
  /** A story over mixed content. */
  explicit Story(RichText content) : m_content(std::move(content)) {}
  /** A story over one styled string. */
  Story(std::u8string utf8, sigil::weave::TextStyle style)
      : m_content(RichText(std::move(style))) {
    m_content.add(m_contentScratch = std::move(utf8));
  }

  /** How each BLOCK of the story is set, in block order — the same value
   *  `Element::paragraphs` takes, and stated once for every frame. */
  Story& paragraphs(std::vector<sigil::weave::ParagraphStyle> blocks) {
    m_blocks = std::move(blocks);
    return *this;
  }

  [[nodiscard]] const RichText& content() const { return m_content; }
  [[nodiscard]] std::span<const sigil::weave::ParagraphStyle> blocks() const {
    return m_blocks;
  }
  [[nodiscard]] bool empty() const { return m_content.empty(); }

  bool operator==(const Story& other) const {
    return m_content == other.m_content && m_blocks == other.m_blocks;
  }

 private:
  RichText m_content;
  std::u8string m_contentScratch;
  std::vector<sigil::weave::ParagraphStyle> m_blocks;
};

}  // namespace sigil::compose
