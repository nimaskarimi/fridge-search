import SwiftUI

/// Second tab: recipe cards taped to the door, checked against the fridge.
struct PinnedView: View {
    let model: FridgeModel

    var body: some View {
        NavigationStack {
            ZStack {
                FridgeDoor()

                ScrollView {
                    VStack(alignment: .leading, spacing: 0) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text("Pinned")
                                .font(Theme.display(34))
                                .foregroundStyle(Theme.ink)
                            Text("Checked against what's in your fridge")
                                .font(Theme.typewriter(13))
                                .foregroundStyle(Theme.inkSoft)
                        }
                        .padding(.top, 14)
                        .padding(.bottom, 26)

                        if model.pinnedResults.isEmpty {
                            NoteCard(title: "Pin recipes you want to come back to",
                                     message: "Tap the pin on any recipe card and it'll be taped up here.")
                        } else {
                            VStack(spacing: 30) {
                                ForEach(Array(model.pinnedResults.enumerated()), id: \.element.id) { index, result in
                                    NavigationLink(value: result) {
                                        RecipeCard(result: result,
                                                   isPinned: true,
                                                   onTogglePin: { model.togglePinned(result) },
                                                   tilt: [-2.0, 1.6, -1.0, 2.2][index % 4],
                                                   taped: true)
                                    }
                                    .buttonStyle(.plain)
                                }
                            }
                            .padding(.horizontal, 6)
                        }
                    }
                    .padding(.horizontal, 18)
                    .padding(.bottom, 32)
                }
            }
            .toolbar(.hidden, for: .navigationBar)
            .navigationDestination(for: RecipeResult.self) { recipe in
                RecipeDetailView(model: model, recipe: recipe)
            }
        }
    }
}
