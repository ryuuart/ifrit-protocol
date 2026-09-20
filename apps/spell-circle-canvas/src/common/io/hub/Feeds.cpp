/** @file
 * The hub's feeds: the one feed a URI names while anybody holds it, the
 * transport a scheme is opened through, the recording a URI that
 * resolves to a file is played back from, and the dispatch that moves
 * every replayed recording forward and runs what is registered to read
 * them.
 */

#include <chrono>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "Caches.h"
#include "Fetch.h"
#include "sigilio/hub/Feed.h"
#include "sigilio/hub/Hub.h"
#include "sigilio/hub/Recording.h"

namespace sigil::io {

namespace {

/** The part of a URI before "://", which is what a transport is
 *  registered under. Empty when the URI names no scheme. */
std::string_view feedScheme(std::string_view uri) {
  const size_t mark = uri.find("://");
  return mark == 0 || mark == std::string_view::npos ? std::string_view{}
                                                     : uri.substr(0, mark);
}

/** The file a feed URI names, or an empty path when it names none. A
 *  mount whose remainder is empty joins nothing onto the mounted path
 *  and leaves a trailing separator, which names no file; the mount
 *  itself is what such a URI meant, so the empty name comes off and a
 *  URI mounted straight onto a recording resolves to it. */
std::filesystem::path feedFile(const Hub& hub, std::string_view uri) {
  std::filesystem::path path = detail::localPath(hub, uri);
  if (!path.empty() && path.filename().empty()) path = path.parent_path();
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || ec) return {};
  return path;
}

}  // namespace

void Hub::setFeedTransport(std::string scheme, FeedTransport transport) {
  const std::lock_guard lock(m_mutex);
  m_caches->feedTransports.insert_or_assign(std::move(scheme),
                                            std::move(transport));
}

FeedTransport Hub::feedTransport(std::string_view scheme) const {
  const std::lock_guard lock(m_mutex);
  const auto registered = m_caches->feedTransports.find(scheme);
  return registered == m_caches->feedTransports.end() ? FeedTransport{}
                                              : registered->second;
}

std::vector<std::shared_ptr<Feed>> Hub::feeds() const {
  const std::lock_guard lock(m_mutex);
  std::vector<std::shared_ptr<Feed>> held;
  held.reserve(m_feeds.size());
  for (auto entry = m_feeds.begin(); entry != m_feeds.end();) {
    std::shared_ptr<Feed> feed = entry->second.lock();
    if (!feed) {
      entry = m_feeds.erase(entry);  // nobody holds it: it is gone
      continue;
    }
    held.push_back(std::move(feed));
    ++entry;
  }
  return held;
}

std::shared_ptr<Feed> Hub::feed(std::string_view uri, Feed::Policy policy) {
  std::shared_ptr<Feed> made;
  bool again = false;
  {
    const std::lock_guard lock(m_mutex);
    // The list is pruned as it is walked, so a URI whose feed nobody
    // holds any more opens a new one here rather than answering with
    // the name of something that is gone.
    for (auto entry = m_feeds.begin(); entry != m_feeds.end();) {
      std::shared_ptr<Feed> held = entry->second.lock();
      if (!held) {
        entry = m_feeds.erase(entry);
        continue;
      }
      if (entry->first == uri) {
        // A DOOR THAT COULD NOT BE OPENED IS OPENED AGAIN HERE. The
        // feed stands, carrying the reason its transport left on it,
        // and what was in the way — a port another program held, a
        // device not plugged in yet — may be gone by now; the ask is
        // what tries it again, into the same feed every reader is
        // already holding. One that has a door is handed back as it
        // stands, and so is one that has closed.
        if (held->opened() || held->closed()) return held;
        made = std::move(held);
        again = true;
        break;
      }
      ++entry;
    }
    // Made under the lock, which costs a string and a policy, so two
    // threads asking for one URI at once cannot open two doors onto it.
    // What OPENS it runs below, with the lock released.
    if (!made) {
      made = std::make_shared<Feed>(std::string(uri), policy);
      m_feeds.emplace_back(std::string(uri), made);
    }
  }

  // The reason the ask before this one left is taken off before this
  // one tries: it is about a door being opened again, and either what
  // follows leaves a reason of its own or there is nothing wrong with
  // the feed.
  if (again) made->fail({});

  // A URI that names a file is a recording: this feed plays that file
  // back instead of listening at a door.
  if (const std::filesystem::path path = feedFile(*this, uri); !path.empty()) {
    if (auto recorded = readRecording(path))
      made->replay(std::move(*recorded));
    else
      made->fail("not a feed recording: " + path.string());
    return made;
  }

  const std::string_view scheme = feedScheme(uri);
  if (scheme.empty()) {
    made->fail("no scheme in \"" + std::string(uri) +
               "\": a feed opens through the transport its scheme names");
    return made;
  }
  FeedTransport transport;
  {
    const std::lock_guard lock(m_mutex);
    const auto registered = m_caches->feedTransports.find(scheme);
    if (registered != m_caches->feedTransports.end())
      transport = registered->second;
  }
  if (!transport) {
    made->fail("no feed transport registered for \"" + std::string(scheme) +
               "\"");
    return made;
  }
  // Called with no lock held: opening a door binds a socket, and the
  // transport may deliver into the feed before it has answered.
  made->opened(transport(uri, std::weak_ptr<Feed>(made)));
  return made;
}

void Hub::dispatch() {
  const std::chrono::duration<double> since =
      std::chrono::steady_clock::now() - m_created;
  dispatch(since.count());
}

DispatchLease Hub::onDispatch(DispatchLease::Callback callback) {
  auto held = std::make_shared<DispatchLease::Callback>(std::move(callback));
  {
    const std::lock_guard lock(m_mutex);
    m_dispatchers.push_back(held);
  }
  return DispatchLease(std::move(held));
}

void Hub::dispatch(double seconds) {
  // Every feed is taken out from under the lock first: what a recording
  // delivers is somebody else's work, and it may reach a reader that
  // asks this hub for a resource.
  for (const std::shared_ptr<Feed>& feed : feeds()) feed->advance(seconds);

  // Then what reads them, for the same reason and with the same second.
  // The live callbacks are copied out and the expired entries erased;
  // holding each one while it runs is what lets a callback release its
  // own lease, or register another, without pulling the list out from
  // under this loop.
  std::vector<std::shared_ptr<DispatchLease::Callback>> live;
  {
    const std::lock_guard lock(m_mutex);
    live.reserve(m_dispatchers.size());
    for (auto entry = m_dispatchers.begin(); entry != m_dispatchers.end();) {
      std::shared_ptr<DispatchLease::Callback> callback = entry->lock();
      if (!callback) {
        entry = m_dispatchers.erase(entry);  // its lease is gone
        continue;
      }
      live.push_back(std::move(callback));
      ++entry;
    }
  }
  for (const std::shared_ptr<DispatchLease::Callback>& callback : live)
    if (*callback) (*callback)(seconds);
}

}  // namespace sigil::io
