import SwiftUI

/// Main tab: ingredients as magnets on the door, recipes as index cards.
struct FridgeView: View {
    @Bindable var model: FridgeModel
    @FocusState private var fieldFocused: Bool

    var body: some View {
        NavigationStack {
            ZStack {
                FridgeDoor()

                ScrollView {
                    VStack(alignment: .leading, spacing: 0) {
                        header
                        magnets
                        addField
                        suggestionList
                        results
                    }
                    .padding(.horizontal, 18)
                    .padding(.bottom, 32)
                }
                .scrollDismissesKeyboard(.interactively)

                statusOverlay
            }
            .toolbar(.hidden, for: .navigationBar)
            .navigationDestination(for: RecipeResult.self) { recipe in
                RecipeDetailView(model: model, recipe: recipe)
            }
            .onChange(of: model.query) { model.updateSuggestions() }
            .onChange(of: model.assumeStaples) { model.search() }
            .onChange(of: model.maxMissing) { model.search() }
            // A light tap when a magnet goes on or comes off the door.
            .sensoryFeedback(.impact(weight: .light), trigger: model.pantry.count)
        }
    }

    // MARK: - Header

    private var header: some View {
        HStack(alignment: .firstTextBaseline) {
            VStack(alignment: .leading, spacing: 2) {
                Text("Fridge")
                    .font(Theme.display(34))
                    .foregroundStyle(Theme.ink)
                Text(doorCount)
                    .font(Theme.typewriter(13))
                    .foregroundStyle(Theme.inkSoft)
            }
            Spacer()
            optionsMenu
        }
        .padding(.top, 14)
        .padding(.bottom, 14)
    }

    private var doorCount: String {
        switch model.pantry.count {
        case 0: return "Nothing on the door yet"
        case 1: return "1 thing on the door"
        default: return "\(model.pantry.count) things on the door"
        }
    }

    private var optionsMenu: some View {
        Menu {
            Toggle("Assume salt, pepper, and water", isOn: $model.assumeStaples)
            Picker("Missing ingredients", selection: $model.maxMissing) {
                Text("Show everything").tag(Int?.none)
                Text("Can make now").tag(Int?.some(0))
                Text("Missing up to 1").tag(Int?.some(1))
                Text("Missing up to 2").tag(Int?.some(2))
                Text("Missing up to 3").tag(Int?.some(3))
            }
            if !model.pantry.isEmpty {
                Button("Clear the door", role: .destructive) { model.clearPantry() }
            }
        } label: {
            Image(systemName: "slider.horizontal.3")
                .font(.system(size: 18, weight: .semibold))
                .foregroundStyle(Theme.ink)
                .frame(width: 44, height: 44)
                .contentShape(Rectangle())
        }
        .accessibilityLabel("Search options")
    }

    // MARK: - Magnets and adding

    @ViewBuilder
    private var magnets: some View {
        if !model.pantry.isEmpty {
            FlowLayout(spacing: 6) {
                ForEach(model.pantry, id: \.self) { item in
                    Magnet(name: item) { model.remove(item) }
                        // The one deliberate animation: magnets snap onto the door.
                        .transition(.asymmetric(
                            insertion: .scale(scale: 1.35).combined(with: .opacity),
                            removal: .scale(scale: 0.7).combined(with: .opacity)))
                }
            }
            .animation(.spring(response: 0.28, dampingFraction: 0.55), value: model.pantry)
            .padding(.bottom, 10)
        }
    }

    private var addField: some View {
        HStack(spacing: 8) {
            TextField("add to the fridge…", text: $model.query)
                .font(Theme.hand(17, relativeTo: .body))
                .foregroundStyle(Theme.pen)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .submitLabel(.done)
                .focused($fieldFocused)
                .onSubmit {
                    model.addQuery()
                    fieldFocused = true  // keep the keyboard up for the next one
                }
            if !model.query.isEmpty {
                Button("Add") { model.addQuery() }
                    .font(.system(.subheadline, design: .rounded).weight(.heavy))
                    .foregroundStyle(Theme.tomato)
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .background(Color.white, in: RoundedRectangle(cornerRadius: 9, style: .continuous))
        .overlay {
            RoundedRectangle(cornerRadius: 9, style: .continuous)
                .strokeBorder(Theme.inkSoft.opacity(0.45), style: StrokeStyle(lineWidth: 1.5, dash: [5, 4]))
        }
    }

    @ViewBuilder
    private var suggestionList: some View {
        if !model.suggestions.isEmpty {
            VStack(alignment: .leading, spacing: 0) {
                ForEach(model.suggestions, id: \.self) { name in
                    Button {
                        model.add(name)
                    } label: {
                        Text(name)
                            .font(Theme.typewriter(15, relativeTo: .body))
                            .foregroundStyle(Theme.ink)
                            .frame(maxWidth: .infinity, alignment: .leading)
                            .padding(.horizontal, 12)
                            .padding(.vertical, 10)
                            .contentShape(Rectangle())
                    }
                    .buttonStyle(.plain)
                    if name != model.suggestions.last {
                        Rectangle().fill(Theme.rule).frame(height: 1)
                    }
                }
            }
            .background(Theme.paper, in: RoundedRectangle(cornerRadius: 3, style: .continuous))
            .padding(.top, 6)
        }
    }

    // MARK: - Results

    @ViewBuilder
    private var results: some View {
        if case .ready = model.status {
            if model.pantry.isEmpty {
                NoteCard(title: "Start with what's on the door",
                         message: "Add a few ingredients and the recipes you can make will show up here.")
                    .padding(.top, 22)
            } else {
                VStack(alignment: .leading, spacing: 2) {
                    Text("You can make")
                        .font(Theme.display(19))
                        .foregroundStyle(Theme.ink)
                    if let summary = model.searchSummary {
                        Text(summary)
                            .font(Theme.typewriter(12))
                            .foregroundStyle(Theme.inkSoft)
                    }
                    if !model.response.unrecognized.isEmpty {
                        Text("Not sure what " + model.response.unrecognized.map { "\"\($0)\"" }
                            .joined(separator: ", ") + " is.")
                            .font(Theme.typewriter(12))
                            .foregroundStyle(Theme.tomato)
                    }
                }
                .padding(.top, 24)
                .padding(.bottom, 12)

                if model.response.results.isEmpty {
                    NoteCard(title: "Nothing fits yet",
                             message: "Add a few more ingredients, or allow more missing ones in the options.")
                }

                VStack(spacing: 14) {
                    ForEach(Array(model.response.results.enumerated()), id: \.element.id) { index, result in
                        NavigationLink(value: result) {
                            RecipeCard(result: result,
                                       isPinned: model.isPinned(result.id),
                                       onTogglePin: { model.togglePinned(result) },
                                       tilt: index.isMultiple(of: 2) ? -0.6 : 0.5)
                        }
                        .buttonStyle(.plain)
                    }
                }
            }
        }
    }

    @ViewBuilder
    private var statusOverlay: some View {
        switch model.status {
        case .loading:
            VStack(spacing: 10) {
                ProgressView()
                    .tint(Theme.ink)
                Text("Loading recipes…")
                    .font(Theme.typewriter(14))
                    .foregroundStyle(Theme.inkSoft)
            }
        case .failed(let message):
            NoteCard(title: "Recipes didn't load", message: message)
                .padding(24)
        case .ready:
            EmptyView()
        }
    }
}
