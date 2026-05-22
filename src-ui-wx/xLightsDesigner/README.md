# xLightsDesigner Integration Surface

This directory owns the xLightsDesigner-specific API and runtime integration.

The production architecture is direct channel output through xLights DataLayers:
- xLightsDesigner reads display, timing, media, and sequence state through the owned API.
- xLightsDesigner writes an adjacent `.xld.fseq` file.
- xLights attaches that file as an XLD DataLayer and renders the final controller `.fseq`.
- Native xLights effect-block authoring is intentionally out of scope for this API.

Activation:
- The integration is inert unless launched with `XLIGHTS_DESIGNER_ENABLED=1`.
- Self-test and smoke modes are env-only: `XLIGHTS_DESIGNER_SELF_TEST=1`, `XLIGHTS_DESIGNER_SMOKE=1`.
- Do not add file-marker activation paths; release builds must not expose the listener accidentally.

Host boundary:
- New xLightsDesigner-owned code belongs in this directory.
- Existing xLights files should receive only narrow, gated hooks needed for lifecycle, noninteractive prompt handling, DataLayer rendering, or display readback.
- Hooks must be inert during normal xLights use.

Reference:
- Current route surface: `docs/API_CURRENT_STATE.md`
- Host ownership boundary: `docs/OWNERSHIP_BOUNDARY.md`
