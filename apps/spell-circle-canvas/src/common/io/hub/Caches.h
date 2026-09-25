#pragma once

/** @file
 * The tables a hub keeps, defined where they are read rather than where
 * the hub is declared: the cache of entries keyed by the string an ask
 * is cached under, the decoder registered per decoded type, the
 * transport registered per feed scheme and the recording replayed per
 * feed URI. Every one is read and written only under the hub's mutex.
 */

#include <boost/container/flat_map.hpp>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <vector>

#include "sigilio/hub/Hub.h"

namespace sigil::io {

struct Hub::Caches {
  /** One decoded view of an entry: the value, and how to make it again
   *  from fresh bytes. */
  struct View {
    std::shared_ptr<const void> value;
    Redecode decode;
  };

  /** One cached resource. The bytes and each decoded view are
   *  independent, each populated the first time its accessor asks;
   *  asking for bytes never decodes, and decoding never drops bytes
   *  already served. A load<T>() with options other than T's defaults
   *  lives in an entry of its own, keyed by the URI and the options'
   *  place among those asked of T before.
   *
   *  `uri` is the original request string. reload() and poll() use it
   *  directly — a URI is never recovered by parsing a map key, so no
   *  character a URI may contain can confuse them. `path` is where
   *  the bytes came from (for http(s) URIs, the disk-cache file) and
   *  doubles as the decode format hint.
   *
   *  `mtime` is what poll() compares. It is written when the entry is
   *  created and when poll() reloads; an accessor that fetches fresh
   *  bytes into an existing entry leaves it alone, so a file that
   *  changed between two asks shows a stale stamp and the next poll()
   *  re-decodes every populated view from one read, bringing the
   *  views back into agreement. */
  struct Entry {
    std::string uri;
    std::shared_ptr<const Bytes> bytes;
    boost::container::flat_map<std::type_index, View> views;
    std::filesystem::path path;
    std::filesystem::file_time_type mtime;
  };

  boost::container::flat_map<std::string, Entry, std::less<>> entries;
  boost::container::flat_map<std::type_index, Redecode> decoders;
  /** Per type loaded with options, what binds other options into its
   *  registered decoder. */
  boost::container::flat_map<std::type_index, Configure> configured;
  /** Per type, every distinct non-default options value asked of it, in
   *  the order first asked: an options value's place here is what its
   *  entries are keyed by, so equal options share one decode. */
  boost::container::flat_map<std::type_index,
                             std::vector<std::shared_ptr<const void>>>
      options;
  boost::container::flat_map<std::string, FeedTransport, std::less<>>
      feedTransports;
  /** Per feed URI, the recording replay() named for it: a feed opened
   *  on that URI plays this file instead of asking a transport. */
  boost::container::flat_map<std::string, std::filesystem::path, std::less<>>
      replays;
};

}  // namespace sigil::io
