/** @file
 * A rule's partials, and the sheet that holds rules in order: how a
 * statement of a literal flattens, how two sheets join, and how two
 * sheets compare.
 */

#include <sigilcompose/core/StyleSheet.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>

#include <algorithm>
#include <utility>

#include "SelectorInternal.h"
#include "SheetBody.h"

namespace sigil::compose {

// ---------------------------------------------------------------------------
// A rule

Rule& Rule::font(sigil::weave::Type partial) {
  // Later wins field by field, exactly as the verb on an element does.
  sigil::weave::merge(m_type, partial);
  // A colour written here is the ink, so a property the ink was read
  // from before no longer stands.
  if (partial.color) {
    m_inkVar.reset();
    m_inkPaint.reset();
    m_statesInk = true;
  }
  return *this;
}

Rule& Rule::block(sigil::weave::Block partial) {
  sigil::weave::merge(m_block, partial);
  return *this;
}

Rule& Rule::ink(material::Color colour) {
  m_type.color = material::skia::toSkColor(colour);
  m_inkVar.reset();
  m_inkPaint.reset();
  m_statesInk = true;
  return *this;
}

Rule& Rule::ink(VarRef reference) {
  m_inkVar = reference;
  m_type.color.reset();
  m_inkPaint.reset();
  m_statesInk = true;
  return *this;
}

Rule& Rule::ink(SurfacePaint paint, PaintAnchor anchor) {
  // A PLAIN COLOUR is the ink lane as it has always been. A paint that
  // happens to be flat is not one: it overrides the glyphs of a leaf set
  // in a style of its own, which an inherited colour does not reach.
  const std::optional<Fill> flat =
      paint.writtenAsPaint() ? std::nullopt : paint.collapsedFill();
  if (flat && flat->kind == Fill::Kind::Color && !flat->references())
    return ink(flat->colorValue);
  if (paint.none()) {
    m_statesInk = true;
    m_inkPaint.reset();
    m_inkAnchor = anchor;
    return *this;
  }
  // Nothing is written until there is something to write, so a fill the
  // slot cannot hold leaves a standing paint where it is.
  std::optional<material::skia::Paint> stored = paint.collapsedPaint();
  if (!stored) return *this;
  m_statesInk = true;
  m_inkPaint = std::move(stored);
  m_inkAnchor = anchor;
  return *this;
}

Rule& Rule::var(std::string_view name, material::Color colour) {
  m_vars.set(compose::var(name), colour);
  return *this;
}

Rule& Rule::var(std::string_view name, Dimension length) {
  m_vars.set(compose::var(name), length);
  return *this;
}

Rule rule(std::string_view cssText) { return Rule(selector(cssText)); }

Rule rule(ElementSelector subject) { return Rule(std::move(subject)); }

// ---------------------------------------------------------------------------
// A sheet's compiled form

namespace detail {
namespace {

/** Every name a `:has()` argument inside @p value tests for, found
 *  wherever the `:has()` stands — in a compound of the chain, or inside
 *  the arguments of `:is`, `:where`, `:not` or an `of` filter. Only the
 *  names an argument's compounds state outright are needed: they are
 *  what the summary can rule out. */
void collectHasNames(const ElementSelector& value, SheetBody& body) {
  for (const ElementSelector& alternative :
       SelectorAccess::alternatives(value))
    for (const Step& step : SelectorAccess::asSteps(alternative))
      for (const Simple& simple : step.compound.simples) {
        if (simple.kind != SimpleKind::Has) {
          for (const ElementSelector& argument : simple.arguments)
            collectHasNames(argument, body);
          continue;
        }
        body.usesHas = true;
        for (const ElementSelector& relative : simple.arguments)
          for (const Step& inner : SelectorAccess::asSteps(relative))
            for (const Simple& named : inner.compound.simples) {
              if (named.kind != SimpleKind::StyleClass &&
                  named.kind != SimpleKind::Role)
                continue;
              HasName one{named.kind == SimpleKind::StyleClass, named.name};
              if (std::find(body.hasNames.begin(), body.hasNames.end(),
                            one) == body.hasNames.end())
                body.hasNames.push_back(std::move(one));
            }
      }
}

}  // namespace

SheetBody::SheetBody(std::vector<Rule> stated) : rules(std::move(stated)) {
  for (uint32_t at = 0; at < (uint32_t)rules.size(); ++at) {
    for (const ElementSelector& alternative :
         SelectorAccess::alternatives(rules[at].selector())) {
      const std::vector<Step> steps = SelectorAccess::asSteps(alternative);
      if (steps.empty()) continue;
      const std::vector<Simple>& subject = steps.back().compound.simples;
      const auto named = [&](SimpleKind kind) -> const Simple* {
        for (const Simple& simple : subject)
          if (simple.kind == kind) return &simple;
        return nullptr;
      };
      std::vector<uint32_t>* bucket = &anyElement;
      if (const Simple* styleClass = named(SimpleKind::StyleClass))
        bucket = &byClass[styleClass->name];
      else if (const Simple* role = named(SimpleKind::Role))
        bucket = &byRole[role->name];
      // A rule two of whose alternatives land in one bucket is filed
      // there once.
      if (bucket->empty() || bucket->back() != at) bucket->push_back(at);
    }
    collectHasNames(rules[at].selector(), *this);
  }
}

}  // namespace detail

// ---------------------------------------------------------------------------
// A sheet

StyleSheet::Statement::Statement(Rule one) {
  m_rules.push_back(std::move(one));
}

StyleSheet::Statement::Statement(const StyleSheet& included)
    : m_rules(included.rules()) {}

StyleSheet::StyleSheet(std::initializer_list<Statement> statements) {
  std::vector<Rule> flat;
  size_t total = 0;
  for (const Statement& statement : statements)
    total += statement.rules().size();
  if (total == 0) return;
  flat.reserve(total);
  for (const Statement& statement : statements)
    flat.insert(flat.end(), statement.rules().begin(), statement.rules().end());
  m_body = std::make_shared<const detail::SheetBody>(std::move(flat));
}

const std::vector<Rule>& StyleSheet::rules() const {
  // A sheet stating nothing holds no storage at all, so the empty
  // answer is one every empty sheet shares.
  static const std::vector<Rule>* const none = new std::vector<Rule>;
  return m_body ? m_body->rules : *none;
}

bool StyleSheet::operator==(const StyleSheet& other) const {
  // One value applied at ten subtrees is one pointer, which is the
  // comparison a reconcile makes over and over.
  if (m_body == other.m_body) return true;
  return rules() == other.rules();
}

StyleSheet operator+(const StyleSheet& earlier, const StyleSheet& later) {
  if (earlier.empty()) return later;
  if (later.empty()) return earlier;
  return StyleSheet{earlier, later};
}

}  // namespace sigil::compose
