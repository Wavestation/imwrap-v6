import SwiftUI

@main
struct ImusePlayerApp: App {
    var body: some Scene {
        WindowGroup {
            ImusePlayerView()
                .frame(minWidth: 980, minHeight: 680)
        }
    }
}
