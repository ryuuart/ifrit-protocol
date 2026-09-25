/** @file
 * The producer's side of a feed: every verb locks the feed it names and
 * hands the call on, so a feed nobody holds any more takes nothing and
 * the transport delivering into it learns so through expired().
 */

#include <memory>
#include <string>
#include <utility>

#include "sigilio/advanced/Transport.h"
#include "sigilio/hub/Feed.h"
#include "sigilio/testing/Testing.h"

namespace sigil::io {

Inlet::Inlet(std::weak_ptr<Feed> feed) : m_feed(std::move(feed)) {}

void Inlet::deliver(Bytes payload, std::string sender) const {
  if (const std::shared_ptr<Feed> feed = m_feed.lock())
    feed->deliver(std::move(payload), std::move(sender));
}

void Inlet::deliver(Bytes payload,
                    std::chrono::duration<double> arrivedAt) const {
  if (const std::shared_ptr<Feed> feed = m_feed.lock())
    feed->deliver(std::move(payload), arrivedAt);
}

void Inlet::fail(std::string why) const {
  if (const std::shared_ptr<Feed> feed = m_feed.lock())
    feed->fail(std::move(why));
}

void Inlet::open(TransportEnd end) const {
  if (const std::shared_ptr<Feed> feed = m_feed.lock()) {
    feed->open(std::move(end));
    return;
  }
  // Nobody holds the feed: the end was opened for no one, and is shut
  // rather than left standing with nothing to read it.
  if (end.close) end.close();
}

void Inlet::close() const {
  if (const std::shared_ptr<Feed> feed = m_feed.lock()) feed->close();
}

bool Inlet::expired() const { return m_feed.expired(); }

Inlet testing::inletOf(const std::shared_ptr<Feed>& feed) {
  return Inlet(feed);
}

}  // namespace sigil::io
