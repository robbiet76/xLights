# xLightsDesigner Integration Surface

This directory owns the xLightsDesigner-specific API and runtime integration.

The production architecture is native xLights effect control:
- xLightsDesigner reads display, timing, media, and sequence state through the owned API.
- xLightsDesigner creates and updates XLD-owned native effect blocks on xLights sequence elements.
- xLightsDesigner controls effect layers and sequencer display-element selection when needed.
- xLights owns timing track creation/editing; XLD reads timing tracks and marks.
- DataLayer routes remain available for direct FSEQ readback/proof workflows, but they are not the primary editable sequencing path.

Activation:
- The integration is inert unless launched with `XLIGHTS_DESIGNER_ENABLED=1`.
- Self-test and smoke modes are env-only: `XLIGHTS_DESIGNER_SELF_TEST=1`, `XLIGHTS_DESIGNER_SMOKE=1`.
- Do not add file-marker activation paths; release builds must not expose the listener accidentally.

Host boundary:
- New xLightsDesigner-owned code belongs in this directory.
- Existing xLights files should receive only narrow, gated hooks needed for lifecycle, noninteractive prompt handling, native effect control, DataLayer rendering, or display readback.
- Hooks must be inert during normal xLights use.

Reference:
- Current route surface: `docs/API_CURRENT_STATE.md`
- Host ownership boundary: `docs/OWNERSHIP_BOUNDARY.md`
