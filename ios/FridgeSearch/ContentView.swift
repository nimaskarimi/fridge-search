import SwiftUI

/// Root of the app: the Fridge and Pinned tabs.
struct ContentView: View {
    let model: FridgeModel

    var body: some View {
        TabView {
            FridgeView(model: model)
                .tabItem { Label("Fridge", systemImage: "refrigerator") }
                .toolbarBackground(Theme.tabBar, for: .tabBar)
                .toolbarBackground(.visible, for: .tabBar)
            PinnedView(model: model)
                .tabItem { Label("Pinned", systemImage: "pin") }
                .badge(model.pinned.count)
                .toolbarBackground(Theme.tabBar, for: .tabBar)
                .toolbarBackground(.visible, for: .tabBar)
        }
        .tint(Theme.tomato)
        .preferredColorScheme(.light)  // a fridge door is light; the palette is built for it
    }
}

#Preview {
    let model = FridgeModel()
    return ContentView(model: model)
        .task { await model.load() }
}
