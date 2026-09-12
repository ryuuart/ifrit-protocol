#pragma once

// Pure Objective-C surface of the native macOS engine, imported by the
// SwiftUI app as the Clang module `SpellCircleMacBridge` (see
// module.modulemap). Everything C++ — the shared spellcircle scene core,
// Skia Graphite, SigilWeave — stays behind the engine's ObjC++ sources.

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/CAMetalLayer.h>

#import "SCKNetworkRuntime.h"

NS_ASSUME_NONNULL_BEGIN

/** A single timestamped message entry shown in the activity feed. */
@interface SCKFeedEntry : NSObject
@property(nonatomic, readonly, copy) NSString *timestamp;  ///< ISO with ms
@property(nonatomic, readonly, copy) NSString *source;     ///< ip:port
@property(nonatomic, readonly, copy) NSString *message;
@end

@class SCKEngine;

/** All callbacks arrive on the main queue — annotated so Swift sees them
 *  as @MainActor-isolated and an @MainActor delegate conforms cleanly. */
NS_SWIFT_UI_ACTOR
@protocol SCKEngineDelegate <NSObject>
/** A new scene frame was rendered into the offscreen canvas (and published
 *  to Syphon) — redraw any views presenting it. */
- (void)engineDidRenderScene:(SCKEngine *)engine;
/** The current scene or its drawing configuration needs a new frame. */
- (void)engineSceneDidChange:(SCKEngine *)engine;
/** The accepted-packet rate changed, including byte-identical packets. */
- (void)enginePacketRateDidChange:(SCKEngine *)engine;
/** One feed entry per changed scene packet. */
- (void)engine:(SCKEngine *)engine didAppendFeedEntry:(SCKFeedEntry *)entry;
/** `starting`, `listening`, or `statusText` changed. */
- (void)engineStatusDidChange:(SCKEngine *)engine;
@end

/**
 * The native SpellCircle engine: receives FlatBuffers scene packets over
 * UDP (same wire protocol and default port as the Qt app), decodes and
 * resolves them through the shared spellcircle core, draws them with Skia
 * Graphite + SigilWeave into an offscreen Metal texture at the configured
 * native canvas size, publishes that texture over Syphon, and blits it into
 * CAMetalLayers for on-screen presentation.
 *
 * Main-thread only: datagrams are delivered to the main queue and every
 * property/method below expects to be called there (NS_SWIFT_UI_ACTOR
 * makes Swift enforce that as @MainActor isolation).
 */
NS_SWIFT_UI_ACTOR
@interface SCKEngine : NSObject

/** Receives on the application's supplied event loop. The engine retains
 *  the runtime while its receiver exists and owns no network worker. */
- (instancetype)initWithRuntime:(SCKNetworkRuntime *)runtime NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

@property(nonatomic, weak, nullable) id<SCKEngineDelegate> delegate;

/** @name Network */
/** UDP port; assigning while listening rebinds in place. */
@property(nonatomic) int port;
/** A bind request is waiting for its event loop to report an outcome. */
@property(nonatomic, readonly) BOOL starting;
@property(nonatomic, readonly) BOOL listening;
@property(nonatomic, readonly, copy) NSString *statusText;

/** @name Graphics configuration
 * Same semantics and defaults as the Qt app's GraphicsConfig: author-space
 * values multiplied by `scale` at draw time; the canvas size is the native
 * pixel size of the offscreen (and Syphon-published) texture. Every setter
 * re-resolves and re-renders the current scene.
 */
/** Clamped to [16, 8192] — 0 or Metal-exceeding sizes must never reach
 *  texture allocation. */
@property(nonatomic) int canvasWidth;
@property(nonatomic) int canvasHeight;
@property(nonatomic) double scale;
@property(nonatomic) double strokeWidth;
@property(nonatomic) double labelOffset;
@property(nonatomic) double pointDistance;
@property(nonatomic) double boxWidth;
@property(nonatomic) double boxHeight;
@property(nonatomic) double boxPadding;
@property(nonatomic) double boxDistance;
@property(nonatomic) double fontSize;
/** Empty selects the platform default typeface. */
@property(nonatomic, copy) NSString *fontFamily;
/** CSS-style weight, 100–900 (400 regular, 700 bold). */
@property(nonatomic) int fontWeight;
@property(nonatomic) BOOL fontItalic;
@property(nonatomic, copy) NSColor *accentColor;

/** @name Appearance
 * Light/dark palette for the on-screen blit only — the backdrop behind the
 * canvas, the transparency checkerboard, and the canvas border. Never part
 * of the offscreen/Syphon texture. The canvas view keeps this in sync with
 * its effectiveAppearance; defaults to dark.
 */
@property(nonatomic) BOOL darkAppearance;

/** @name Performance
 * Smoothed metrics for status display, updated as packets arrive and
 * frames render. Rate and rendered-frame callbacks report these separately.
 */
/** CPU milliseconds spent recording + submitting the last scene draws
 *  (exponential moving average). */
@property(nonatomic, readonly) double renderMillis;
/** Accepted scene packet rate over a one-second window, zero after silence. */
@property(nonatomic, readonly) double scenesPerSecond;

/** @name Presentation
 * Received scenes are decoded and marked pending; a self-pacing render
 * clock draws them at targetFramesPerSecond — always, display asleep or
 * not, Syphon client or not — so the published texture never stalls. The
 * canvas view's display link additionally calls renderPendingScene before
 * each blit so on-screen frames are as fresh as the clock allows.
 */

/** Offscreen render rate cap in frames per second (60, 120, 144, …).
 *  Clamped to [1, 240]; default 60. */
@property(nonatomic) double targetFramesPerSecond;

@end

/** The socket and what arrives on it. */
@interface SCKEngine (Network)
/** Requests an asynchronous bind. Status changes report binding, success,
 *  bind failure, and a terminal receive failure on the main queue. */
- (void)start;
- (void)stop;
/** Removes all scene entities and re-renders the (empty) canvas. */
- (void)clearScene;
@end

/** The offscreen canvas and the clock that paces it. */
@interface SCKEngine (Render)
/** Renders the pending scene into the offscreen texture if one is due.
 *  Called by the canvas view on each display-link tick. */
- (void)renderPendingScene;
@end

/** The on-screen presentation of what was rendered offscreen. */
@interface SCKEngine (Blit)
/**
 * Blits the current offscreen scene into @p layer's next drawable: an
 * appearance-matched backdrop, the canvas plate, the latest scene frame,
 * and a progressive top-edge blur emulating the system toolbar scroll-edge
 * effect over the top @p topInsetPixels (0 disables it). The view owns the
 * whole transform: @p scale is drawable pixels per canvas pixel and
 * @p centerX / @p centerY position the canvas center in drawable pixels.
 */
- (void)drawInLayer:(CAMetalLayer *)layer
              scale:(double)scale
            centerX:(double)centerX
            centerY:(double)centerY
           topInset:(double)topInsetPixels;
@end

NS_ASSUME_NONNULL_END
