/** @file
 * The additions pass: every node applying adding operators is handed its
 * scope — the settled nodes under it, closed at a node with operators of
 * its own — and what the operators attach is reconciled beside the owner's
 * authored children, after them, out of their flow.
 */

#include <include/core/SkMatrix.h>

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ComposeRuntime.h"
#include "DeriveInternal.h"

namespace sigil::compose {

using namespace detail;

bool sameDescription(const Element& a, const Element& b) {
  const ElementNode& left = *a.node();
  const ElementNode& right = *b.node();
  if (!propertiesEqual(left, right)) return false;
  if (left.children.size() != right.children.size()) return false;
  for (size_t i = 0; i < left.children.size(); ++i)
    if (!sameDescription(left.children[i], right.children[i])) return false;
  return true;
}

namespace {

bool listsEqual(const std::vector<Element>& a, const std::vector<Element>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (!sameDescription(a[i], b[i])) return false;
  return true;
}

/** The classes an operator states, laid UNDER the ones the element names
 *  itself — first in the list, so the element's own fold over them. */
void layClassesUnder(ElementNode& node, std::string_view names) {
  if (names.empty()) return;
  std::vector<std::string> stated;
  size_t start = 0;
  while (start < names.size()) {
    const size_t end = names.find(' ', start);
    const std::string_view one =
        names.substr(start, end == std::string_view::npos ? names.size() - start
                                                          : end - start);
    if (!one.empty()) stated.emplace_back(one);
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  if (stated.empty()) return;
  std::vector<std::string>& classes = node.cascadeData.ensure().classes;
  classes.insert(classes.begin(), stated.begin(), stated.end());
}

}  // namespace

void Composer::Impl::collectScope(Instance& from, SkPoint origin, Scope& scope,
                                  std::vector<Instance*>& owners) {
  for (const auto& child : from.children) {
    const ElementNode& node = *child->description;
    // A node an operator added is nobody's to read: an operator reads
    // what was authored and builds on it.
    if (node.added()) continue;
    Scope::Node record;
    record.key = node.key;
    if (node.operatorData) record.attributes = node.operatorData->attributes;
    if (node.cascadeData) record.classes = node.cascadeData->classes;
    const SkRect absolute = absoluteRect(*child);
    record.bounds = absolute.makeOffset(-origin.x(), -origin.y());
    record.outline = resolvedShapeOf(*child).makeTransform(
        SkMatrix::Translate(record.bounds.left(), record.bounds.top()));
    scope.mutableNodes().push_back(std::move(record));
    owners.push_back(child.get());
    // THE SCOPE IS CLOSED at a node with operators of its own: it is one
    // node here, and what stands under it is its own business.
    const bool closed =
        node.operatorData && !node.operatorData->operators.empty();
    if (!closed) collectScope(*child, origin, scope, owners);
  }
}

bool Composer::Impl::phaseAdditions() {
  bool changed = false;
  // Which (owner, source) slices this pass wrote, so a slice a scope no
  // longer attaches to is retired rather than left standing.
  std::vector<std::pair<Instance*, Instance*>> touched;
  const auto flatten = [](Instance& owner) {
    owner.addedChildren.clear();
    for (const Instance::AdditionSlice& slice : owner.additions)
      owner.addedChildren.insert(owner.addedChildren.end(),
                                 slice.elements.begin(), slice.elements.end());
  };
  const auto remount = [&](Instance& owner) {
    flatten(owner);
    reconciler.patchChildren(owner, children(owner, owner.description));
    owner.markPaintDirtyUp();
    changed = true;
  };
  // ONE SCOPE'S SLICE OF ONE OWNER: replaced when it differs, left alone
  // when it is the same list, so a node attached to by its own operators
  // and by a scope above it keeps both.
  const auto settle = [&](Instance& owner, Instance& source,
                          std::vector<Element> list) {
    touched.emplace_back(&owner, &source);
    auto slice = std::find_if(
        owner.additions.begin(), owner.additions.end(),
        [&](const Instance::AdditionSlice& s) { return s.source == &source; });
    if (slice == owner.additions.end()) {
      if (list.empty()) return;
      owner.additions.push_back({&source, std::move(list)});
    } else {
      if (listsEqual(slice->elements, list)) return;
      if (list.empty())
        owner.additions.erase(slice);
      else
        slice->elements = std::move(list);
    }
    remount(owner);
  };
  for (Instance* inst : addingInstances) {
    const OperatorData& operators = *inst->description->operatorData;
    Scope scope;
    const SkRect own = absoluteRect(*inst);
    scope.box = SkRect::MakeWH(own.width(), own.height());
    std::vector<Instance*> owners;
    collectScope(*inst, {own.left(), own.top()}, scope, owners);
    scope.mutableNodes();  // binds every record to the scope it is read from
    // The lists, one per owner: the scope's own is always settled, so an
    // operator that attaches nothing this frame retires what it attached
    // last frame.
    boost::unordered_flat_map<Instance*, std::vector<Element>> lists;
    lists[inst];
    for (const Operator& op : operators.operators) {
      if (!op.adds()) continue;
      const size_t before = scope.attachments().size();
      op.add(scope);
      for (size_t i = before; i < scope.attachments().size(); ++i) {
        const Scope::Attachment& attachment = scope.attachments()[i];
        Element element =
            *std::static_pointer_cast<Element>(attachment.element);
        ElementNode* node = NodeAccess::declarations(element);
        node->operatorData.ensure().added = true;
        // The operator's own properties, where the element states none.
        if (op.zIndexStated() && node->paint.zIndex == 0)
          node->paint.zIndex = *op.zIndexStated();
        layClassesUnder(*node, op.classesStated());
        Instance* owner = attachment.owner == Scope::Attachment::kScope
                              ? inst
                              : owners[attachment.owner];
        lists[owner].push_back(std::move(element));
      }
    }
    for (auto& [owner, list] : lists) settle(*owner, *inst, std::move(list));
  }
  // A slice no scope wrote this pass — its scope stopped attaching, or
  // stopped applying operators at all — keeps nothing from the last.
  const std::vector<Instance*> owners = additionOwners;
  for (Instance* owner : owners) {
    bool retired = false;
    std::erase_if(owner->additions, [&](const Instance::AdditionSlice& slice) {
      const bool written =
          std::find(touched.begin(), touched.end(),
                    std::pair<Instance*, Instance*>(owner, slice.source)) !=
          touched.end();
      if (!written) retired = true;
      return !written;
    });
    if (retired) remount(*owner);
  }
  if (changed) {
    // What was mounted inherits from where it stands and is indexed by
    // key like anything else; the layout runs again with it standing.
    cascadeDirty = true;
    runCascade();
    volatileDirty = true;
    rebuildKeyIndex();
    needsLayout = true;
  }
  return changed;
}

}  // namespace sigil::compose
