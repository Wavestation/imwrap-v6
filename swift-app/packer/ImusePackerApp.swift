import SwiftUI

@main
struct ImusePackerApp: App {
    var body: some Scene {
        WindowGroup {
            ImuseProjectView()
                .frame(minWidth: 960, minHeight: 650)
        }
    }
}
