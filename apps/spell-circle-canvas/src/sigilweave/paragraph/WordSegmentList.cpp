/** @file
 * The segment storage behind a Word. The bytes in the public header hold
 * an inline vector of one segment; the container type stays here so no
 * abseil header reaches a consumer, and the size and alignment of the
 * public bytes are checked against it where it is instantiated.
 */

#include <absl/container/inlined_vector.h>

#include <new>
#include <utility>

#include "sigilweave/paragraph/Word.h"

namespace sigil::weave {

namespace {

using SegmentStorage = absl::InlinedVector<WordSegment, 1>;
static_assert(sizeof(SegmentStorage) <= sizeof(WordSegmentList),
              "WordSegmentList's storage is too small for the container");
static_assert(alignof(SegmentStorage) <= alignof(WordSegmentList),
              "WordSegmentList's storage is under-aligned for the container");

SegmentStorage& storageOf(WordSegmentList& list) {
  return *std::launder(reinterpret_cast<SegmentStorage*>(&list));
}
const SegmentStorage& storageOf(const WordSegmentList& list) {
  return *std::launder(reinterpret_cast<const SegmentStorage*>(&list));
}

}  // namespace

WordSegmentList::WordSegmentList() noexcept {
  new (m_storage) SegmentStorage();
}
WordSegmentList::WordSegmentList(const WordSegmentList& other) {
  new (m_storage) SegmentStorage(storageOf(other));
}
WordSegmentList::WordSegmentList(WordSegmentList&& other) noexcept {
  new (m_storage) SegmentStorage(std::move(storageOf(other)));
}
WordSegmentList& WordSegmentList::operator=(const WordSegmentList& other) {
  if (this != &other) storageOf(*this) = storageOf(other);
  return *this;
}
WordSegmentList& WordSegmentList::operator=(WordSegmentList&& other) noexcept {
  // Move-constructing over the destroyed container keeps the assignment
  // non-throwing: the container's own move assignment may allocate.
  if (this != &other) {
    storageOf(*this).~SegmentStorage();
    new (m_storage) SegmentStorage(std::move(storageOf(other)));
  }
  return *this;
}
WordSegmentList::~WordSegmentList() { storageOf(*this).~SegmentStorage(); }
std::span<const WordSegment> WordSegmentList::view() const noexcept {
  const SegmentStorage& storage = storageOf(*this);
  return {storage.data(), storage.size()};
}
void WordSegmentList::append(WordSegment segment) {
  storageOf(*this).push_back(std::move(segment));
}
void WordSegmentList::clear() noexcept { storageOf(*this).clear(); }

}  // namespace sigil::weave
