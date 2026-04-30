# xLightsDesigner API Current State

Status date:
- 2026-04-26

Repository:
- `/Users/robterry/xLights-2026.06`
- branch: `xld-2026.06-migration`

Reference points:
- baseline fork commit: `9bcfba7f44462ed8038dd3532e8739c8a19c4f7d`
- current API state commit: `8c320eef6`

## Summary

The owned xLightsDesigner API is now a real additive integration surface inside xLights.

What exists now:
- a dedicated owned code boundary under `xLights/xLightsDesigner/`
- a separate owned listener and transport layer
- a queued job runtime for mutating operations
- a health/readiness model
- read APIs for sequence, timing, media, layout, elements, and effects
- sequence lifecycle APIs for open, create, and save
- mutation APIs for timing and effects
- higher-level sequencing APIs for composed apply operations
- owned API modal/window state reporting through `/health`
- launch-time prompt suppression for known owned-enabled startup paths
- rgbeffects and effect-preset autosave prompt policy in owned-enabled mode

What does not exist yet:
- full app migration to the owned API for every xLights operation
- a finalized production request contract for all future plan shapes
- removal of the temporary open-sequence tracing added during stability work

## Ownership Model

Rules currently enforced:
- all new xLightsDesigner-owned API code lives under `xLights/xLightsDesigner/`
- legacy xLights automation under `xLights/automation/...` remains baseline
- xLightsDesigner integration is additive, not a replacement for xLights automation
- the owned API is enabled only when xLightsDesigner integration is explicitly turned on

Primary integration entrypoint:
- `xLights/xLightsDesigner/DesignerIntegration.h`

Minimal host hooks outside the owned directory:
- `xLights/xLightsApp.cpp`
- `xLights/xLightsApp.h`
- `xLights/TabSequence.cpp`

## Runtime Model

Enablement:
- `XLIGHTS_DESIGNER_ENABLED=1`

Optional diagnostics:
- `XLIGHTS_DESIGNER_SELF_TEST=1`
- `XLIGHTS_DESIGNER_SMOKE=1`
- `XLIGHTS_DESIGNER_PORT=<port>`
- `XLIGHTS_DESIGNER_STARTUP_SETTLE_MS=<ms>`

Listener:
- default port: `49915`
- owned base path: `/xlightsdesigner/api`
- implemented in `xLights/xLightsDesigner/DesignerApiListener.h`
- no longer depends on the legacy `wxHTTPServer` event-loop path for owned transport

Runtime queue:
- implemented in `xLights/xLightsDesigner/DesignerApiRuntime.h`
- mutating requests are submitted as jobs and executed serially
- jobs run through a dedicated worker and marshal UI-bound work to the xLights main thread as needed

Health model:
- `GET /xlightsdesigner/api/health`
- reports:
  - listener state
  - worker state
  - queue depth
  - active job id
  - submitted/completed/failed counts
  - app readiness
  - startup settle state

Job model:
- `GET /xlightsdesigner/api/jobs/get?jobId=...`
- queued mutation routes return `202` with a `jobId`
- caller polls `jobs.get` for final success or failure

Startup gating:
- startup now distinguishes listener readiness from true app readiness
- `sequence.open` and other sensitive flows can return `409 APP_NOT_READY` until the configured settle window has elapsed

## Owned Route Surface

### Runtime
- `GET /xlightsdesigner/api/health`
- `GET /xlightsdesigner/api/jobs/get`

### Sequence
- `GET /xlightsdesigner/api/sequence/open`
- `GET /xlightsdesigner/api/sequence/revision`
- `GET /xlightsdesigner/api/sequence/settings`
- `POST /xlightsdesigner/api/sequence/open`
- `POST /xlightsdesigner/api/sequence/create`
- `POST /xlightsdesigner/api/sequence/save`
- `POST /xlightsdesigner/api/sequence/close`
- `POST /xlightsdesigner/api/sequence/render-current`
- `POST /xlightsdesigner/api/sequence/render-samples`

### Timing
- `GET /xlightsdesigner/api/timing/tracks`
- `GET /xlightsdesigner/api/timing/marks`
- `POST /xlightsdesigner/api/timing/ensure-track`
- `POST /xlightsdesigner/api/timing/add-marks`

