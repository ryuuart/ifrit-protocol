import Foundation

/// One activity-feed row: a received scene (or status event) with its
/// timestamp and source address.
struct FeedEntry: Identifiable {
    let id = UUID()
    let timestamp: String
    let source: String
    let message: String

    /// The clock part of the ISO timestamp, for compact display.
    var clockTime: String {
        timestamp.split(separator: "T").last.map(String.init) ?? timestamp
    }
}
