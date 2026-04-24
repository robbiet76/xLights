# xLightsDesigner API Migration Map

This document maps the historical xLights-owned automation surface to the current owned `xLightsDesigner/api/` architecture. It should not be read as a future plan.

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
- `handlers/SequencingHandler.h`
- `services/SequencingService.h`
- `models/SequencingModels.h`
- `handlers/EffectHandler.h`
- `services/EffectService.h`
- `models/EffectModels.h`

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

Owned migration outcome:
- no active owned transaction module exists
- current apply semantics use `handlers/SequencingHandler.h`, `services/SequencingService.h`, and `models/SequencingModels.h`
- app-side apply validation uses owned batch plans and revision tokens rather than rollback transactions

## Jobs

Current source:
- `JobsV2Api.inl`

Owned outcome:
- no separate jobs module exists
- queued work is owned by runtime infrastructure and exposed through `GET /jobs/get`

## System / playback / export

Current source:
- `SystemV2Api.inl`
- `LegacySystemControlApi.inl`
- `LegacyPlaybackApi.inl`
- `LegacyExportPackagingApi.inl`
- `LegacyRenderTransferApi.inl`

Owned outcome:
- runtime health and job polling are exposed through `RuntimeHandler`
- playback/export/packaging are not active owned xLightsDesigner routes

## Explicit cleanup rules

When migrating into owned code:
- do not preserve `V2` naming
- do not preserve `Legacy` naming
- do not preserve historical file boundaries unless they align with capability boundaries
- compatibility adapters, if required later, should live in a narrow compatibility seam and not shape the owned API layout
