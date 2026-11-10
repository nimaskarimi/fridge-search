import SwiftUI

/// The fridge-door look: an enamel door, letter-magnet ingredients, and
/// recipes on index cards. Fonts are built into iOS, so nothing to install.
enum Theme {
    // The door
    static let door = Color(hex: 0xE3EAE2)
    static let handle = Color(hex: 0xC9D3CC)
    static let tabBar = Color(hex: 0xF4F7F3)

    // Text on the door
    static let ink = Color(hex: 0x23303A)
    static let inkSoft = Color(hex: 0x5D6B72)

    // Magnets
    static let tomato = Color(hex: 0xD9483B)
    static let butter = Color(hex: 0xF0B429)
    static let blueberry = Color(hex: 0x3B5BA5)
    static let pea = Color(hex: 0x5E9B5A)

    // Index cards
    static let paper = Color(hex: 0xFFFEF8)
    static let rule = Color(hex: 0xCBDCEB)      // blue lines
    static let margin = Color(hex: 0xE58C8C)    // red line
    static let pen = Color(hex: 0x2B3A67)       // handwriting ink
    static let pencil = Color(hex: 0xB9C3BD)
    static let tape = Color(hex: 0xF0CF6E).opacity(0.75)

    // Fonts
    static func display(_ size: CGFloat) -> Font {
        .system(size: size, weight: .black, design: .rounded)
    }

    static func hand(_ size: CGFloat, relativeTo style: Font.TextStyle = .title3) -> Font {
        .custom("Noteworthy-Bold", size: size, relativeTo: style)
    }

    static func typewriter(_ size: CGFloat, relativeTo style: Font.TextStyle = .footnote) -> Font {
        .custom("AmericanTypewriter", size: size, relativeTo: style)
    }

    /// A number derived from the text that stays the same between launches
    /// (unlike hashValue), so each magnet keeps its color and tilt.
    static func seed(_ text: String) -> Int {
        text.unicodeScalars.reduce(0) { ($0 &* 31 &+ Int($1.value)) & 0xFFFF }
    }
}

extension Color {
    init(hex: UInt32) {
        self.init(red: Double((hex >> 16) & 0xFF) / 255,
                  green: Double((hex >> 8) & 0xFF) / 255,
                  blue: Double(hex & 0xFF) / 255)
    }
}

/// The enamel door behind every screen, with a handle along the right edge.
struct FridgeDoor: View {
    var body: some View {
        Theme.door
            .overlay(alignment: .topTrailing) {
                Capsule()
                    .fill(Theme.handle)
                    .frame(width: 6, height: 120)
                    .padding(.top, 150)
                    .padding(.trailing, 5)
            }
            .ignoresSafeArea()
    }
}
