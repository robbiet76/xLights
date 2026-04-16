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

## Target folder structure

```text
xLightsDesigner/
  DesignerIntegration.h
  api/
    transport/
      ApiRequest.h
      ApiResponse.h
      ErrorCatalog.h
      RequestRouter.h
    parsing/
      RequestParser.h
      ParameterReaders.h
    validation/
      RequestValidator.h
      ValidationResult.h
    handlers/
      SequenceHandler.h
      SequencerHandler.h
      LayoutHandler.h
      TimingHandler.h
      MediaHandler.h
      TransactionsHandler.h
      JobsHandler.h
      SystemHandler.h
    services/
      SequenceService.h
      SequencerService.h
      LayoutService.h
      TimingService.h
      MediaService.h
      TransactionsService.h
      JobsService.h
      SystemService.h
    models/
      SequenceModels.h
      LayoutModels.h
      TimingModels.h
      TransactionModels.h
      JobModels.h
```

This is a target shape, not a requirement to create many files immediately.
Start with the minimum set of files that preserves these separations.

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

## Refactor sequence

1. inventory legacy logic worth preserving
2. group it by capability
3. define owned request/response models
4. define shared transport and validation primitives
5. implement the first handler/service pair end-to-end
6. only then add further capability modules

## First concrete target

The first implementation slice should probably be:
- `transport/ApiRequest.h`
- `transport/ApiResponse.h`
- `transport/ErrorCatalog.h`
- `parsing/RequestParser.h`
- `validation/ValidationResult.h`
- `handlers/SequenceHandler.h`
- `services/SequenceService.h`

Reason:
- sequence operations are the most central capability for xLightsDesigner
- it gives the cleanest template for the rest of the API surface

## Boundary rule

The owned API implementation must sit behind `DesignerIntegration.h`.
Core xLights should not need to understand the internal `xLightsDesigner/api/` structure.
