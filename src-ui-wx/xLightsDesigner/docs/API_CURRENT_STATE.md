# xLightsDesigner API Current State

Baseline:
- xLights release tag: `2026.08`
- xLightsDesigner branch: `xld-2026.08-migration`

## Architecture

xLightsDesigner uses generated FSEQ DataLayers, not native xLights effect authoring.

Primary flow:
1. Launch xLights with `XLIGHTS_DESIGNER_ENABLED=1`.
2. Read show folder, layout, channel map, timing tracks, media, and sequence state.
3. Generate an adjacent `<sequence>.xld.fseq` and `<sequence>.xld.manifest.json`.
4. Attach or update the `XLD AI Layer` DataLayer.
5. Save the `.xsq`, render/save the final controller `.fseq`, and validate sync health.

Native effect-control endpoints are intentionally not exposed.

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

The XLD DataLayer manifest carries display snapshot, channel-map fingerprint, timing revision, generated FSEQ path, size, and channel/frame dimensions. `sequence/sync-health` validates this manifest against the current xLights sequence and layout.

## Layout

Routes:
- `GET /xlightsdesigner/api/layout/models`
- `GET /xlightsdesigner/api/layout/submodels`
- `GET /xlightsdesigner/api/layout/model-nodes`
- `GET /xlightsdesigner/api/layout/channel-map`
- `GET /xlightsdesigner/api/layout/scene`
- `GET /xlightsdesigner/api/layout/settings`
- `GET /xlightsdesigner/api/layout/group-members`
- `POST /xlightsdesigner/api/layout/models/custom`

`layout/channel-map` is the authoritative channel mapping source for generated FSEQ alignment.

## Timing

Routes:
- `GET /xlightsdesigner/api/timing/tracks`
- `GET /xlightsdesigner/api/timing/marks`
- `POST /xlightsdesigner/api/timing/ensure-track`
- `POST /xlightsdesigner/api/timing/add-marks`

Timing track names are labels. Consumers should use track descriptions and explicit metadata from xLightsDesigner handoff files for semantic meaning.

## Media And Show Folder

Routes:
- `GET /xlightsdesigner/api/media/current`
- `GET /xlightsdesigner/api/media/directories`
- `POST /xlightsdesigner/api/media/show-directory`
- `POST /xlightsdesigner/api/media/request-show-directory-access`
- `POST /xlightsdesigner/api/media/paths/validate-access`
- `GET /xlightsdesigner/api/media/audio/capabilities`

Show-folder switching is explicit and should be followed by fresh display, channel-map, timing, and sequence state readback.

## Elements

Routes:
- `GET /xlightsdesigner/api/elements/summary`
- `GET /xlightsdesigner/api/elements/display-order`
- `POST /xlightsdesigner/api/elements/display-order`

Element APIs support ordering/readback. They do not expose native effect authoring.
