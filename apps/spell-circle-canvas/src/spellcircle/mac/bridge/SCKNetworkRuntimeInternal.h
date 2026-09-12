#pragma once

#import "SCKNetworkRuntime.h"

#include <boost/asio/any_io_executor.hpp>

NS_ASSUME_NONNULL_BEGIN

/** C++ hosts may supply an executor they already drive. This initializer
 *  owns no thread or context and never stops the supplied context. Its
 *  owner must keep the context alive through every engine using it. */
@interface SCKNetworkRuntime (Executor)
- (instancetype)initWithExecutor:(boost::asio::any_io_executor)executor;
- (boost::asio::any_io_executor)executor;
@end

NS_ASSUME_NONNULL_END
