/** @file
 * Bounded paragraph reuse behind the text service. Boost's node map keeps
 * entry keys stable while std::list maintains recency without scanning.
 * Skia's paragraph cache holds SkParagraph internals; these entries retain
 * SigilWeave's own editable analysis and isolate it from published results.
 */

#include "sigilweave/layout/TextContext.h"

#include <boost/container_hash/hash.hpp>
#include <boost/unordered/unordered_node_map.hpp>
#include <list>
#include <string>

#include "sigilweave/unicode/Unicode.h"

namespace sigil::weave {
namespace {

struct TextKey {
  std::u16string text;
  ShapingStyle style;
};

struct TextKeyView {
  std::u16string_view text;
  const ShapingStyle& style;
};

TextKeyView viewOf(const TextKey& key) { return {key.text, key.style}; }
TextKeyView viewOf(TextKeyView key) { return key; }

struct TextKeyHash {
  using is_transparent = void;
  template <typename Key>
  size_t operator()(const Key& source) const {
    const auto key = viewOf(source);
    size_t hash = boost::hash_range(key.text.begin(), key.text.end());
    const ShapingStyle& style = key.style;
    boost::hash_combine(hash, style.typeface.get());
    boost::hash_combine(hash, style.fontSize);
    boost::hash_combine(hash, style.letterSpacing);
    boost::hash_combine(hash, style.scaleX);
    boost::hash_combine(hash, style.wordSpacing);
    boost::hash_combine(hash, style.languageTag);
    boost::hash_combine(hash, style.textTransform);
    boost::hash_combine(hash, style.verticalForm);
    boost::hash_combine(hash, style.aliased);
    boost::hash_combine(hash, style.opticalKerning);
    for (const auto& feature : style.fontFeatures) {
      boost::hash_range(hash, std::begin(feature.tag), std::end(feature.tag));
      boost::hash_combine(hash, feature.value);
    }
    for (const auto& variation : style.variations) {
      boost::hash_range(hash, std::begin(variation.tag),
                        std::end(variation.tag));
      boost::hash_combine(hash, variation.value);
    }
    return hash;
  }
};

struct TextKeyEqual {
  using is_transparent = void;
  template <typename Left, typename Right>
  bool operator()(const Left& a, const Right& b) const {
    const auto left = viewOf(a);
    const auto right = viewOf(b);
    return left.text == right.text && left.style == right.style;
  }
};

}  // namespace

struct TextContext::Impl {
  struct Entry {
    std::shared_ptr<Paragraph> paragraph;
    std::list<const TextKey*>::iterator recency;
  };

  std::unique_ptr<FontContext> ownedFonts;
  FontContext* fonts;
  TextContextOptions options;
  boost::unordered_node_map<TextKey, Entry, TextKeyHash, TextKeyEqual> entries;
  std::list<const TextKey*> recent;
  Stats stats;

  Impl(FontContext& service, TextContextOptions settings)
      : fonts(&service), options(settings) {}

  std::shared_ptr<Paragraph> build(std::u16string_view text,
                                   const TextStyle& style) {
    auto paragraph = std::make_shared<Paragraph>();
    paragraph->appendText(text, style);
    ++stats.paragraphBuilds;
    return paragraph;
  }

  std::shared_ptr<Paragraph> acquire(std::u16string_view text,
                                     const TextStyle& style) {
    // A NaN-bearing style cannot identify even itself in an equality map.
    if (options.paragraphCacheEntries == 0 || !(style.shaping == style.shaping))
      return build(text, style);
    auto found = entries.find(TextKeyView{text, style.shaping});
    if (found == entries.end()) {
      auto paragraph = build(text, style);
      if (entries.size() >= options.paragraphCacheEntries) {
        entries.erase(*recent.back());
        recent.pop_back();
      }
      found = entries
                  .emplace(TextKey{std::u16string(text), style.shaping},
                           Entry{std::move(paragraph), {}})
                  .first;
      recent.push_front(&found->first);
      found->second.recency = recent.begin();
    } else {
      ++stats.paragraphCacheHits;
      recent.splice(recent.begin(), recent, found->second.recency);
      // Layout can change segmentation or extend lazy shaping. Published
      // results retain the old paragraph; build a fresh identity before
      // any mutation rather than copying a document's identity/edit log.
      if (found->second.paragraph.use_count() > 1)
        found->second.paragraph = build(text, style);
      else
        found->second.paragraph->setPaint(0, static_cast<uint32_t>(text.size()),
                                          style.paint);
    }
    return found->second.paragraph;
  }
};

TextContext::TextContext(sk_sp<SkFontMgr> fontManager,
                         TextContextOptions options,
                         sk_sp<SkTypeface> defaultTypeface,
                         FontContext::FallbackResolver fallbackResolver) {
  auto fonts = std::make_unique<FontContext>(std::move(fontManager),
                                             std::move(defaultTypeface),
                                             std::move(fallbackResolver));
  m_impl = std::make_unique<Impl>(*fonts, options);
  m_impl->ownedFonts = std::move(fonts);
}

TextContext::TextContext(FontContext& fonts, TextContextOptions options)
    : m_impl(std::make_unique<Impl>(fonts, options)) {}
TextContext::~TextContext() = default;
FontContext& TextContext::fonts() { return *m_impl->fonts; }

float TextContext::naturalWidth(std::u8string_view text,
                                const TextStyle& style) {
  return naturalWidth(unicode::toUtf16(text), style);
}
float TextContext::naturalWidth(std::u16string_view text,
                                const TextStyle& style) {
  auto paragraph = m_impl->acquire(text, style);
  paragraph->setSoftHyphenBreaks(true);
  paragraph->setHyphenator(nullptr, {});
  paragraph->setKinsoku({});
  return paragraph->naturalWidth(fonts());
}

TextLayout TextContext::layout(std::u8string_view text, const TextStyle& style,
                               FlowGeometry& flow,
                               const ParagraphLayoutOptions& options) {
  return layout(unicode::toUtf16(text), style, flow, options);
}
TextLayout TextContext::layout(std::u16string_view text, const TextStyle& style,
                               FlowGeometry& flow,
                               const ParagraphLayoutOptions& options) {
  auto paragraph = m_impl->acquire(text, style);
  auto result = layoutParagraph(fonts(), *paragraph, flow, options);
  return TextLayout(std::move(paragraph), std::move(result));
}

TextLayout TextContext::singleLine(std::u8string_view text,
                                   const TextStyle& style, SkPoint baseline,
                                   const PathTextOptions& options) {
  return singleLine(unicode::toUtf16(text), style, baseline, options);
}
TextLayout TextContext::singleLine(std::u16string_view text,
                                   const TextStyle& style, SkPoint baseline,
                                   const PathTextOptions& options) {
  auto paragraph = m_impl->acquire(text, style);
  paragraph->setSoftHyphenBreaks(true);
  paragraph->setHyphenator(nullptr, {});
  paragraph->setKinsoku({});
  auto result = layoutSingleLine(fonts(), *paragraph, baseline, options);
  return TextLayout(std::move(paragraph), std::move(result));
}

void TextContext::purgeParagraphs() {
  m_impl->recent.clear();
  m_impl->entries.clear();
}
TextContext::Stats TextContext::stats() const {
  Stats stats = m_impl->stats;
  stats.paragraphEntries = m_impl->entries.size();
  return stats;
}
void TextContext::resetStats() { m_impl->stats = {}; }

}  // namespace sigil::weave
