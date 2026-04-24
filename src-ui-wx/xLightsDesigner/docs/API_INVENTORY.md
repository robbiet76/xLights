# xLightsDesigner API Inventory

This inventory records the legacy xLights-owned automation API surface that informed the current owned API under `xLightsDesigner/api/`.

This is an audit artifact only. It is not the current route contract. For active routes, use `API_CURRENT_STATE.md`.

## Current source surface

Current xLights-owned automation API files:
- `EffectsV2Api.inl`
- `JobsV2Api.inl`
- `LayoutV2Api.inl`
- `LegacyExportPackagingApi.inl`
- `LegacyLayoutMutationApi.inl`
- `LegacyPlaybackApi.inl`
- `LegacyReadQueryApi.inl`
- `LegacyRenderTransferApi.inl`
- `LegacySequenceCoreApi.inl`
- `LegacySequencerMutationApi.inl`
- `LegacySystemControlApi.inl`
- `MediaV2Api.inl`
- `SequenceV2Api.inl`
- `SequencerV2Api.inl`
- `SystemV2Api.inl`
- `TimingAnalysisV2Api.inl`
- `TimingV2Api.inl`
- `TransactionsV2Api.inl`
- `V2RequestParsingApi.inl`
- `V2ValidationApi.inl`

## Capability classification

### Transport and shared protocol
Current files:
- `V2RequestParsingApi.inl`
- `V2ValidationApi.inl`

Owned target area:
- `api/transport/`
- `api/parsing/`
- `api/validation/`

Notes:
- these should become shared primitives, not capability files
- remove `V2` naming entirely in the owned surface

### Sequence
Current files:
- `SequenceV2Api.inl`
- `LegacySequenceCoreApi.inl`

Observed responsibilities:
- open sequence
- get open sequence state
- get revision
- create sequence
- set sequence settings
- basic sequence-level state mutation

Owned target area:
- `api/handlers/SequenceHandler.h`
- `api/services/SequenceService.h`
- `api/models/SequenceModels.h`

### Sequencer
Current files:
- `SequencerV2Api.inl`
- `LegacySequencerMutationApi.inl`
- parts of `EffectsV2Api.inl`

Observed responsibilities:
- display element ordering
- visibility/active element state
- effect grid mutation behavior
- sequencing-oriented mutations

Owned target area:
- `api/handlers/SequencingHandler.h`
- `api/services/SequencingService.h`
- `api/models/SequencingModels.h`

### Layout
Current files:
- `LayoutV2Api.inl`
- `LegacyLayoutMutationApi.inl`

Observed responsibilities:
- model inventory
- model detail
- groups and submodels
- geometry
- layout display elements
- layout mutation behavior

Owned target area:
- `api/handlers/LayoutHandler.h`
- `api/services/LayoutService.h`
- `api/models/LayoutModels.h`

### Timing
Current files:
- `TimingV2Api.inl`
- `TimingAnalysisV2Api.inl`

Observed responsibilities:
- timing track inventory
- timing marks and sections
- timing analysis-oriented endpoints

Owned target area:
- `api/handlers/TimingHandler.h`
- `api/services/TimingService.h`
- `api/models/TimingModels.h`

### Media
Current files:
- `MediaV2Api.inl`

Observed responsibilities:
- media path/state access
- media operations associated with a sequence

Owned target area:
- `api/handlers/MediaHandler.h`
- `api/services/MediaService.h`

### Transactions
Current files:
- `TransactionsV2Api.inl`

Observed responsibilities:
- apply units of change
- mutation grouping
- commit/rollback-oriented operations

Owned migration outcome:
- no active owned transaction handler/service/model exists
- current mutation orchestration uses `sequencing.applyBatchPlan` plus queued jobs
- do not restore rollback-style transaction docs unless a new owned contract is explicitly designed

### Jobs
Current files:
- `JobsV2Api.inl`

Observed responsibilities:
- long-running work state
- job polling/status

Owned outcome:
- no separate jobs module exists
- queued work is owned by runtime infrastructure and exposed through `GET /jobs/get`

### System
Current files:
- `SystemV2Api.inl`
- `LegacySystemControlApi.inl`
- `LegacyPlaybackApi.inl`

Observed responsibilities:
- system state
- playback and control behaviors
- environmental/process-level operations

Owned outcome:
- runtime health and job polling are exposed through `RuntimeHandler`
- playback/export/packaging are not active owned xLightsDesigner routes

### Effects
Current files:
- `EffectsV2Api.inl`
- parts of `LegacySequencerMutationApi.inl`

Observed responsibilities:
- effect inspection
- effect detail retrieval
- effect mutation support

Owned target area:
- `api/handlers/EffectHandler.h`
- `api/services/EffectService.h`
- `api/models/EffectModels.h`

Recommendation:
- keep effect inspection/mutation in the effect capability
- keep higher-level orchestration in the sequencing capability

### Export / render transfer / packaging
Current files:
- `LegacyExportPackagingApi.inl`
- `LegacyRenderTransferApi.inl`

Observed responsibilities:
- export and packaging flows
- render transfer / handoff style operations

Owned outcome:
- not active as owned xLightsDesigner routes
- render feedback currently uses sequence render/sample routes plus layout scene data

### Read-query compatibility
Current files:
- `LegacyReadQueryApi.inl`

Observed responsibilities:
- mixed read-only compatibility endpoints

Owned target area:
- do not preserve as a category
- redistribute each read endpoint into the owning capability

## Naming cleanup required

Current names that should not survive into the owned API:
- `V2*`
- `Legacy*`
- `*MutationApi`
- `*CoreApi`
- `*ReadQueryApi`

Replace with capability names such as:
- `SequenceHandler`
- `SequencingService`
- `LayoutHandler`
- `TimingService`
- `RequestParser`
- `ValidationResult`

## Owned migration status

Implemented owned areas now include sequence, sequencing, layout, timing, media, effects, elements, runtime health, and queued jobs. See `API_ARCHITECTURE.md` for the current file structure and `API_CURRENT_STATE.md` for active routes.
