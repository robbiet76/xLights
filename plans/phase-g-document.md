# Phase G — Document / iCloud polish

**Status: ✓ complete (2026-04-22).**

Builds on the Phase E document lifecycle. Phase E got save /
save-as / open / close / new working against the local
filesystem and the show-folder bookmark; Phase G layered on the
iPadOS-idiomatic polish around iCloud Drive, multi-process file
coordination, and the "Open in xLights" system integration, plus
end-to-end `.xsqz` sequence-package round-trip.

## What landed

### G-3 — `.xsq` + `.xsqz` document registration

- Partial `Info.plist` at `macOS/Assets/xLights-iPad/Info.plist`
  declares two UTIs:
  - `org.xlights.sequence` (conforms to `public.xml`, extension
    `xsq`) — role `Editor`, `LSHandlerRank=Owner`.
  - `org.xlights.sequence-package` (conforms to
    `public.zip-archive`, extension `xsqz`) — role `Editor`,
    `LSHandlerRank=Owner`.
  `INFOPLIST_FILE` build setting points at it;
  `GENERATE_INFOPLIST_FILE = YES` stays so Xcode still merges in
  `CFBundle*`, scene-manifest, and orientation keys from the
  pbxproj.
- `LSSupportsOpeningDocumentsInPlace = true` +
  `UIFileSharingEnabled = true` so Files offers "Open in
  xLights" directly (not just via share sheet).
- `ContentView` `.onOpenURL { handleIncomingSequenceURL($0) }`.
  Handler routes `.xsqz` through the sandbox round-trip (see
  below) and `.xsq` through the existing `openSequence`, minting
  a security-scoped bookmark via
  `XLSequenceDocument.obtainAccess(toPath:enforceWritable:)`.
- `.xsq` URLs that arrive before the show folder finishes
  loading are queued in `pendingOpenURL` and replayed when
  `isShowFolderLoaded` flips true. `.xsqz` never defers — it
  brings its own show folder (temp extract dir), so it opens
  immediately even on a fresh install with no configured show
  folder.

### `.xsqz` sequence-package round-trip

- **Open**: `handleIncomingSequenceURL` detects `.xsqz` /
  `.zip` / `.piz` → `copyPackageToSandbox(originalURL:)` uses
  `NSFileCoordinator.coordinate(readingItemAt:)` + URL-based
  `FileManager.copyItem` (both honour the Files-delivered
  security scope) to stage the package inside the app's
  `NSTemporaryDirectory()`. iOS path-based POSIX syscalls can't
  read `~/Library/Mobile Documents` without iCloud entitlements,
  so every subsequent path-based op (minizip extraction, stat,
  etc.) runs against the sandbox copy.
- Bridge `openPackagedSequence(atPath:)` creates a
  `SequencePackage`, extracts to a sub-temp-dir, calls
  `LoadShowFolder(GetTempShowFolder())` (not `GetTempDir()` —
  old-format `.xsqz` files nest under `<showname>/` and
  `GetTempShowFolder` returns the dir actually containing
  `xlights_rgbeffects.xml`), and opens the inner `.xsq` through
  the normal flow.
- On fresh-install tap (no show folder ever configured), the
  view model flips `isShowFolderLoaded = true` + mirrors the
  temp dir into `showFolderPath` so `ContentView` renders the
  sequencer instead of the folder-setup screen. The auto-opened
  setup sheet is dismissed by `.onChange(of: isSequenceLoaded)`
  so the App-Store reviewer flow is a single tap.
- **Save**: bridge `saveSequence` detects package state and
  calls `SequencePackage::Pack(originalSandboxXsqzPath, …)` to
  repack the temp show dir into the sandbox `.xsqz`. The view
  model then calls `copySandboxBackToOriginal(…)` which uses
  `NSFileCoordinator.coordinate(writingItemAt:.forReplacing)` +
  URL-based copy to write the repacked package back to the
  user-tapped iCloud / Files URL. Atomic at both ends.
- **Close**: bridge destroys the `SequencePackage` (destructor
  wipes the extraction temp dir), restores the previous show
  folder if one was configured, and the view model wipes the
  sandbox scratch dir. On fresh-install (no prior show folder),
  `isShowFolderLoaded` flips back to false so the user returns
  to the setup prompt.

### `SequencePackage::Pack` (shared desktop+iPad packager)

Replaces the old wx-only `xLightsFrame::PackageSequence` that
walked a hard-coded category list and dumped externals into
`_lost/`. New wx-free packager in `src-core/render/`:

