#pragma once

/** @file
 * A host with nothing behind it: a description that is a key, a kind and
 * one integer; a node that records the lane it carried; and every
 * ReconcileHost operation written into a log, so a test reads what the
 * reconciler asked of its host and in what order.
 */

#include <sigilcore/reconcile/Reconcile.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Every feature's fake host lives in its own `sigil::core::test::<feature>`
// namespace. That is what lets each of them use the plainest name for what
// it is — FakeHost, FakeNode — without one redefining another.
namespace sigil::core::test::reconcile {

struct FakeDescription;
using Description = std::shared_ptr<FakeDescription>;

/** The description: comparable in `key`, `kind` and `value`. */
struct FakeDescription {
  std::string key;
  std::string kind = "box";
  int value = 0;
  /** Children of a positioned node carry a mode fixed at mount, which is
   *  what remountRequired() decides on. */
  bool positioned = false;
  /** A slot: its children are filled by replaceContent(), never walked. */
  bool slot = false;
  std::vector<Description> children;
  std::optional<Memo<Description>> memo;
};

/** One description, keyed or not, with children. */
inline Description description(std::string key, int value = 0,
                 std::vector<Description> children = {}) {
  auto d = std::make_shared<FakeDescription>();
  d->key = std::move(key);
  d->value = value;
  d->children = std::move(children);
  return d;
}

/** The retained node: the skeleton plus what this host keeps per node. */
struct FakeNode : Node<FakeNode, Description> {
  int id = 0;
  std::string kind;
  bool positionedMode = false;  ///< fixed at mount
  int lane = 0;                 ///< retained across patches, like a motion
};

struct FakeHost {
  using Node = FakeNode;
  using Reconciler = core::Reconciler<FakeHost, FakeNode, Description>;

  Reconciler reconciler{*this};
  std::unique_ptr<FakeNode> root;
  std::vector<std::string> log;
  std::vector<std::pair<int, uint64_t>> retired;  ///< (id, frame)
  int nextId = 1;

  // ---- reading a description ----
  static const std::string& keyOf(const Description& d) { return d->key; }
  static bool equal(const Description& a, const Description& b) {
    return a->key == b->key && a->kind == b->kind && a->value == b->value;
  }
  static bool reconcilesChildren(const Description& d) { return !d->slot; }
  static const std::vector<Description>& children(const Description& d) {
    return d->children;
  }
  // A fake element IS its description, so the handle read off it is the
  // element itself. Every caller reads one out of the parent's child
  // vector, which outlives the reconciler pass; a temporary never reaches
  // here.
  // NOLINTNEXTLINE(bugprone-return-const-ref-from-parameter)
  static const Description& descriptionOf(const Description& child) { return child; }
  static const Memo<Description>* memoOf(const Description& d) {
    return d->memo ? &*d->memo : nullptr;
  }
  static Description produce(const Memo<Description>& memo) {
    return memo.invoke(memo.props);
  }

  // ---- acting on a node ----
  std::unique_ptr<FakeNode> create(const Description& d, FakeNode* parent,
                                   size_t ordinal, size_t count) {
    auto node = std::make_unique<FakeNode>();
    node->id = nextId++;
    node->parent = parent;
    node->positionedMode = parent && parent->description && parent->description->positioned;
    log.push_back("create " + d->key + " #" + std::to_string(node->id) + " " +
                  std::to_string(ordinal) + "/" + std::to_string(count));
    reconciler.patch(*node, d);
    return node;
  }
  void onPatched(FakeNode& node, const FakeDescription* prev, const FakeDescription& next) {
    log.push_back(std::string("patch ") + next.key + (prev ? "" : " (mount)"));
    // An identity change rebuilds what the kind decides and keeps the rest.
    node.kind = next.kind;
  }
  void reorder(FakeNode& parent, bool structureChanged) {
    log.push_back("reorder " + (parent.description ? parent.description->key : "?") +
                  (structureChanged ? " changed" : ""));
  }
  bool remountRequired(const FakeNode& match, const FakeNode& parent) const {
    return match.positionedMode != (parent.description && parent.description->positioned);
  }
  void invalidate(FakeNode& node) {
    log.push_back("invalidate " + (node.description ? node.description->key : "?"));
  }
  void destroy(std::unique_ptr<FakeNode> node, uint64_t frame) {
    retired.emplace_back(node->id, frame);
    log.push_back("destroy #" + std::to_string(node->id));
  }

  // ---- conveniences ----
  void render(const Description& d) { reconciler.render(root, d); }
  FakeNode* child(size_t i) { return root->children.at(i).get(); }
  std::vector<std::string> childKeys() const {
    std::vector<std::string> keys;
    for (const auto& c : root->children) keys.push_back(c->description->key);
    return keys;
  }
};

}  // namespace sigil::core::test::reconcile
