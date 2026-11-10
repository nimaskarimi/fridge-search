import SwiftUI

/// An ingredient as a chunky letter magnet. Tap to take it off the door.
struct Magnet: View {
    let name: String
    let onRemove: () -> Void

    var body: some View {
        let seed = Theme.seed(name)
        let colors: [(fill: Color, text: Color)] = [
            (Theme.tomato, .white),
            (Theme.butter, Color(hex: 0x5A3D00)),
            (Theme.blueberry, .white),
            (Theme.pea, .white),
        ]
        let style = colors[seed % colors.count]
        let tilt = Double(seed % 7) - 3  // -3° to 3°

        Button(action: onRemove) {
            Text(name)
                .font(.system(.subheadline, design: .rounded).weight(.heavy))
                .foregroundStyle(style.text)
                .padding(.horizontal, 11)
                .padding(.vertical, 6)
                .background(style.fill, in: RoundedRectangle(cornerRadius: 9, style: .continuous))
                // A darker bottom edge gives the magnet some thickness.
                .background {
                    RoundedRectangle(cornerRadius: 9, style: .continuous)
                        .fill(Color.black.opacity(0.28))
                        .offset(y: 3)
                }
                .padding(.bottom, 3)
        }
        .buttonStyle(.plain)
        .rotationEffect(.degrees(tilt))
        .accessibilityLabel("Remove \(name)")
    }
}

/// A recipe written on an index card, with a red margin line and a pin.
struct RecipeCard: View {
    let result: RecipeResult
    let isPinned: Bool
    let onTogglePin: () -> Void
    var tilt: Double = 0
    var taped = false

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(result.name)
                .font(Theme.hand(20))
                .foregroundStyle(Theme.pen)
                .multilineTextAlignment(.leading)
                .lineLimit(2)
            Text(note)
                .font(Theme.typewriter(13))
                .foregroundStyle(Theme.inkSoft)
                .multilineTextAlignment(.leading)
                .lineLimit(3)
        }
        .padding(.leading, 28)
        .padding(.trailing, 44)
        .padding(.vertical, 12)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(alignment: .leading) {
            ZStack(alignment: .leading) {
                Theme.paper
                Rectangle()
                    .fill(Theme.margin)
                    .frame(width: 1)
                    .padding(.leading, 17)
            }
        }
        .clipShape(RoundedRectangle(cornerRadius: 3, style: .continuous))
        .shadow(color: Color.black.opacity(0.1), radius: 1, x: 0, y: 1)
        .overlay(alignment: .topTrailing) {
            PinButton(isPinned: isPinned, action: onTogglePin)
        }
        .overlay(alignment: .top) {
            if taped {
                Rectangle()
                    .fill(Theme.tape)
                    .frame(width: 64, height: 18)
                    .rotationEffect(.degrees(-tilt * 1.5))
                    .offset(y: -9)
            }
        }
        .rotationEffect(.degrees(tilt))
    }

    private var note: String {
        if result.canMakeNow {
            return "Have all \(result.total). Go."
        }
        return "Have \(result.have) of \(result.total).\nNeed: " + result.missing.joined(separator: ", ")
    }
}

/// Pushpin toggle: pinned recipes go on the Pinned tab.
struct PinButton: View {
    let isPinned: Bool
    let action: () -> Void
    var size: CGFloat = 17

    var body: some View {
        Button(action: action) {
            Image(systemName: isPinned ? "pin.fill" : "pin")
                .font(.system(size: size, weight: .semibold))
                .foregroundStyle(isPinned ? Theme.tomato : Theme.pencil)
                .frame(width: 44, height: 44)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(isPinned ? "Unpin from fridge" : "Pin to fridge")
    }
}

/// A plain index card holding a short message, for empty and error states.
struct NoteCard: View {
    let title: String
    let message: String

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(title)
                .font(Theme.hand(20))
                .foregroundStyle(Theme.pen)
            Text(message)
                .font(Theme.typewriter(13))
                .foregroundStyle(Theme.inkSoft)
        }
        .padding(.leading, 28)
        .padding(.trailing, 16)
        .padding(.vertical, 14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(alignment: .leading) {
            ZStack(alignment: .leading) {
                Theme.paper
                Rectangle()
                    .fill(Theme.margin)
                    .frame(width: 1)
                    .padding(.leading, 17)
            }
        }
        .clipShape(RoundedRectangle(cornerRadius: 3, style: .continuous))
        .shadow(color: Color.black.opacity(0.1), radius: 1, x: 0, y: 1)
    }
}
