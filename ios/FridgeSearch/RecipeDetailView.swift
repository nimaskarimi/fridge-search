import SwiftUI

/// One recipe as a full index card: handwritten title over a red line, then
/// the ingredients on the blue lines, with what you still need marked.
struct RecipeDetailView: View {
    let model: FridgeModel
    let recipe: RecipeResult

    private let lineHeight: CGFloat = 34

    var body: some View {
        ZStack {
            FridgeDoor()

            ScrollView {
                VStack(alignment: .leading, spacing: 0) {
                    card
                    Text("Coverage counts rare ingredients more than common ones, so missing saffron matters more than missing butter.")
                        .font(Theme.typewriter(12))
                        .foregroundStyle(Theme.inkSoft)
                        .padding(.top, 16)
                        .padding(.horizontal, 4)
                }
                .padding(.horizontal, 18)
                .padding(.top, 8)
                .padding(.bottom, 32)
            }
        }
        .navigationBarTitleDisplayMode(.inline)
        .toolbar(.visible, for: .navigationBar)
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                PinButton(isPinned: model.isPinned(recipe.id),
                          action: { model.togglePinned(recipe) },
                          size: 19)
            }
        }
    }

    private var card: some View {
        VStack(alignment: .leading, spacing: 0) {
            // Header area above the red line
            VStack(alignment: .leading, spacing: 6) {
                Text(recipe.name)
                    .font(Theme.hand(28, relativeTo: .title))
                    .foregroundStyle(Theme.pen)
                Text(summary)
                    .font(Theme.typewriter(13))
                    .foregroundStyle(Theme.inkSoft)
            }
            .padding(.horizontal, 18)
            .padding(.top, 18)
            .padding(.bottom, 12)

            Rectangle()
                .fill(Theme.margin)
                .frame(height: 1.5)

            // Ingredients, one per ruled line
            ForEach(recipe.ingredients, id: \.self) { line in
                HStack(alignment: .firstTextBaseline, spacing: 10) {
                    Image(systemName: line.have ? "checkmark" : "circle")
                        .font(.system(size: 12, weight: .bold))
                        .foregroundStyle(line.have ? Theme.pea : Theme.tomato)
                        .frame(width: 14)
                    Text(line.name)
                        .font(Theme.hand(18, relativeTo: .body))
                        .foregroundStyle(line.have ? Theme.pen : Theme.tomato)
                    Spacer()
                    if !line.have {
                        Text("need")
                            .font(Theme.typewriter(12))
                            .foregroundStyle(Theme.tomato)
                    }
                }
                .padding(.horizontal, 18)
                .frame(minHeight: lineHeight, alignment: .bottom)
                .padding(.bottom, 4)
                .overlay(alignment: .bottom) {
                    Rectangle().fill(Theme.rule).frame(height: 1)
                }
            }

            // A couple of empty lines so it reads as a card, not a list
            ForEach(0..<2, id: \.self) { _ in
                Color.clear
                    .frame(height: lineHeight)
                    .overlay(alignment: .bottom) {
                        Rectangle().fill(Theme.rule).frame(height: 1)
                    }
            }
            Color.clear.frame(height: 14)
        }
        .background(Theme.paper)
        .clipShape(RoundedRectangle(cornerRadius: 3, style: .continuous))
        .shadow(color: Color.black.opacity(0.1), radius: 1, x: 0, y: 1)
    }

    private var summary: String {
        var parts: [String] = []
        parts.append(recipe.canMakeNow
                     ? "You have all \(recipe.total) ingredients."
                     : "You have \(recipe.have) of \(recipe.total) ingredients.")
        parts.append("Coverage \(Int((recipe.coverage * 100).rounded()))%.")
        if recipe.rating > 0 {
            parts.append("Rated " + String(format: "%.1f", recipe.rating) + ".")
        }
        return parts.joined(separator: " ")
    }
}
