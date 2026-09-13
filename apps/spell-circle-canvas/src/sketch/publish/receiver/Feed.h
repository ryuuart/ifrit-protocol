#pragma once

/** @file
 * A SUBSCRIPTION TO ONE PUBLICATION, held by the name it announced rather
 * than by the process behind it — so a publisher that stops and starts is
 * followed instead of lost.
 *
 * FRAMES ARRIVE ON SYPHON'S OWN THREAD and are counted there; the newest
 * one is asked for on the thread that is about to draw or read it.
 */

#import <Metal/Metal.h>
#import <Syphon/SyphonMetalClient.h>

#include <atomic>
#include <memory>
#include <string>

namespace receiver {

class Feed {
 public:
  /** A feed that will follow the publication @p name announces — and,
   *  when @p app is not empty, only that application's. Nothing is
   *  subscribed to until it is opened. */
  Feed(id<MTLDevice> device, std::string name, std::string app);
  ~Feed();
  Feed(const Feed &) = delete;
  Feed &operator=(const Feed &) = delete;

  /** Subscribes, waiting up to @p withinSeconds for the publication to
   *  answer the directory. True when the subscription stands. A feed that
   *  is already standing is left alone. */
  bool open(double withinSeconds);

  /** True while frames can still arrive: the publication said nothing
   *  about retiring AND the directory still knows of it. Both, because a
   *  publisher that retires tells its subscribers and a publisher that
   *  was killed tells nobody — the second is noticed by the publication
   *  going off the list rather than by the subscription being closed. */
  [[nodiscard]] bool standing() const;

  /** How many frames have arrived since the first subscription opened.
   *  Counted across reconnections, because it is what a frame rate is
   *  read from and a publisher that came back is the same publication. */
  [[nodiscard]] unsigned long long frames() const;

  /** The newest frame, or nil when none has arrived. The texture is the
   *  subscription's own and is asked for again for every frame. */
  [[nodiscard]] id<MTLTexture> newestFrame() const;

  /** The application the standing subscription is drawn in, as the
   *  directory named it; empty until one stands. */
  [[nodiscard]] const std::string &publishingApp() const { return m_publishingApp; }

 private:
  id<MTLDevice> m_device;
  SyphonMetalClient *m_client = nil;
  /** Which publication the standing subscription is onto, so one that
   *  stopped and started is told from the one that was subscribed to. */
  NSString *m_identity = nil;
  /** WHAT WAS ASKED FOR, kept as asked: every reconnection looks for the
   *  same thing the first subscription did, so a name that was left open
   *  to any application stays open to any. */
  std::string m_name;
  std::string m_app;
  std::string m_publishingApp;
  /** SHARED WITH THE HANDLER, which Syphon may be inside while this feed
   *  is being let go: the count outlives the feed rather than the handler
   *  reaching into one that has gone. */
  std::shared_ptr<std::atomic<unsigned long long>> m_frames;
};

}  // namespace receiver
