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
- `src-ui-wx/sequencer/tabSequencer.cpp`
- `src-ui-wx/shared/utils/wxUtilities.cpp`
- `src-core/render/SongStructureManager.cpp`
- `src-core/render/SongStructureManager.h`

Reason:
- `xLightsApp.*` provides startup/shutdown lifecycle hooks and env-gated noninteractive startup behavior.
- `xLightsMain.*` provides env-gated prompt suppression callbacks and the narrow requested-size House Preview export hook.
- `TabSequence.cpp`, `TabSetup.cpp`, `SeqFileUtilities.cpp`, and `wxUtilities.cpp` prevent noninteractive prompt deadlocks only when xLightsDesigner explicitly launched xLights.
- `xLightsAutomations.cpp` keeps existing preview automation dialog-free when it is invoked.
- `tabSequencer.cpp` contains small null/stale-pointer guards observed during noninteractive launch testing.
- `SongStructureManager.*` exposes a read-only accessor for regions in a specific song-structure view so XLD can inspect all native region views without changing the active xLights view.

Modal boundary:
- normal owned API automation should prevent modals by using deterministic route preconditions
- unexpected xLights modals should be reported as blocking diagnostics rather than silently dismissed
- macOS Accessibility-based dismissal is a launch-time fallback before the owned API is available, not the core modal strategy

Baseline-preserved surfaces:
- `xLights/automation/...` remains xLights-owned and baseline
- core xLights files outside the allowed hook list should remain baseline unless a future hook is explicitly justified

Owned API rule:
- native xLights effect-block authoring is in scope only through XLD-owned effect metadata and the owned API under `src-ui-wx/xLightsDesigner`
- timing track creation/editing is out of scope for XLD; xLights owns timing tracks and XLD reads them
- generated FSEQ DataLayers remain available for direct-channel proof workflows, but native effect blocks are the editable sequencing path
- if xLights-owned automation ever needs to call into this API, keep that bridge narrow rather than shaping the owned API architecture around it
