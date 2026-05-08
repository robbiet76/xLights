# xLightsDesigner API Architecture

Goal:
- define the owned `xLightsDesigner/api/` surface as a clean first-class API implementation
- avoid carrying forward historical xLights automation structure
- keep the architecture readable, capability-based, and easy to extend

## Core principles

1. Owned API code is not a migration layer
- it should read as a product surface built intentionally for xLightsDesigner
- do not preserve legacy file fragmentation unless there is a clear engineering need

2. No historical `V2` naming
- do not use `V2` in owned file names, namespaces, classes, functions, or docs
- versioning, if ever needed, belongs at the protocol or schema layer, not in internal module names

3. Capability-based organization
- organize around what the API does
- do not organize around old transport or xlDo command history

4. Transport, parsing, validation, and execution must be separated
- request parsing should not execute commands
- validation should not own command side effects
- handlers should not own transport framing

5. Compatibility is a seam, not the architecture
- if xLights-owned automation ever needs to bridge into xLightsDesigner, keep that in a narrow adapter layer
- do not let compatibility concerns dictate internal owned layout

## Current folder structure

```text
xLightsDesigner/
  DesignerIntegration.h
  DesignerApiHost.h
  DesignerApiHarness.h
  DesignerApiRuntime.h
  api/
    transport/
      ApiRequest.h
      ApiResponse.h
      ErrorCatalog.h
      EndpointRouter.h
      JsonTransport.h
      RequestRouter.h
    parsing/
      RequestParser.h
      ParameterReaders.h
    validation/
      ValidationResult.h
    handlers/
      SequenceHandler.h
      SequencingHandler.h
      LayoutHandler.h
      TimingHandler.h
      MediaHandler.h
      EffectHandler.h
      ElementHandler.h
      RuntimeHandler.h
    services/
      SequenceService.h
      SequencingService.h
      LayoutService.h
      TimingService.h
      MediaService.h
      EffectService.h
      ElementService.h
      EffectMetadataStatus.h
    models/
      SequenceModels.h
      LayoutModels.h
      TimingModels.h
      MediaModels.h
      SequencingModels.h
      EffectModels.h
      ElementModels.h
```

This is the implemented owned surface as of the 2026.06 migration branch. Do not reintroduce transaction handler/service/model files unless a new owned contract explicitly requires them; current mutation orchestration uses `sequencing.applyBatchPlan` and queued jobs.

## Module responsibilities

### `transport/`
Owns:
- request envelope
- response envelope
- error codes
- top-level routing dispatch

Does not own:
- xLights business logic
- parsing details for individual commands

### `parsing/`
Owns:
- extracting typed parameters from requests
- normalizing inputs into internal request models

Does not own:
- policy validation
- execution

### `validation/`
Owns:
- semantic validation
- missing-field checks
- state precondition checks
- conflict detection before execution

Does not own:
- mutation
- response formatting beyond validation output

### `handlers/`
Owns:
- command-level orchestration for a capability area
- mapping validated requests to service calls

Does not own:
- low-level xLights operations directly if a service boundary is available
- transport framing

### `services/`
Owns:
- calls into xLights host objects
- actual business operations on sequence, layout, timing, transactions, media, jobs, system state

Does not own:
- transport response shaping
- command parsing

### `models/`
Owns:
- internal typed request and response structures
- capability-specific domain models

Does not own:
- transport serialization behavior beyond simple conversion helpers

## Naming rules

Use names like:
- `SequenceHandler`
- `TimingService`
- `RequestParser`
- `ValidationResult`
- `ApiResponse`

Do not use names like:
- `SequenceV2Api`
- `LegacySequencerMutationApi`
- `V2ValidationApi`
- `DesignerLegacyPlaybackApi`

If an adapter is required for compatibility, name it explicitly as an adapter, for example:
- `LegacyAutomationAdapter`
- `XlDoCompatibilityAdapter`

## Error and response policy

Owned APIs should converge on one response model:
- one request envelope shape
- one success envelope shape
- one error envelope shape
- one validation result model

Do not spread response conventions across capability files.

## Refactor status

Implemented:
- shared request/response/error transport
- owned endpoint router and JSON transport
- sequence open/create/save/close/revision/settings/render-current/render-samples
- layout models/settings/group-members/scene
- timing tracks/marks/ensure-track/add-marks
- media current/directories
- effects window/add/clear/apply-batch
- sequencing apply-window-plan/apply-batch-plan
- runtime health and job polling

Still active cleanup:
- keep `API_CURRENT_STATE.md` synchronized with route additions/removals
- prefer route-specific documentation updates when behavior changes
- remove stale references to future transaction modules or legacy rollback semantics from active docs

## Current proof-loop focus

The current app integration depends on the owned render-feedback path:
- `POST /sequence/render-current`
- `POST /sequence/render-samples`
- `GET /layout/scene`

These routes must remain smoke-covered and documented because native Review apply uses them for backup, render, observation, critique, and revision evidence.

## Boundary rule

The owned API implementation must sit behind `DesignerIntegration.h`.
Core xLights should not need to understand the internal `xLightsDesigner/api/` structure.
