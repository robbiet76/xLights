# xLightsDesigner API Inventory

This inventory classifies the current xLights-owned automation API surface so it can be re-expressed as a clean owned API under `xLightsDesigner/api/`.

This is an audit artifact only.
It does not imply that the owned API should preserve these file names or boundaries.

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
- `api/handlers/SequencerHandler.h`
- `api/services/SequencerService.h`
- `api/models/SequenceModels.h`

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

Owned target area:
- `api/handlers/TransactionsHandler.h`
- `api/services/TransactionsService.h`
- `api/models/TransactionModels.h`

### Jobs
Current files:
- `JobsV2Api.inl`

Observed responsibilities:
- long-running work state
- job polling/status

Owned target area:
- `api/handlers/JobsHandler.h`
- `api/services/JobsService.h`
- `api/models/JobModels.h`

### System
Current files:
- `SystemV2Api.inl`
- `LegacySystemControlApi.inl`
- `LegacyPlaybackApi.inl`

Observed responsibilities:
- system state
- playback and control behaviors
- environmental/process-level operations

Owned target area:
- `api/handlers/SystemHandler.h`
- `api/services/SystemService.h`

### Effects
Current files:
- `EffectsV2Api.inl`
- parts of `LegacySequencerMutationApi.inl`

Observed responsibilities:
- effect inspection
- effect detail retrieval
- effect mutation support

Owned target area:
- either a separate `EffectsHandler` if effect complexity warrants it
- or fold into `SequencerHandler` and `SequencerService`

Recommendation:
- start with effects folded into sequencer capability
- split only if the owned effect surface becomes large enough to justify it

### Export / render transfer / packaging
Current files:
- `LegacyExportPackagingApi.inl`
- `LegacyRenderTransferApi.inl`

Observed responsibilities:
- export and packaging flows
- render transfer / handoff style operations

Owned target area:
- likely `api/handlers/SystemHandler.h` initially
- split into export/render capabilities later if needed

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
- `SequencerService`
- `LayoutHandler`
- `TimingService`
- `RequestParser`
- `ValidationResult`

## First owned migration slice

Recommended first owned slice:
- Sequence
- shared transport/parsing/validation primitives needed for Sequence

Reason:
- sequence operations are central
- they define the request, error, and lifecycle patterns the rest of the API will follow
- they are the cleanest place to prove the owned architecture before tackling broader mutation surfaces
