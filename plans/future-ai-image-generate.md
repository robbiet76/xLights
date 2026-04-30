# Future — AI Image Generate

Not in the first-pass Phase C scope. Captured here so the desktop
parity doesn't get forgotten.

## Gap

**G33 — AI image generation.** Desktop's `PicturesPanel` and
`ManageMediaPanel` both expose an "Generate Image" entry that
calls xLights' AI service and drops the resulting PNG straight
into the sequence's image cache. iPad has no equivalent.

## Why deferred

- Depends on porting the AI service client to iOS — no bridge yet
  today. Shares the same infrastructure need as
  [`future-ai-palette-generate.md`](future-ai-palette-generate.md),
  which tracks the palette-generate version of the same plumbing.
  Bring both up together when the service client lands.
- Niche workflow on iPad — casual users won't reach for it, and
  the file-picker reuse (G27) already covers the "I already have
  this image somewhere" path.

## Scope when we come back

- iOS bridge for the shared AI service client (currently wx-
  embedded on desktop). Same work unblocks G17-AI palette
  generation.
- SwiftUI dialog analogue to the desktop image-gen prompt: text
  prompt, size / aspect picker, model picker, preview of the
  returned image, Save (writes into `<showFolder>/Images/` and
  commits the path to the picking effect's filename key).
- Entry points:
  - Per-effect picker (Pictures) — a "Generate Image…" button
    alongside "Browse Files…" in `MediaPickerSheet`.
  - `MediaManagerSheet` toolbar overflow — "Generate Image…"
    for arbitrary-context creation (no specific effect bound).
- The freshly-generated file gets dropped into the show folder's
  `Images/` subdirectory with a descriptive filename, then
  routed through the normal per-effect relocation path so the
  stored reference is show-relative.