- Walks `SequenceMedia.GetAllMediaPaths()` (Image / SVG /
  Shader / TextFile / BinaryFile / Video), every model /
  view-object's `GetFileReferences()`, and Matrix-face images
  via `Model::GetFaceFiles(all:true)`. `xlights_rgbeffects.xml`
  and `xlights_networks.xml` are derived from `showDir` — no
  need to pass them explicitly.
- Files under `showDir` keep their show-relative path; externals
  relocate under typed subdirs (`Images/`, `Videos/`, `Shaders/`,
  `Glediators/`, `Faces/`, `Objects/`). Basename collisions
  disambiguate by subfolder (`Images/dup2/foo.png`) so filenames
  never gain a numeric suffix — `PicturesEffect` would otherwise
  misread `-1` / `_1` as an animation-sequence marker.
- Per-object group colocation for meshes: `MeshObject`'s `.obj` +
  `.mtl` + texture references share a source dir; when that dir
  is outside `showDir`, all files from the group land in the
  same `Objects/<parentBasename>/` subdir so sibling-relative
  refs (`mtllib house.mtl`, `map_Kd textures/wood.png`) still
  resolve on extract.
- Path rewrites happen on in-memory `pugi` copies of rgbeffects
  and the `.xsq` — the on-disk originals are never touched.
  Rewrite list is sorted by descending key length so longer
  paths match before shorter prefixes (no accidental inside-
  rewriting of `/foo/bar.png` within `/foo/bar.png.bak`).
- Pre-flight readability check before creating each zip entry
  (minizip has no remove-entry API, so a failed read after entry
  creation would leave a corrupt stub).
- Per-file failures (permission-denied, missing, unreadable)
  collect into `outWarnings`; Pack keeps going and produces the
  best package it can. Catastrophic failures (missing `.xsq` /
  rgbeffects, zipOpen failure, XML parse failure, final rename
  failure) return false. Desktop surfaces warnings via
  `DisplayWarning` after a successful pack so the user knows the
  package is incomplete.
- Atomic write: writes to `.xsqz.tmp`, then `std::filesystem::
  rename` swaps in place.

Desktop `PackageSequence` in `xLightsMain.cpp` now delegates
entirely to `Pack()` — no more `AddFileToZipFile`, `FixFile`,
`StripPresets` helpers.

### G-1 — `NSFileCoordinator` on sequence writes

- `SequencerViewModel` has a `coordinatedWrite(at:_:)` helper
  wrapping bridge calls in
  `NSFileCoordinator.coordinate(writingItemAt:options:.forReplacing)`.
- `saveSequence()`, `saveSequenceAs(path:)`, and
  `tickAutosave()` all route through it. Empty-path new-unsaved
  case bypasses the coordinator.

### G-2 — iCloud ubiquity status in the picker

- `UbiquityStatus` enum (`.local` / `.downloaded` /
  `.downloading` / `.notDownloaded`) + helper that reads
  `isUbiquitousItemKey`, `ubiquitousItemDownloadingStatusKey`,
  `ubiquitousItemIsDownloadingKey`.
- `UbiquityBadge` view renders `icloud` / `icloud.and.arrow.down`
  icons. `.local` suppresses the badge.
- `SequencePickerView`'s Recent and show-folder rows carry the
  badge. `openWithDownloadIfNeeded(path:status:)` triggers
  `startDownloadingUbiquitousItem` for `.notDownloaded` taps and
  polls up to ~5 s for completion.

### G-4 — Background lifecycle

- `quiesceForInactive()` pauses playback + scrub.
- `shutdownForBackground()` cancels background render polling,
  calls `abortRenderAndWait(3.0)`, and stops controller output
  (iOS throttles background network traffic so sACN/ArtNet/DDP
  streams become unreliable; halt cleanly). User re-enables via
  the toolbar output toggle on foreground.

### Preferences rename (shared desktop+iPad)

- "Exclude Presets" checkbox renamed to "Exclude Videos" —
  the old option stripped a rgbeffects element that no longer
  carries presets, so it was a no-op. "Exclude Videos" addresses
  the legitimate copyright concern that motivated the original
  name. Config key migrated silently; the new key defaults to
  false.

## Deferred

- `.piz` / bare `.zip` files are NOT registered as UTIs — we
  handle them in-core via `SequencePackage` if a user picks one
  explicitly, but we don't want Files to route every arbitrary
  `.zip` tap into xLights.
- Save-back for `.xsqz` opened from non-Files-provider sources
  (e.g. deep-linked via `itms-services://`) is untested but
  should work through the same sandbox round-trip.
