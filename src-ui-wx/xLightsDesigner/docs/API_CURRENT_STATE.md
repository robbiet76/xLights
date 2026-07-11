# xLightsDesigner API Current State

Baseline:
- Built into the active xLights source tree.
- API identity is capability-based, not tied to a specific xLights release.

## Architecture

xLightsDesigner uses native xLights effect blocks for editable sequencing.

Primary flow:
1. Launch xLights with `XLIGHTS_DESIGNER_ENABLED=1`.
2. Read show folder, layout, channel map, native song structure regions, timing tracks, media, and sequence state.
3. Generate a design/sequence plan in xLightsDesigner.
4. Write XLD-owned native effect blocks and layers to the `.xsq`.
5. Save the `.xsq`, render/save the final controller `.fseq`, and validate sequence state.

DataLayer routes remain available for direct FSEQ proof/readback workflows, but
native effect control is the primary editable sequencing integration.

## Runtime

Routes:
- `GET /xlightsdesigner/api/health`
- `GET /xlightsdesigner/api/capabilities`
- `GET /xlightsdesigner/api/jobs/get?jobId=...`

Mutation routes that touch xLights UI state are job-backed and return `202` with `jobId`.
Callers must poll `jobs/get` until `state` is `succeeded` or `failed`.

## Sequence

Routes:
- `GET /xlightsdesigner/api/sequence/open`
- `GET /xlightsdesigner/api/sequence/revision`
- `GET /xlightsdesigner/api/sequence/state`
- `GET /xlightsdesigner/api/sequence/settings`
- `POST /xlightsdesigner/api/sequence/settings`
- `POST /xlightsdesigner/api/sequence/open`
- `POST /xlightsdesigner/api/sequence/create`
- `POST /xlightsdesigner/api/sequence/save`
- `POST /xlightsdesigner/api/sequence/close`
- `POST /xlightsdesigner/api/sequence/focus`
- `POST /xlightsdesigner/api/sequence/render-current`
- `POST /xlightsdesigner/api/sequence/save-final-fseq`
- `POST /xlightsdesigner/api/sequence/check`
- `POST /xlightsdesigner/api/sequence/export-preview-video`
- `POST /xlightsdesigner/api/sequence/render-samples`
- `GET /xlightsdesigner/api/sequence/final-fseq`
- `GET /xlightsdesigner/api/sequence/sync-health`

## DataLayers

Routes:
- `GET /xlightsdesigner/api/sequence/data-layers`
- `POST /xlightsdesigner/api/sequence/data-layers/upsert`
- `POST /xlightsdesigner/api/sequence/data-layers/remove`
- `POST /xlightsdesigner/api/sequence/data-layers/reorder`
- `POST /xlightsdesigner/api/sequence/data-layers/validate`

The XLD DataLayer manifest carries display snapshot, channel-map fingerprint, timing revision, generated FSEQ path, size, and channel/frame dimensions. `sequence/sync-health` validates this manifest against the current xLights sequence and layout. These routes are retained for direct-channel proof workflows, not as the default editable sequence authoring path.

## Native Effects

Routes:
- `GET /xlightsdesigner/api/effects`
- `POST /xlightsdesigner/api/effects/upsert`
- `POST /xlightsdesigner/api/effects/remove`
- `GET /xlightsdesigner/api/effects/layers`
- `POST /xlightsdesigner/api/effects/layers/ensure`
- `POST /xlightsdesigner/api/effects/layers/remove`

Effect writes use xLights' native effect name, settings string, palette string,
layer index, and millisecond timing. XLD-owned effects are marked in native
settings with `XLD_OWNER` and optional `XLD_ID`; mutation endpoints refuse to
update or remove user-owned effects unless the request explicitly opts into it.
The API does not define effect-specific setting schemas.

## Layout

Routes:
- `GET /xlightsdesigner/api/layout/models`
- `GET /xlightsdesigner/api/layout/submodels`
- `GET /xlightsdesigner/api/layout/model-nodes`
- `GET /xlightsdesigner/api/layout/render-buffer-nodes`
- `GET /xlightsdesigner/api/layout/channel-map`
- `GET /xlightsdesigner/api/layout/scene`
- `GET /xlightsdesigner/api/layout/settings`
- `GET /xlightsdesigner/api/layout/group-members`
- `GET /xlightsdesigner/api/layout/preview-groups`

The layout API is read-only. It exposes existing xLights layout geometry,
submodels, group membership, channel spans, node coordinates, preview settings,
and background-image references. `layout/channel-map` is the authoritative
channel mapping source for generated FSEQ alignment.
`layout/render-buffer-nodes` materializes a requested target and xLights render
style through xLights' own render-buffer pipeline so external render boards do
not recreate buffer-style geometry in app code.

Each `layout/models` row includes `controllerCalibration`, a typed snapshot of
physical-controller/deployment metadata from the model's xLights
`ControllerConnection` and, when safely resolvable, its assigned controller.
Nullable fields remain JSON `null` when unassigned or unsupported; an inactive
connection default is never reported as an implicit 100%. Explicit model/port
brightness and gamma use `model_port_explicit` provenance. Effective values
fall back to `controller_full_control_default` only when the assigned controller
is resolved, full xLights control is active, and that controller advertises the
corresponding default capability. Other provenance values distinguish
`controller_not_resolved`, `full_control_unsupported`,
`full_control_inactive`, `controller_default_unsupported`, and `unavailable`.
`deploymentMetadataOnly=true` and `previewApplicationProven=false` make clear
that this is deployment calibration, not proof that preview rendering applied
the values. Calibration content participates in the channel-map fingerprint,
so changes invalidate dependent XLD freshness evidence.

## Timing

Routes:
- `GET /xlightsdesigner/api/timing/tracks`
- `GET /xlightsdesigner/api/timing/marks`
- `GET /xlightsdesigner/api/timing/song-structure`

Native song structure regions are the preferred section-level musical spine when
available. Timing track names are labels. xLights owns timing and song-structure
creation/editing; XLD reads regions, timing tracks, and marks as musical anchors
for design and sequencing.

## Media And Show Folder

Routes:
- `GET /xlightsdesigner/api/media/current`
- `GET /xlightsdesigner/api/media/directories`
- `POST /xlightsdesigner/api/media/show-directory`
- `POST /xlightsdesigner/api/media/request-show-directory-access`
- `POST /xlightsdesigner/api/media/paths/validate-access`
- `GET /xlightsdesigner/api/media/audio/capabilities`

Show-folder switching is explicit and should be followed by fresh display, channel-map, timing, and sequence state readback.
When active audio is available, `media/current` reports `mediaContentFingerprint`
as `xlights-audio-md5:<hex>`, using xLights' content hash of the decoded audio
samples rather than a path or file-metadata surrogate. The field is an empty
string when no valid active audio is available.

## Elements

Routes:
- `GET /xlightsdesigner/api/elements/summary`
- `POST /xlightsdesigner/api/elements/ensure`
- `GET /xlightsdesigner/api/elements/display-order`
- `POST /xlightsdesigner/api/elements/display-order`
- `GET /xlightsdesigner/api/elements/selected`
- `POST /xlightsdesigner/api/elements/selected`

Element APIs support ordering/readback and sequencer display-element selection.
