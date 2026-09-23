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

#include "ComposeCompare.h"
#include "ComposeInternal.h"
#include "SelectorInternal.h"
#include "SheetBody.h"

namespace sigil::compose {

// ---------------------------------------------------------------------------
// A rule

Rule::Rule(ElementSelector subject) : m_selector(std::move(subject)) {}

Rule& Rule::transition(motion::Transition how) {
  m_node->nodeTransition = std::move(how);
  return *this;
}

namespace {

/** The cascade half a rule states, or nothing where it states none. */
const detail::CascadeData* cascadeOf(const detail::ElementNode& node) {
  return node.cascadeData ? &*node.cascadeData : nullptr;
}

}  // namespace

const sigil::weave::Type& Rule::type() const {
  static const sigil::weave::Type none;
  const detail::CascadeData* cascade = cascadeOf(*node());
  return cascade && cascade->font ? *cascade->font : none;
}

const sigil::weave::ParagraphBlock& Rule::paragraph() const {
  static const sigil::weave::ParagraphBlock none;
  const detail::CascadeData* cascade = cascadeOf(*node());
  return cascade && cascade->block ? *cascade->block : none;
}

const std::optional<VarRef>& Rule::inkVar() const {
  static const std::optional<VarRef> none;
  const detail::CascadeData* cascade = cascadeOf(*node());
  return cascade ? cascade->inkVar : none;
}

const std::optional<material::skia::Paint>& Rule::inkPaint() const {
  static const std::optional<material::skia::Paint> none;
  const detail::CascadeData* cascade = cascadeOf(*node());
  return cascade ? cascade->inkPaint : none;
}

PaintBox Rule::inkBox() const {
  const detail::CascadeData* cascade = cascadeOf(*node());
  return cascade ? cascade->inkBox : PaintBox::Element;
}

bool Rule::statesInk() const {
  const detail::CascadeData* cascade = cascadeOf(*node());
  return cascade && cascade->statesInk;
}

const VarTable& Rule::vars() const {
  static const VarTable none;
  const detail::CascadeData* cascade = cascadeOf(*node());
  return cascade ? cascade->vars : none;
}

const std::optional<motion::Transition>& Rule::transition() const {
  return node()->nodeTransition;
}

bool Rule::operator==(const Rule& other) const {
  if (!(m_selector == other.m_selector)) return false;
  if (node() == other.node()) return true;
  // The declarations are compared as the reconcile compares two
  // descriptions, so two rules are equal exactly where no element could
  // tell them apart — with the text properties added, which the reconcile
  // reads only on a text leaf and a rule states for the leaves it matches.
  const detail::ElementNode &a = *node(), &b = *other.node();
  const detail::TextOptions none;
  const detail::TextOptions& optionsA = a.textData ? a.textData->options : none;
  const detail::TextOptions& optionsB = b.textData ? b.textData->options : none;
  return optionsA == optionsB && detail::propertiesEqual(a, b);
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
