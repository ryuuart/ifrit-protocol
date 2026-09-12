#import "SCKNetworkRuntimeInternal.h"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>

#include <memory>
#include <thread>
#include <utility>

namespace {

/** Work remains available between receiver bindings, so the application
 *  can stop and start an engine without restarting its event loop. */
struct OwnedContext {
  boost::asio::io_context context;
  boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work{
      context.get_executor()};
  std::thread worker{[this] { context.run(); }};

  ~OwnedContext() {
    work.reset();
    context.stop();
    if (worker.joinable()) worker.join();
  }
};

}  // namespace

@interface SCKNetworkRuntime () {
  std::unique_ptr<OwnedContext> _owned;
  boost::asio::any_io_executor _executor;
}
@end

@implementation SCKNetworkRuntime

- (instancetype)init {
  self = [super init];
  if (!self) return nil;
  _owned = std::make_unique<OwnedContext>();
  _executor = _owned->context.get_executor();
  return self;
}

- (instancetype)initWithExecutor:(boost::asio::any_io_executor)executor {
  self = [super init];
  if (!self) return nil;
  _executor = std::move(executor);
  return self;
}

- (boost::asio::any_io_executor)executor {
  return _executor;
}

@end
