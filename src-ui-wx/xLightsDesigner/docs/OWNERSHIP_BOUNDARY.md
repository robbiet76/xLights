# xLightsDesigner Ownership Boundary

Goal:
- keep forked xLights source as close to baseline as possible
- place all new xLightsDesigner-owned code under `xLights/xLightsDesigner/`
- add only minimal host hooks into existing xLights files
- keep hooks inert by default so normal xLights behavior is unchanged

Current allowed non-baseline files outside `xLights/xLightsDesigner/`:
- `xLights/xLightsApp.cpp`
- `xLights/xLightsApp.h`
- `xLights/TabSequence.cpp`

Reason:
- `xLightsApp.*` provides the minimal startup and shutdown lifecycle hooks used to register the xLightsDesigner integration module
- `TabSequence.cpp` applies owned-enabled policy to rgbeffects and effect-preset autosave recovery prompts so known startup recovery prompts do not block automated listener startup

Modal boundary:
- normal owned API automation should prevent modals by using deterministic route preconditions
- unexpected xLights modals should be reported as blocking diagnostics rather than silently dismissed
- macOS Accessibility-based dismissal is a launch-time fallback before the owned API is available, not the core modal strategy

Baseline-preserved surfaces:
- `xLights/automation/...` remains xLights-owned and baseline
- core xLights files outside the allowed hook list should remain baseline unless a future hook is explicitly justified

Owned API rule:
- the future `xLights/xLightsDesigner/api/` surface must be treated as a clean owned product surface
- do not inherit legacy `V2` naming or migration-era file organization there
- if compatibility with xLights-owned automation is ever needed, keep that compatibility in a narrow seam rather than shaping the owned API architecture around it
