# Phase H — App Store readiness

**Status: H-0/H-1/H-2/H-3 ✓ complete; H-4/H-5 remaining
(organizational).**

Apple Developer team: Kulp Lights LLC (existing). iPad app
ships as an additional platform on the existing `org.xlights`
App Store Connect record (Universal Purchase), not as a
separate listing. iOS platform added to the xLights app in
App Store Connect 2026-04-22.

## What landed

### H-0 — Unified bundle ID

- iPad target `PRODUCT_BUNDLE_IDENTIFIER = org.xlights` (shared
  with Mac) across Debug / Release / Archive configs. Universal
  Purchase gives customers automatic cross-platform access
  under one App Store record. In-app-purchase catalog (when
  added later) lives on the unified record with per-platform
  availability flags so iPad-only IAPs can be scoped without
  affecting Mac.

### H-1 — App icon + launch screen

- `src-iPad/AppIcon.icon/` — Icon Composer bundle (Liquid
  Glass) with Light / Dark / Tintable appearance variants
  flattened over the gradient background; source artwork is
  the xLights logo scaled 0.9 with `glass: true` layer. Flat
  1024×1024 fallback for pre-iOS-26. `ASSETCATALOG_COMPILER_
  APPICON_NAME = AppIcon` finds it via the `src-iPad/`
  synchronized root group (no pbxproj edit needed).
- `src-iPad/Assets.xcassets/LaunchBackground.colorset` —
  light (white) / dark (#1A1A1A) variants.
- `src-iPad/Assets.xcassets/LaunchLogo.imageset` — 1200×460
  logo tagged @2x (renders 600×230 pt centered on any iPad).
- `UILaunchScreen` dict in Info.plist wires the color + image
  with `UIImageRespectsSafeAreaInsets`. Removed
  `INFOPLIST_KEY_UILaunchScreen_Generation = YES` on all three
  iPad configs — our explicit dict is authoritative.

### H-2 — Privacy manifest

- `macOS/Assets/xLights-iPad/PrivacyInfo.xcprivacy` declaring
  only what xLights actually does:
  - `NSPrivacyTracking = false`
  - `NSPrivacyTrackingDomains = []`
  - `NSPrivacyCollectedDataTypes = []`
  - `NSPrivacyAccessedAPITypes`:
    - FileTimestamp (C617.1) — autosave mtime checks, media
      cache invalidation
    - UserDefaults (CA92.1) — SwiftUI `@AppStorage`
    - SystemBootTime (35F9.1) — `CACurrentMediaTime` /
      `CFAbsoluteTimeGetCurrent` for elapsed-time measurement
- Wired into the `xLights-iPad` target's Resources phase via
  pbxproj (standalone file reference + build file entry).

### H-3 — Network + encryption Info.plist keys

- `NSLocalNetworkUsageDescription` — honest copy about
  controller discovery (sACN / ArtNet / DDP / FPP) even though
  the UI hides output for MVP.
- `NSBonjourServices = ["_fppd._udp"]` — the only Bonjour
  service the core advertises / browses for today.
- `ITSAppUsesNonExemptEncryption = false` — xLights uses only
  Apple-provided HTTPS/TLS (NSURLSession, Bonjour). Stops
  TestFlight's per-build encryption prompt.

### Xcode Cloud infrastructure

Not numbered but necessary for H-4:

- `ci_scripts/ci_pre_xcodebuild.sh` fetches the dependency
  tarball (via extracted `macOS/scripts/download_deps` — shared
  with local developer builds). Download runs BEFORE xcodebuild
  starts because Xcode validates XCFramework references during
  `CreateBuildDescription`, which is before any build-phase
  script can execute. Retry loop handles occasional GitHub
  mid-transfer failures on the large (~500 MB) tarball.
- `xLights-core` → `xLights-Apple-core` target dependency edge
  added so the downloader-owning target always completes
  before `src-core/` compiles.
- Bundle version stamping moved into shared
  `macOS/scripts/set_bundle_version` (read
  `src-core/xLightsVersion.h`, compute `CFBundleVersion` from
  `YYMMBBB`). Mac target's `mac_fix_dylibs` delegates to it;
  iPad target has a new "Set Bundle Version" build phase that
  also delegates. iPad builds now report as `2026.06.1 (2606001)`
  instead of the raw Xcode-default `260000`.
- ANGLE `libEGL.xcframework` / `libGLESv2.xcframework`
  Info.plists patched at dep-download time to add missing
  `CFBundleShortVersionString` — fixes App Store ITMS-90057
  rejection. Upstream fix landed in
  `xLights-macOS-dependencies/submodules/build_angle.sh` so
  future tarballs ship correct plists.

## H-4 — TestFlight + beta group

Workflow builds to App Store Connect successfully; Universal
Purchase record accepts iOS builds. Remaining:

- **Internal group** — invite primary reviewers, enable
  "Automatic Distribution" so every Xcode Cloud archive lands
  in the test queue without manual promotion.
- **External group** — up to 10,000 email addresses. First
  external build requires Beta App Review (~24–48 hour
  turnaround). Fill in:
  - Test information (feedback email, optional marketing URL)
  - Description of what to test
  - Beta App Review Information (contact info, notes,
    demo-account-not-required flag)

Both are App Store Connect UI work — nothing further on the
code side.

## H-5 — Submission

- Screenshots (iPad landscape + portrait, 12.9" and 11").
- Description / promotional text.
- Keywords.
- Age rating questionnaire.
- Privacy policy URL.
- Support URL.
- Final metadata review.

All organizational. The binary is ready.
