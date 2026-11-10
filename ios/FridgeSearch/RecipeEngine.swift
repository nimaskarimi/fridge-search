import Foundation

struct IngredientLine: Hashable {
    let name: String
    let have: Bool
}

struct RecipeResult: Identifiable, Hashable {
    let id: UInt32
    let name: String
    let score: Float
    let coverage: Float
    let rating: Float
    let have: Int
    let total: Int
    let ingredients: [IngredientLine]  // missing ingredients first

    var missing: [String] { ingredients.filter { !$0.have }.map(\.name) }
    var canMakeNow: Bool { have == total }
}

struct SearchResponse {
    var results: [RecipeResult] = []
    var unrecognized: [String] = []
    var micros: Double = 0
}

enum EngineError: LocalizedError {
    case loadFailed(String)

    var errorDescription: String? {
        switch self {
        case .loadFailed(let message): return "Couldn't load recipes: \(message)"
        }
    }
}

/// Swift wrapper around the C++ engine (through bridge/fridge_bridge.h).
///
/// It owns the engine handle and copies every result into Swift values, so no
/// C pointers leave this file. It lives on the main actor because the engine
/// isn't thread-safe; searches take a few milliseconds, so that's fine. Only
/// the one-time load runs in the background.
@MainActor
final class RecipeEngine {
    // nonisolated(unsafe) so deinit can free it; nothing else uses it off the main actor.
    nonisolated(unsafe) private let handle: OpaquePointer
    let recipeCount: Int
    let ingredientCount: Int

    private init(handle: OpaquePointer) {
        self.handle = handle
        recipeCount = fridge_engine_recipe_count(handle)
        ingredientCount = fridge_engine_ingredient_count(handle)
    }

    deinit {
        fridge_engine_destroy(handle)
    }

    /// Loads and indexes the recipe file on a background thread.
    static func load(path: String) async throws -> RecipeEngine {
        // The handle comes back as an integer because raw pointers can't be
        // passed between threads in Swift's concurrency model.
        let (bits, message) = await Task.detached(priority: .userInitiated) { () -> (UInt, String) in
            var buffer = [CChar](repeating: 0, count: 512)
            let capacity = buffer.count
            let handle = path.withCString { fridge_engine_create($0, &buffer, capacity) }
            let message = buffer.withUnsafeBufferPointer { String(cString: $0.baseAddress!) }
            return (UInt(bitPattern: handle), message)
        }.value

        guard let handle = OpaquePointer(bitPattern: bits) else {
            throw EngineError.loadFailed(message)
        }
        return RecipeEngine(handle: handle)
    }

    func search(_ pantry: [String], topK: Int = 30, maxMissing: Int? = nil,
                assumeStaples: Bool = true) -> SearchResponse {
        let limit = maxMissing.map { UInt32(clamping: $0) } ?? UInt32.max
        let raw = withCStrings(pantry) { strings, count in
            fridge_search(handle, strings, count, topK, limit, assumeStaples)
        }
        return read(raw)
    }

    /// Specific recipes by id (the pinned ones), checked against the pantry.
    func lookup(_ ids: [UInt32], pantry: [String], assumeStaples: Bool = true) -> [RecipeResult] {
        let raw = withCStrings(pantry) { strings, count in
            ids.withUnsafeBufferPointer { idBuffer in
                fridge_lookup(handle, idBuffer.baseAddress, idBuffer.count, strings, count, assumeStaples)
            }
        }
        return read(raw).results
    }

    /// Copies Swift strings into C strings that live for the duration of `body`.
    private func withCStrings<T>(_ strings: [String],
                                 _ body: (UnsafePointer<UnsafePointer<CChar>?>?, Int) -> T) -> T {
        let cStrings: [UnsafeMutablePointer<CChar>?] = strings.map { strdup($0) }
        defer { cStrings.forEach { free($0) } }
        let constStrings: [UnsafePointer<CChar>?] = cStrings.map { $0.map { UnsafePointer($0) } }
        return constStrings.withUnsafeBufferPointer { buffer in
            body(buffer.baseAddress, buffer.count)
        }
    }

    /// Copies a C results handle into Swift values, then frees it.
    private func read(_ raw: OpaquePointer?) -> SearchResponse {
        guard let raw else { return SearchResponse() }
        defer { fridge_results_destroy(raw) }

        var response = SearchResponse()
        response.micros = fridge_results_search_micros(raw)
        for i in 0..<fridge_results_unrecognized_count(raw) {
            response.unrecognized.append(String(cString: fridge_results_unrecognized(raw, i)))
        }
        for i in 0..<fridge_results_count(raw) {
            let ingredients = (0..<fridge_result_ingredient_count(raw, i)).map { j in
                IngredientLine(name: String(cString: fridge_result_ingredient(raw, i, j)),
                               have: fridge_result_ingredient_have(raw, i, j))
            }
            response.results.append(RecipeResult(
                id: fridge_result_id(raw, i),
                name: String(cString: fridge_result_name(raw, i)),
                score: fridge_result_score(raw, i),
                coverage: fridge_result_coverage(raw, i),
                rating: fridge_result_rating(raw, i),
                have: Int(fridge_result_have(raw, i)),
                total: Int(fridge_result_total(raw, i)),
                ingredients: ingredients))
        }
        return response
    }

    /// Ingredient names for autocomplete, most common first.
    func suggestions(for prefix: String, limit: Int = 6) -> [String] {
        var names = [UnsafePointer<CChar>?](repeating: nil, count: limit)
        let count = prefix.withCString { cPrefix in
            names.withUnsafeMutableBufferPointer { buffer in
                fridge_engine_suggest(handle, cPrefix, buffer.baseAddress, limit)
            }
        }
        return names.prefix(count).compactMap { $0.map { String(cString: $0) } }
    }
}