### Media
- `GET /xlightsdesigner/api/media/current`
- `GET /xlightsdesigner/api/media/directories`

### Layout
- `GET /xlightsdesigner/api/layout/models`
- `GET /xlightsdesigner/api/layout/scene`
- `GET /xlightsdesigner/api/layout/settings`
- `GET /xlightsdesigner/api/layout/group-members`
- `POST /xlightsdesigner/api/layout/models/custom`

`layout/settings` currently reports:
- layout dirty flags
- `modelsChangeCount`
- `showDirectory`
- saved file paths and last modified timestamps for:
  - `xlights_rgbeffects.xml`
  - networks config

`layout/scene` returns model/group geometry and channel ranges for render-feedback sampling. It is the preferred layout read for sequence-agent observation and critique.

`sequence/render-samples` reads packed channel samples from the most recent rendered `.fseq`. Call `sequence/render-current` first when the current sequence has changed; otherwise the endpoint can return `409 RENDER_SAMPLES_UNAVAILABLE`.

### Elements
- `GET /xlightsdesigner/api/elements/summary`
- `GET /xlightsdesigner/api/elements/display-order`
- `POST /xlightsdesigner/api/elements/display-order`

### Effects
- `GET /xlightsdesigner/api/effects/window`
- `POST /xlightsdesigner/api/effects/add-effect`
- `POST /xlightsdesigner/api/effects/clear-window`
- `POST /xlightsdesigner/api/effects/apply-batch`
- `POST /xlightsdesigner/api/effects/update`
- `POST /xlightsdesigner/api/effects/delete`
- `POST /xlightsdesigner/api/effects/delete-layer`
- `POST /xlightsdesigner/api/effects/reorder-layer`
- `POST /xlightsdesigner/api/effects/compact-layers`

### Sequencing
- `POST /xlightsdesigner/api/sequencing/apply-window-plan`
- `POST /xlightsdesigner/api/sequencing/apply-batch-plan`

## Route Semantics

### Read routes
Read routes execute synchronously and return data immediately.

Primary read coverage:
- current open sequence and revision token
- sequence settings and media attachment
- timing tracks and timing marks
- layout model inventory
- element summaries and effect layers
- effects in an explicit time window

### Queued mutation routes
The following routes are job-backed and return `202 Accepted` with a `jobId`:
- `sequence.open`
- `sequence.create`
- `sequence.save`
- `timing.addMarks`
- `effects.addEffect`
- `effects.clearWindow`
- `effects.applyBatch`
- `effects.clone`
- `effects.update`
- `effects.delete`
- `effects.deleteLayer`
- `effects.reorderLayer`
- `effects.compactLayers`
- `elements.setDisplayOrder`
- `sequencing.applyWindowPlan`
- `sequencing.applyBatchPlan`

`timing.ensureTrack` is still a narrow single-purpose mutation route and remains directly exposed as part of the owned surface.

`effects.clone` preserves existing sequence content by default. Missing target layers are allocated automatically, but occupied target layer/time windows are rejected atomically with `TARGET_WINDOW_OCCUPIED` before any clone effects are created.

## High-Level Sequencing Contracts

### `sequencing.applyWindowPlan`
Purpose:
- create or reuse a timing track
- add timing marks
- clear a target effect window
- add one effect

Important contract:
- this route rejects partial structure-track creation
- if the request is trying to build a structure track, it must supply the full timing context, not a single mark
- current error code for partial context:
  - `FULL_TRACK_CONTEXT_REQUIRED`

### `sequencing.applyBatchPlan`
Purpose:
- create or reuse a timing track
- add the full mark set
- apply a batch of effect writes in one queued job

This is the current preferred high-level owned sequencing route for compressible plans coming from the app.

## Validated Behavior

The following behaviors were validated during this phase:

### Listener and runtime
- owned listener starts cleanly when integration is enabled
- health endpoint reports readiness and queue state
- queued job runtime remains responsive during mutating operations

### Sequence open
- `sequence.open` no longer uses the original fragile inline transport path
- open requests execute through the job model
- a host lifetime bug that caused crashes during queued open was fixed by keeping the host alive with `std::shared_ptr`

