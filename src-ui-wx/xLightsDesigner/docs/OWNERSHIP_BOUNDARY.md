# xLightsDesigner Ownership Boundary

Goal:
- keep forked xLights source as close to baseline as possible
- place all new xLightsDesigner-owned code under `xLights/xLightsDesigner/`
- add only minimal host hooks into existing xLights files
- keep hooks inert by default so normal xLights behavior is unchanged

Current allowed non-baseline files outside `xLights/xLightsDesigner/`:
- `src-ui-wx/xLightsApp.cpp`
- `src-ui-wx/xLightsApp.h`
- `src-ui-wx/xLightsMain.cpp`
- `src-ui-wx/xLightsMain.h`
- `src-ui-wx/app-shell/TabSequence.cpp`
- `src-ui-wx/app-shell/TabSetup.cpp`
- `src-ui-wx/automation/xLightsAutomations.cpp`
- `src-ui-wx/import_export/SeqFileUtilities.cpp`
- `src-ui-wx/media/VideoExporter.cpp`
- `src-ui-wx/media/VideoExporter.h`
- `src-ui-wx/sequencer/tabSequencer.cpp`
- `src-ui-wx/shared/utils/wxUtilities.cpp`

Reason:
- `xLightsApp.*` provides startup/shutdown lifecycle hooks and env-gated noninteractive startup behavior.
- `xLightsMain.*` provides env-gated prompt suppression callbacks and the narrow requested-size House Preview export hook.
- `TabSequence.cpp`, `TabSetup.cpp`, `SeqFileUtilities.cpp`, and `wxUtilities.cpp` prevent noninteractive prompt deadlocks only when xLightsDesigner explicitly launched xLights.
- `VideoExporter.*` converts preview export file-open/header failures into ordinary API-visible errors instead of uncaught exceptions.
- `xLightsAutomations.cpp` keeps legacy preview automation dialog-free when it is invoked.
- `tabSequencer.cpp` contains small null/stale-pointer guards observed during noninteractive launch testing.

Modal boundary:
- normal owned API automation should prevent modals by using deterministic route preconditions
- unexpected xLights modals should be reported as blocking diagnostics rather than silently dismissed
- macOS Accessibility-based dismissal is a launch-time fallback before the owned API is available, not the core modal strategy

Baseline-preserved surfaces:
- `xLights/automation/...` remains xLights-owned and baseline
- core xLights files outside the allowed hook list should remain baseline unless a future hook is explicitly justified

Owned API rule:
- native xLights effect-block authoring is out of scope for the xLightsDesigner API
- generated FSEQ DataLayers are the sequencing integration path
- if compatibility with xLights-owned automation is ever needed, keep that compatibility in a narrow seam rather than shaping the owned API architecture around it
