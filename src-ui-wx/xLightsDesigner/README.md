# xLightsDesigner Integration Surface

This directory owns xLightsDesigner-specific integration code.

Current architecture:
- baseline xLights automation remains under `xLights/automation/`
- xLightsDesigner integration is additive, not a replacement for xLights-owned automation
- the only active host seam is a minimal lifecycle hook in `xLightsApp`

Current active entrypoint:
- `DesignerIntegration.h`
  - startup registration via `InitializeDesignerIntegration(...)`
  - shutdown via `ShutdownDesignerIntegration()`

Rules:
- new xLightsDesigner-owned code lives here
- existing xLights automation should stay baseline unless there is a narrowly justified host hook
- hooks must be inert by default and must not interfere with normal xLights behavior
- owned API code should be built as a clean first-class surface, not as a carry-over from legacy xLights automation
- do not use `V2` naming in the owned xLightsDesigner API surface

Next planned refactor:
- see `docs/API_AUDIT_PLAN.md`
- perform a full audit of the future `xLightsDesigner/api/` folder before building out more owned API functionality

Current current-state API reference:
- see `docs/API_CURRENT_STATE.md`

Current in-process harness:
- `DesignerApiHarness.h`
  - invoke owned commands directly
  - invoke canonical owned endpoints directly
  - returns JSON-ready responses without relying on xLights-owned automation

Endpoint examples:
- see `docs/API_EXAMPLES.md`


Owned self-test toggle:
- set `XLIGHTS_DESIGNER_SELF_TEST=1` to run the header-only xLightsDesigner self-tests during integration initialization
- current scope covers endpoint mapping and transport normalization

Owned smoke toggle:
- set `XLIGHTS_DESIGNER_SMOKE=1` to run a small in-process owned API smoke pass during integration initialization
- smoke output is logged and uses `DesignerApiHarness` against the owned endpoint layer
