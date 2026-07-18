import SwiftUI

/// The Liquid Glass toolbar treatment: a strip spanning the full window
/// width, sized to the bar region and feathered out with a gradient mask.
/// Hit-test transparent. (Measured: z-order and even the strip itself are
/// compositing-cost neutral — WindowServer load under a 60 fps scene
/// stream is the same with the strip on top, beneath, or absent.)
struct GlassToolbarStrip: View {
    var body: some View {
        GeometryReader { geometry in
            Rectangle()
                .fill(Color.clear)
                .glassEffect(.clear, in: .rect(cornerRadius: 0))
                .frame(height: max(geometry.safeAreaInsets.top, 1))
                .mask(LinearGradient(
                    stops: [.init(color: .black, location: 0),
                            .init(color: .clear, location: 1)],
                    startPoint: .top, endPoint: .bottom))
                .offset(y: -geometry.safeAreaInsets.top)
                .frame(maxHeight: .infinity, alignment: .top)
                .allowsHitTesting(false)
        }
    }
}
