# xLightsDesigner API Migration Map

This document maps the current xLights-owned automation surface to the future owned `xLightsDesigner/api/` architecture.

## Shared infrastructure

Current source:
- `V2RequestParsingApi.inl`
- `V2ValidationApi.inl`

Owned target:
- `transport/ApiRequest.h`
- `transport/ApiResponse.h`
- `transport/ErrorCatalog.h`
- `parsing/RequestParser.h`
- `parsing/ParameterReaders.h`
- `validation/RequestValidator.h`
- `validation/ValidationResult.h`

## Sequence

Current source:
- `SequenceV2Api.inl`
- `LegacySequenceCoreApi.inl`

Owned target:
- `handlers/SequenceHandler.h`
- `services/SequenceService.h`
- `models/SequenceModels.h`

## Sequencer and effects

Current source:
- `SequencerV2Api.inl`
- `EffectsV2Api.inl`
- `LegacySequencerMutationApi.inl`

Owned target:
- `handlers/SequencerHandler.h`
- `services/SequencerService.h`
- `models/SequenceModels.h`

## Layout

Current source:
- `LayoutV2Api.inl`
- `LegacyLayoutMutationApi.inl`

Owned target:
- `handlers/LayoutHandler.h`
- `services/LayoutService.h`
- `models/LayoutModels.h`

## Timing

Current source:
- `TimingV2Api.inl`
- `TimingAnalysisV2Api.inl`

Owned target:
- `handlers/TimingHandler.h`
- `services/TimingService.h`
- `models/TimingModels.h`

## Media

Current source:
- `MediaV2Api.inl`

Owned target:
- `handlers/MediaHandler.h`
- `services/MediaService.h`

## Transactions

Current source:
- `TransactionsV2Api.inl`

Owned target:
- `handlers/TransactionsHandler.h`
- `services/TransactionsService.h`
- `models/TransactionModels.h`

## Jobs

Current source:
- `JobsV2Api.inl`

Owned target:
- `handlers/JobsHandler.h`
- `services/JobsService.h`
- `models/JobModels.h`

## System / playback / export

Current source:
- `SystemV2Api.inl`
- `LegacySystemControlApi.inl`
- `LegacyPlaybackApi.inl`
- `LegacyExportPackagingApi.inl`
- `LegacyRenderTransferApi.inl`

Owned target:
- `handlers/SystemHandler.h`
- `services/SystemService.h`

## Explicit cleanup rules

When migrating into owned code:
- do not preserve `V2` naming
- do not preserve `Legacy` naming
- do not preserve historical file boundaries unless they align with capability boundaries
- compatibility adapters, if required later, should live in a narrow compatibility seam and not shape the owned API layout
