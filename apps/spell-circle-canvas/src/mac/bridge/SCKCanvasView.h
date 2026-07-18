#pragma once

// CAMetalLayer-backed canvas view, rendered full-bleed behind the floating
// chrome and presented inside SwiftUI through NSViewRepresentable. The view
// owns the whole viewport transform: fit (biased into the region right of
// the floating activity panel via leftContentInset), scroll-wheel zoom
// around the cursor, and middle- or left-button drag panning. Drawing is
// event-driven — gestures, resizes, and engine frame callbacks — never a
// timer.

#import "SCKEngine.h"

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface SCKCanvasView : NSView

@property(nonatomic, strong, nullable) SCKEngine *engine;

/** Points covered by floating chrome on the left/right; consulted the next
 *  time a fit runs (first render, Fit command, window-resize refit) so the
 *  canvas centers in the uncovered region. Deliberately passive: changing
 *  them never re-fits by itself — pane reveals must not move the canvas. */
@property(nonatomic) double leftContentInset;
@property(nonatomic) double rightContentInset;

/** Called after every redraw with the effective on-screen scale of the
 *  canvas in percent (100 = one canvas pixel per drawable pixel). */
@property(nonatomic, copy, nullable) void (^onViewChanged)
    (double effectiveZoomPercent);

/** Draws the engine's Skia progressive top-edge blur in the blit — the
 *  self-rendered alternative to the SwiftUI Liquid Glass toolbar strip
 *  (ContentView). Off by default; the two shouldn't stack. */
@property(nonatomic) BOOL drawsTopEdgeBlur;

/** Requests a re-render at the current transform. Coalesced onto the
 *  display link: any number of requests per refresh interval (scene frames
 *  and drag/zoom events combined) produce exactly one drawable, so the
 *  main thread never stalls waiting on the drawable pool. */
- (void)redraw;

/** Fits the whole canvas into the uncovered region and centers it. */
- (void)resetView;

/** Zooms to 100% (one canvas pixel per drawable pixel), centered in the
 *  uncovered region. */
- (void)zoomToActualSize;

@end

/**
 * The canvas's transparent stand-in inside the navigation chrome. The real
 * SCKCanvasView renders as a window-level backdrop *behind* the split view
 * — canvas position and UI layout are independent concerns, so pane reveal
 * animations never move a pixel of canvas. This view occupies the detail
 * column instead: it forwards pointer gestures to the canvas and reports
 * how much of the window the chrome covers (the fit-centering insets).
 */
@interface SCKCanvasDetailProxyView : NSView

/** Resolves the backdrop canvas on demand (it may not exist yet when the
 *  chrome builds). */
@property(nonatomic, copy, nullable) SCKCanvasView *_Nullable (^canvasProvider)
    (void);

@end

NS_ASSUME_NONNULL_END