Measured open timings from live validation on `Validation-Clean-Phase1.xsq`:
- `after_close_sequence`: about `685 ms`
- `ReadFalconFile`: about `2606 ms`
- `XmlFileOpen`: about `9801 ms`
- `SeqLoadXlightsFile`: about `3900 ms`
- total `OpenSequence`: about `18877 ms`

### Sequence create
- `sequence.create` now creates a brand new `.xsq` through native xLights `NewSequence(...)` plus `SaveAsSequence(...)`
- the route is queued like the other mutating operations
- live validation created a fresh disposable sequence and verified:
  - file creation on disk
  - open sequence readback
  - media attachment
  - expected duration and frame timing
- for brand new files on macOS, access is acquired on the parent directory instead of the nonexistent target path
- when `sequence.create` closes a dirty disposable sequence in owned mode, it suppresses the native save/discard modal programmatically instead of blocking the API job
- live validation covered this by:
  - creating a disposable sequence
  - dirtying it with a timing-mark write
  - creating a second sequence successfully without a blocking save dialog

### Sequence save
- owned `sequence.save` uses native `SequenceFile::Save(...)` after validating that the current sequence path is writable through the current show folder or an explicitly trusted root
- the current owned route intentionally does not create an extra xLightsDesigner temporary save file outside native xLights save behavior

### Timing routes
Validated:
- `timing.getTracks`
- `timing.getMarks`
- `timing.ensureTrack`
- `timing.addMarks`

Important fixes made during validation:
- URL query decoding for timing track names
- consistent layer numbering in readback
- enforcement that higher-level sequencing routes must use full song-section context when creating structure tracks

### Effects routes
Validated:
- `effects.clearWindow`
- `effects.addEffect`
- `effects.getWindow`
- `effects.applyBatch`

### Layout routes
Validated:
- `layout.getModels`
- `layout.getScene`
- `layout.createCustomModel`

Important contract:
- `layout.createCustomModel` accepts body `dryRun: true`
- dry-run requests validate and echo model metadata without mutating the layout

Scale check completed:
- `effects.applyBatch` was validated with a `100`-effect batch in one request and one job

### Sequencing routes
Validated:
- `sequencing.applyWindowPlan`
- `sequencing.applyBatchPlan`

Observed successful batch-plan behaviors:
- full `XD: Song Structure` mark set creation with `15` marks
- multi-effect apply in one request
- larger realistic multi-model payload applied successfully in one job

## Native xLights Files Impacted

These are the xLights-owned files changed relative to the fork baseline.

### `xLights/xLightsApp.cpp`
Purpose of change:
- add lifecycle hook into owned integration

Current responsibilities added:
- include `xLightsDesigner/DesignerIntegration.h`
- call `xLightsDesigner::InitializeDesignerIntegration(topFrame)` during `OnInit()`
- call `xLightsDesigner::NotifyDesignerAppReady()` after frame startup via `CallAfter`
- call `xLightsDesigner::ShutdownDesignerIntegration()` during app exit

### `xLights/xLightsApp.h`
Purpose of change:
- add `OnExit()` override required for clean integration shutdown

### `xLights/TabSequence.cpp`
Purpose of change:
- apply owned-enabled policy to rgbeffects and effect-preset autosave recovery prompts

Current responsibilities added:
- include `xLightsDesigner/DesignerLaunchPolicy.h`
- when integration is enabled, avoid showing rgbeffects/effect-preset recovery prompts during automated startup
- use `XLIGHTS_DESIGNER_MODAL_POLICY=save` only when explicitly opting into autosave recovery

## Owned xLightsDesigner Files Added

### Integration and runtime
- `xLights/xLightsDesigner/DesignerIntegration.h`
- `xLights/xLightsDesigner/DesignerApiListener.h`
- `xLights/xLightsDesigner/DesignerApiRuntime.h`
- `xLights/xLightsDesigner/DesignerApiHost.h`
- `xLights/xLightsDesigner/DesignerApiHarness.h`
- `xLights/xLightsDesigner/DesignerApiSelfTest.h`
- `xLights/xLightsDesigner/DesignerApiSmoke.h`

