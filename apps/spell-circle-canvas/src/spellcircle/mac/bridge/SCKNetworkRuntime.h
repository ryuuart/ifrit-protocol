#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/** The application's network event loop. Construct it at the application
 *  composition point and pass it to each engine that shares the loop.
 *  The default initializer owns one background worker and keeps the loop
 *  running until the last runtime reference is released. */
NS_SWIFT_UI_ACTOR
@interface SCKNetworkRuntime : NSObject
- (instancetype)init;
@end

NS_ASSUME_NONNULL_END
