import SwiftUI

@main
struct ImuseSysExApp: App {
    var body: some Scene {
        WindowGroup {
            ImuseSysExView()
                .frame(minWidth: 800, minHeight: 600)
        }
        .windowStyle(.hiddenTitleBar)
    }
}