### Transport
- `xLights/xLightsDesigner/api/transport/ApiRequest.h`
- `xLights/xLightsDesigner/api/transport/ApiResponse.h`
- `xLights/xLightsDesigner/api/transport/EndpointRouter.h`
- `xLights/xLightsDesigner/api/transport/ErrorCatalog.h`
- `xLights/xLightsDesigner/api/transport/JsonTransport.h`
- `xLights/xLightsDesigner/api/transport/RequestRouter.h`

### Parsing and validation
- `xLights/xLightsDesigner/api/parsing/ParameterReaders.h`
- `xLights/xLightsDesigner/api/parsing/RequestParser.h`
- `xLights/xLightsDesigner/api/validation/ValidationResult.h`

### Models
- `xLights/xLightsDesigner/api/models/SequenceModels.h`
- `xLights/xLightsDesigner/api/models/TimingModels.h`
- `xLights/xLightsDesigner/api/models/MediaModels.h`
- `xLights/xLightsDesigner/api/models/LayoutModels.h`
- `xLights/xLightsDesigner/api/models/ElementModels.h`
- `xLights/xLightsDesigner/api/models/EffectModels.h`
- `xLights/xLightsDesigner/api/models/SequencingModels.h`

### Services
- `xLights/xLightsDesigner/api/services/SequenceService.h`
- `xLights/xLightsDesigner/api/services/TimingService.h`
- `xLights/xLightsDesigner/api/services/MediaService.h`
- `xLights/xLightsDesigner/api/services/LayoutService.h`
- `xLights/xLightsDesigner/api/services/ElementService.h`
- `xLights/xLightsDesigner/api/services/EffectService.h`
- `xLights/xLightsDesigner/api/services/SequencingService.h`

### Handlers
- `xLights/xLightsDesigner/api/handlers/RuntimeHandler.h`
- `xLights/xLightsDesigner/api/handlers/SequenceHandler.h`
- `xLights/xLightsDesigner/api/handlers/TimingHandler.h`
- `xLights/xLightsDesigner/api/handlers/MediaHandler.h`
- `xLights/xLightsDesigner/api/handlers/LayoutHandler.h`
- `xLights/xLightsDesigner/api/handlers/ElementHandler.h`
- `xLights/xLightsDesigner/api/handlers/EffectHandler.h`
- `xLights/xLightsDesigner/api/handlers/SequencingHandler.h`

### Planning and reference docs already in the owned directory
- `xLights/xLightsDesigner/docs/API_ARCHITECTURE.md`
- `xLights/xLightsDesigner/docs/API_AUDIT_PLAN.md`
- `xLights/xLightsDesigner/docs/API_EXAMPLES.md`
- `xLights/xLightsDesigner/docs/API_INVENTORY.md`
- `xLights/xLightsDesigner/docs/API_MIGRATION_MAP.md`
- `xLights/xLightsDesigner/docs/API_REFACTOR_CHECKLIST.md`
- `xLights/xLightsDesigner/docs/OWNERSHIP_BOUNDARY.md`
- `xLights/xLightsDesigner/README.md`

## Known Limitations and Follow-Up Work

Current limitations:
- the owned API surface is working, but not all app-side xLights operations have been migrated to it yet
- `/health` now reports modal/window state, but root-cause fixes are still required for any unexpected modal surfaced by validation
- the owned API contract exists, but the final production schema for all plan shapes is not yet frozen

Recommended next cleanup work inside xLights:
- root-cause and prevent any unexpected modal surfaced by owned API validation
- document a stable production request schema for batch sequencing
- continue migrating app-side xLights operations to the owned queued routes only where the owned path is already proven stable

## Practical Conclusion

The xLights-side API work is now beyond scaffolding.
It has a live owned listener, a queued runtime, a non-trivial read surface, bulk mutation support, and a high-level sequencing batch route already validated in a running xLights session.

The xLights fork impact is still contained:
- the owned product surface lives under `xLights/xLightsDesigner/`
- non-owned source changes are limited to explicit lifecycle hooks and narrowly scoped startup prompt policy
