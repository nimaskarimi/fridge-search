# Fridge Search iOS app

A SwiftUI front end for the C++ engine. The engine runs on the phone itself
(no server), so search works offline.

```
SwiftUI views ──► FridgeModel ──► RecipeEngine.swift ──► bridge/fridge_bridge.h (C) ──► C++ engine
```

Swift calls the engine through a small C interface (`bridge/`), imported with
a bridging header. `RecipeEngine.swift` copies every result into Swift structs
so no C pointers leak into the UI code.

## Run it

Requires Xcode 15.3 or newer (iOS 17+).

```bash
brew install xcodegen          # one time
cd ios
xcodegen                       # creates FridgeSearch.xcodeproj
open FridgeSearch.xcodeproj
```

In Xcode, pick an iPhone simulator at the top and press **Run** (⌘R).

If `data/foodcom.tsv` exists when you run `xcodegen`, the app bundles all
~230K recipes; otherwise it uses the 30-recipe sample. Re-run `xcodegen`
after creating `foodcom.tsv`.

**On your own iPhone:** in `project.yml`, change `com.yourname` in
`PRODUCT_BUNDLE_IDENTIFIER` to something unique, re-run `xcodegen`, then in
Xcode select the FridgeSearch target → Signing & Capabilities → choose your
Apple ID as the Team.

## Design

The app looks like a fridge door. Ingredients are letter magnets that snap
on when added (the only animation), recipes are handwritten index cards, and
pinned recipes are taped to the door on the Pinned tab. Fonts are built into
iOS: Noteworthy for handwriting, American Typewriter for notes, SF Rounded
for headings.

## Features

- Ingredient autocomplete, most common ingredients first
- Magnets that persist between launches; tap one to remove it
- Ranked recipe cards showing what you have and what you still need
- Filter by how many ingredients you're missing; toggle assumed staples
- Pin recipes; pinned cards update as your fridge changes
- Recipe detail as a ruled index card with every ingredient checked off
- Shows how long each search took
