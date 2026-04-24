# xLightsDesigner API Audit And Refactor Plan

Status: mostly executed for the 2026.06 migration branch. Keep this file as the ongoing audit checklist for API-touching work, not as a stale pre-migration plan.

Goal:
- keep the active `xLightsDesigner/api/` surface readable, capability-based, and internally consistent
- remove stale or historical docs while changing API behavior
- prevent legacy naming baggage inherited from xLights automation history from returning

Design rules:
- do not use `V2` in file names, symbols, or documentation
- do not mirror legacy xLights automation file names unless a compatibility seam explicitly requires it
- group files by responsibility, not by historical transport format
- keep request parsing, command handling, validation, and response shaping separated
- prefer a small number of clear modules over many thin historical fragments

Ongoing audit checklist:
1. inventory every owned API file touched by a change
2. classify each changed file by responsibility:
   - transport
   - request parsing
   - validation
   - sequencing
   - layout
   - media
   - timing
   - jobs
   - system
3. identify code copied from legacy xLights automation and decide whether to:
   - keep
   - rewrite
   - delete
4. remove historical naming such as `V2`, `Legacy*`, and other migration-era labels unless they are required for a compatibility shim
5. keep `API_ARCHITECTURE.md` aligned with the current package structure
6. keep `API_CURRENT_STATE.md`, `API_EXAMPLES.md`, and smoke coverage aligned with active route behavior
7. standardize response and error contracts for owned APIs
8. remove references to inactive transaction/rollback contracts unless a new owned contract revives them
9. implement new owned API modules behind `DesignerIntegration`

Target outcome:
- `xLightsDesigner/api/` becomes the only owned API surface
- xLights-owned automation remains baseline and separate
- owned API code reads like a first-class product surface, not a migration layer
