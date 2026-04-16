# xLightsDesigner API Audit And Refactor Plan

Goal:
- perform a full audit of the future `xLights/xLightsDesigner/api/` surface
- organize it as if it were being built from scratch today
- keep it readable, capability-based, and internally consistent
- remove legacy naming baggage inherited from xLights automation history

Design rules:
- do not use `V2` in file names, symbols, or documentation
- do not mirror legacy xLights automation file names unless a compatibility seam explicitly requires it
- group files by responsibility, not by historical transport format
- keep request parsing, command handling, validation, and response shaping separated
- prefer a small number of clear modules over many thin historical fragments

Audit checklist:
1. inventory every owned API file before refactoring
2. classify each file by responsibility:
   - transport
   - request parsing
   - validation
   - sequencing
   - layout
   - media
   - timing
   - transactions
   - jobs
   - system
3. identify code copied from legacy xLights automation and decide whether to:
   - keep
   - rewrite
   - delete
4. remove historical naming such as `V2`, `Legacy*`, and other migration-era labels unless they are required for a compatibility shim
5. define a clean target package structure for `xLightsDesigner/api/`
6. standardize file naming around capability-based names
7. standardize response and error contracts for owned APIs
8. document the final folder structure and naming rules
9. only after the audit, begin implementing new owned API modules behind `DesignerIntegration`

Target outcome:
- `xLightsDesigner/api/` becomes the only owned API surface
- xLights-owned automation remains baseline and separate
- owned API code reads like a first-class product surface, not a migration layer
