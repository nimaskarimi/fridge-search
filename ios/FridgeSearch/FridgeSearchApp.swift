import SwiftUI

@main
struct FridgeSearchApp: App {
    @State private var model = FridgeModel()

    var body: some Scene {
        WindowGroup {
            ContentView(model: model)
                .task { await model.load() }
        }
    }
}
