import Foundation
import Observation

/// A recipe pinned to the fridge. The name is kept so a pinned id can be
/// checked against the loaded dataset (ids differ between the sample and
/// Food.com files).
struct PinnedRecipe: Codable, Hashable {
    let id: UInt32
    let name: String
}

/// App state: the pantry, search options, results, and pinned recipes.
@MainActor
@Observable
final class FridgeModel {
    enum Status {
        case loading
        case ready(recipeCount: Int)
        case failed(String)
    }

    var status: Status = .loading
    private(set) var pantry: [String] = []
    var query = ""
    private(set) var suggestions: [String] = []
    var assumeStaples = true
    var maxMissing: Int? = nil
    private(set) var response = SearchResponse()

    private(set) var pinned: [PinnedRecipe] = []         // newest first
    private(set) var pinnedResults: [RecipeResult] = []  // pinned, checked against the pantry

    private var engine: RecipeEngine?
    private static let pantryKey = "pantry"
    private static let pinnedKey = "savedRecipes"  // key kept so existing pins survive

    init() {
        let defaults = UserDefaults.standard
        pantry = defaults.stringArray(forKey: Self.pantryKey) ?? []
        if let data = defaults.data(forKey: Self.pinnedKey),
           let decoded = try? JSONDecoder().decode([PinnedRecipe].self, from: data) {
            pinned = decoded
        }
    }

    // MARK: - Loading

    /// Loads the full Food.com data if it was bundled, else the sample file.
    func load() async {
        guard engine == nil else { return }
        let path = Bundle.main.path(forResource: "foodcom", ofType: "tsv")
            ?? Bundle.main.path(forResource: "sample_recipes", ofType: "tsv")
        guard let path else {
            status = .failed("No recipe file was found in the app.")
            return
        }
        do {
            let engine = try await RecipeEngine.load(path: path)
            self.engine = engine
            status = .ready(recipeCount: engine.recipeCount)
            search()
        } catch {
            status = .failed(error.localizedDescription)
        }
    }

    // MARK: - Pantry

    func addQuery() {
        add(query)
    }

    func add(_ ingredient: String) {
        let item = ingredient.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        query = ""
        suggestions = []
        guard !item.isEmpty, !pantry.contains(item) else { return }
        pantry.append(item)
        pantryChanged()
    }

    func remove(_ ingredient: String) {
        pantry.removeAll { $0 == ingredient }
        pantryChanged()
    }

    func clearPantry() {
        pantry.removeAll()
        pantryChanged()
    }

    func updateSuggestions() {
        guard let engine, !query.trimmingCharacters(in: .whitespaces).isEmpty else {
            suggestions = []
            return
        }
        suggestions = engine.suggestions(for: query).filter { !pantry.contains($0) }
    }

    // MARK: - Search

    /// Re-runs the search and refreshes pinned recipes against the pantry.
    func search() {
        guard let engine else { return }
        response = pantry.isEmpty
            ? SearchResponse()
            : engine.search(pantry, maxMissing: maxMissing, assumeStaples: assumeStaples)
        refreshPinned()
    }

    /// e.g. "Searched 231,636 recipes in 2.4 ms"
    var searchSummary: String? {
        guard case .ready(let count) = status, !pantry.isEmpty else { return nil }
        let micros = response.micros
        let time = micros < 1000 ? "\(Int(micros)) µs" : String(format: "%.1f ms", micros / 1000)
        return "Searched \(count.formatted()) recipes in \(time)"
    }

    // MARK: - Pinned recipes

    func isPinned(_ id: UInt32) -> Bool {
        pinned.contains { $0.id == id }
    }

    func togglePinned(_ recipe: RecipeResult) {
        if isPinned(recipe.id) {
            pinned.removeAll { $0.id == recipe.id }
        } else {
            pinned.insert(PinnedRecipe(id: recipe.id, name: recipe.name), at: 0)
        }
        if let data = try? JSONEncoder().encode(pinned) {
            UserDefaults.standard.set(data, forKey: Self.pinnedKey)
        }
        refreshPinned()
    }

    private func refreshPinned() {
        guard let engine else { return }
        let names = Dictionary(pinned.map { ($0.id, $0.name) }, uniquingKeysWith: { first, _ in first })
        pinnedResults = engine
            .lookup(pinned.map(\.id), pantry: pantry, assumeStaples: assumeStaples)
            .filter { names[$0.id] == $0.name }
    }

    private func pantryChanged() {
        UserDefaults.standard.set(pantry, forKey: Self.pantryKey)
        search()
    }
}
