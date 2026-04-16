# xLightsDesigner Ownership Boundary

Goal:
- keep forked xLights source as close to baseline as possible
- place all new xLightsDesigner-owned code under `xLights/xLightsDesigner/`
- add only minimal host hooks into existing xLights files
- keep hooks inert by default so normal xLights behavior is unchanged

Current allowed non-baseline files outside `xLights/xLightsDesigner/`:
- `xLights/xLightsApp.cpp`
- `xLights/xLightsApp.h`
- `xLights/SeqFileUtilities.cpp`
- `xLights/TabSequence.cpp`
- `xLights/xLightsXmlFile.cpp`

Reason:
- `xLightsApp.*` provides the minimal startup and shutdown lifecycle hooks used to register the xLightsDesigner integration module
- `SeqFileUtilities.cpp` and `TabSequence.cpp` suppress autosave recovery prompts in owned-enabled mode so listener startup is not blocked by modal dialogs
- `xLightsXmlFile.cpp` implements atomic sequence save behavior so write failures do not corrupt `.xsq` files

Baseline-preserved surfaces:
- `xLights/automation/...` remains xLights-owned and baseline
- core xLights files outside the allowed hook list should remain baseline unless a future hook is explicitly justified

Owned API rule:
- the future `xLights/xLightsDesigner/api/` surface must be treated as a clean owned product surface
- do not inherit legacy `V2` naming or migration-era file organization there
- if compatibility with xLights-owned automation is ever needed, keep that compatibility in a narrow seam rather than shaping the owned API architecture around it
