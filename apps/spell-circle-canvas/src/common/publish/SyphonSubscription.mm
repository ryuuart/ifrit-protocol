// The Syphon client behind the subscribe seam, and the directory it
// finds a publication through. Every line that talks to Objective-C is
// here: the seam itself is plain C++ over opaque handles, so a consumer
// of it compiles wherever this repository compiles.

#import <Metal/Metal.h>
#import <Syphon/SyphonMetalClient.h>
#import <Syphon/SyphonServerDirectory.h>

#include "SyphonSubscription.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace sigil::publish {

namespace {

/** WHAT IS BEING OFFERED ON THIS MACHINE is something the other
 *  processes have to be asked for: the directory hears of a publication
 *  through a notification between processes. Every reading below is of
 *  what it has heard already — a host with a run loop of its own turns
 *  it as part of running, and one without hears the first announcement
 *  whenever it next turns. */
NSDictionary<NSString *, id> *publicationNamed(NSString *name, NSString *application) {
  NSArray<NSDictionary<NSString *, id> *> *matching =
      [[SyphonServerDirectory sharedDirectory] serversMatchingName:name appName:application];
  return matching.count > 0 ? matching.firstObject : nil;
}

/** The application a publication is drawn in, empty where the
 *  description does not say. */
NSString *publicationApplication(NSDictionary<NSString *, id> *publication) {
  NSString *application = publication[SyphonServerDescriptionAppNameKey];
  return application ? application : @"";
}

/** Which publication this is — the one thing about a description that
 *  tells one publication from another of the same name in the same
 *  application. Nil when the publisher did not say. */
NSString *publicationIdentity(NSDictionary<NSString *, id> *publication) {
  return publication[SyphonServerDescriptionUUIDKey];
}

/** Whether the publication @p identity names is still one the directory
 *  knows of.
 *
 *  A PUBLISHER THAT DIED SAYS NOTHING, so a subscription to it stays
 *  open onto frames that will never arrive: what a client is told is
 *  that a server retired, and a process that was killed never told
 *  anybody. The directory finds out anyway — it asks everything it knows
 *  of to announce itself whenever any application on this machine starts
 *  looking, and drops whatever does not answer — so a publication that
 *  is no longer listed is one to stop waiting on. */
bool publicationIsListed(NSString *identity) {
  // A publisher that named no identity cannot be told from another under
  // the same name, so the question it answers is not asked of it.
  if (!identity) return true;
  for (NSDictionary<NSString *, id> *publication in
       [[SyphonServerDirectory sharedDirectory] servers])
    if ([identity isEqualToString:publicationIdentity(publication)]) return true;
  return false;
}

class SyphonSubscription final : public Subscription {
 public:
  SyphonSubscription(std::string name, std::string application, id<MTLDevice> device)
      : m_name(std::move(name)),
        m_application(std::move(application)),
        m_device(device),
        m_generation(std::make_shared<std::atomic<uint64_t>>(0)) {}
  ~SyphonSubscription() override { close(); }
  SyphonSubscription(const SyphonSubscription &) = delete;
  SyphonSubscription &operator=(const SyphonSubscription &) = delete;

  void *newestFrame() override {
    @autoreleasepool {
      // ASKING IS ALSO WHAT OPENS: a publication that was not there when
      // this subscription was made, and one that stopped and came back,
      // is opened onto here — so a host that asks every frame follows
      // the name it was given without a second call for it.
      if (!standing()) open();
      // The frame before this one was borrowed until now.
      [m_frame release];
      m_frame = nil;
      // This file compiles without automatic reference counting, so the
      // reference the client hands over is this object's to hold and to
      // let go of on the next ask.
      m_frame = m_client ? [m_client newFrameImage] : nil;
      return (__bridge void *)m_frame;
    }
  }

  uint64_t generation() const override { return m_generation->load(); }

  bool standing() const override {
    @autoreleasepool {
      return m_client != nil && m_client.isValid && publicationIsListed(m_identity);
    }
  }

  std::string_view name() const override { return m_name; }

  std::string_view publishingApplication() const override { return m_publishingApplication; }

 private:
  void open() {
    close();
    NSDictionary<NSString *, id> *publication =
        publicationNamed(@(m_name.c_str()), @(m_application.c_str()));
    if (!publication) return;
    m_identity = [publicationIdentity(publication) retain];
    m_publishingApplication = publicationApplication(publication).UTF8String;
    // THE HANDLER HOLDS THE COUNTER AND NOTHING ELSE. Frames are
    // announced on Syphon's own thread, and one that arrives while this
    // subscription is being let go increments a number that is still
    // there to increment.
    std::shared_ptr<std::atomic<uint64_t>> generation = m_generation;
    m_client = [[SyphonMetalClient alloc] initWithServerDescription:publication
                                                             device:m_device
                                                            options:nil
                                                    newFrameHandler:^(SyphonMetalClient *) {
                                                      generation->fetch_add(1);
                                                    }];
  }

  void close() {
    // Releasing every reference would stop the client on its own;
    // stopping it says when, and the publisher sees its subscriber leave
    // rather than finding out later.
    [m_client stop];
    [m_client release];
    m_client = nil;
    [m_identity release];
    m_identity = nil;
    [m_frame release];
    m_frame = nil;
    // Which application is drawing this is a fact about the standing
    // subscription, so it goes with it.
    m_publishingApplication.clear();
  }

  /** WHAT WAS ASKED FOR, KEPT AS ASKED: every reconnection looks for the
   *  same thing the first subscription did, so a name that was left open
   *  to any application stays open to any. */
  std::string m_name;
  std::string m_application;
  std::string m_publishingApplication;
  id<MTLDevice> m_device = nil;
  SyphonMetalClient *m_client = nil;
  /** Which publication the standing subscription is onto, so one that
   *  stopped and started is told from the one that was subscribed to. */
  NSString *m_identity = nil;
  /** The frame handed out last, held until another is asked for. */
  id<MTLTexture> m_frame = nil;
  /** SHARED WITH THE HANDLER, which Syphon may be inside while this
   *  subscription is being let go: the count outlives the subscription
   *  rather than the handler reaching into one that has gone. */
  std::shared_ptr<std::atomic<uint64_t>> m_generation;
};

}  // namespace

std::unique_ptr<Subscription> makeSyphonSubscription(std::string name, std::string application,
                                                     void *mtlDevice) {
  @autoreleasepool {
    // KEEPS HEARING WHILE THIS APPLICATION IS NOT THE ACTIVE ONE. What
    // one process has to say to another is held back from an application
    // that is not in front until it comes forward, and a host wearing
    // another application's picture is by definition behind the one
    // drawing it — so it is told to listen anyway. A run with no
    // application of its own is never held back and is unaffected.
    [[NSDistributedNotificationCenter defaultCenter] setSuspended:NO];
  }
  id<MTLDevice> device = (__bridge id<MTLDevice>)mtlDevice;
  return std::make_unique<SyphonSubscription>(std::move(name), std::move(application), device);
}

void *metalDeviceOfThisMachine() {
  // Created once and never released: it is the device every drawing on
  // this machine that asked for no other stands on, and letting it go
  // while anything still draws would take that drawing with it.
  static id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  return (__bridge void *)device;
}

}  // namespace sigil::publish
