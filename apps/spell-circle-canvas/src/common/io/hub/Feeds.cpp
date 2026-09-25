/** @file
 * The hub's feeds: the one feed a URI names while anybody holds it, the
 * transport a scheme is opened through, the recording replay() names
 * for a URI in front of that transport, and the advance that moves
 * every replayed recording forward and runs what is registered to read
 * them.
 */

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Caches.h"
#include "Fetch.h"
#include "FeedDoor.h"
#include "sigilio/hub/Feed.h"
#include "sigilio/hub/Hub.h"
#include "sigilio/hub/Recording.h"

namespace sigil::io {

namespace {

/** The installer the transport feature handed over as the program
 *  started; null when no transport is linked. */
std::atomic<void (*)(Hub&)> linkedTransports{nullptr};

/** The part of a URI before "://", which is what a transport is
 *  registered under. Empty when the URI names no scheme. */
std::string_view feedScheme(std::string_view uri) {
  const size_t mark = uri.find("://");
  return mark == 0 || mark == std::string_view::npos ? std::string_view{}
                                                     : uri.substr(0, mark);
}

}  // namespace

void detail::setLinkedTransports(void (*install)(Hub& hub)) {
  linkedTransports.store(install);
}

void Hub::setFeedTransport(std::string scheme, Transport transport) {
  const std::lock_guard lock(m_mutex);
  m_caches->feedTransports.insert_or_assign(std::move(scheme),
                                            std::move(transport));
}

Transport Hub::feedTransport(std::string_view scheme) const {
  const std::lock_guard lock(m_mutex);
  const auto registered = m_caches->feedTransports.find(scheme);
  return registered == m_caches->feedTransports.end() ? Transport{}
                                              : registered->second;
}

std::vector<Feed> Hub::feeds() const {
  const std::lock_guard lock(m_mutex);
  std::vector<Feed> held;
  held.reserve(m_feeds.size());
  for (auto entry = m_feeds.begin(); entry != m_feeds.end();) {
    std::shared_ptr<detail::FeedDoor> feed = entry->second.lock();
    if (!feed) {
      entry = m_feeds.erase(entry);  // nobody holds it: it is gone
      continue;
    }
    held.push_back(Feed(std::move(feed)));
    ++entry;
  }
  return held;
}

Feed Hub::listen(std::string_view uri, ListenOptions options) {
  std::shared_ptr<detail::FeedDoor> made;
  bool again = false;
  {
    const std::lock_guard lock(m_mutex);
    // The list is pruned as it is walked, so a URI whose feed nobody
    // holds any more opens a new one here rather than answering with
    // the name of something that is gone.
    for (auto entry = m_feeds.begin(); entry != m_feeds.end();) {
      std::shared_ptr<detail::FeedDoor> held = entry->second.lock();
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
        if (held->state().readiness != ReadyState::Connecting)
          return Feed(std::move(held));
        made = std::move(held);
        again = true;
        break;
      }
      ++entry;
    }
    // Made under the lock, which costs a string and the options, so two
    // threads asking for one URI at once cannot open two doors onto it.
    // What OPENS it runs below, with the lock released.
    if (!made) {
      made = std::make_shared<detail::FeedDoor>(std::string(uri),
                                                std::move(options));
      m_feeds.emplace_back(std::string(uri), made);
    }
  }

  // The reason the ask before this one left is taken off before this
  // one tries: it is about a door being opened again, and either what
  // follows leaves a reason of its own or there is nothing wrong with
  // the feed.
  if (again) made->fail({});

  // A URI replay() named is a recording: this feed plays that file back
  // instead of listening at a door.
  std::filesystem::path recording;
  {
    const std::lock_guard lock(m_mutex);
    const auto replayed = m_caches->replays.find(uri);
    if (replayed != m_caches->replays.end()) recording = replayed->second;
  }
  if (!recording.empty()) {
    if (auto recorded = readRecording(recording))
      made->replay(std::move(*recorded));
    else
      made->fail("not a feed recording: " + recording.string());
    return Feed(std::move(made));
  }

  const std::string_view scheme = feedScheme(uri);
  if (scheme.empty()) {
    made->fail("no scheme in \"" + std::string(uri) +
               "\": a feed opens through the transport its scheme names");
    return Feed(std::move(made));
  }
  Transport transport = feedTransport(scheme);
  // THE TRANSPORTS LINKED INTO THE PROGRAM ARE REGISTERED ON THE FIRST ASK
  // NOTHING ANSWERS, once per hub, outside the lock: registering makes
  // sockets' threads and may read this hub back.
  if (!transport) {
    bool install = false;
    {
      const std::lock_guard lock(m_mutex);
      install = !m_linkedTransports;
      m_linkedTransports = true;
    }
    if (void (*const linked)(Hub&) = linkedTransports.load();
        install && linked) {
      linked(*this);
      transport = feedTransport(scheme);
    }
  }
  if (!transport) {
    made->fail("no feed transport registered for \"" + std::string(scheme) +
               "\"");
    return Feed(std::move(made));
  }
  // Called with no lock held: opening a door binds a socket, and the
  // transport may deliver into the feed before it has answered.
  const Inlet inlet(made);
  inlet.open(transport(uri, inlet));
  return Feed(std::move(made));
}

Feed Hub::replay(std::string_view uri, std::string_view recording,
                 ListenOptions options) {
  std::filesystem::path path = detail::localPath(*this, recording);
  std::shared_ptr<detail::FeedDoor> standing;
  {
    const std::lock_guard lock(m_mutex);
    m_caches->replays.insert_or_assign(std::string(uri), std::move(path));
    // The feed standing at the URI is taken off the list, so the ask
    // below makes the replaying one rather than handing back the door
    // that is already open.
    for (auto entry = m_feeds.begin(); entry != m_feeds.end(); ++entry)
      if (entry->first == uri) {
        standing = entry->second.lock();
        m_feeds.erase(entry);
        break;
      }
  }
  // Closed outside the lock: a transport shutting its end down may call
  // back into this hub.
  if (standing) standing->close();
  return listen(uri, std::move(options));
}

void Hub::advance() { advance(std::chrono::steady_clock::now() - m_created); }

Lease Hub::onAdvance(Lease::Callback callback) {
  auto held = std::make_shared<Lease::Callback>(std::move(callback));
  {
    const std::lock_guard lock(m_mutex);
    m_advancers.push_back(held);
  }
  return Lease(std::move(held));
}

void Hub::advance(std::chrono::duration<double> time) {
  // Every feed is taken out from under the lock first: what a recording
  // delivers is somebody else's work, and it may reach a reader that
  // asks this hub for a resource.
  for (const Feed& feed : feeds()) feed.m_door->advance(time);

  // Then what reads them, for the same reason and with the same time.
  // The live callbacks are copied out and the expired entries erased;
  // holding each one while it runs is what lets a callback release its
  // own lease, or register another, without pulling the list out from
  // under this loop.
  std::vector<std::shared_ptr<Lease::Callback>> live;
  {
    const std::lock_guard lock(m_mutex);
    live.reserve(m_advancers.size());
    for (auto entry = m_advancers.begin(); entry != m_advancers.end();) {
      std::shared_ptr<Lease::Callback> callback = entry->lock();
      if (!callback) {
        entry = m_advancers.erase(entry);  // its lease is gone
        continue;
      }
      live.push_back(std::move(callback));
      ++entry;
    }
  }
  for (const std::shared_ptr<Lease::Callback>& callback : live)
    if (*callback) (*callback)(time);
}

}  // namespace sigil::io
