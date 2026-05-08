# xLightsDesigner API Refactor Checklist

## Audit
- [x] Inventory candidate API functionality moved into owned code
- [x] Classify active owned routes by capability
- [x] Identify copied legacy logic versus logic worth rewriting cleanly for the current migration slice
- [x] Keep historical `V2` and `Legacy*` names out of owned API file names

## Architecture
- [x] Define initial `api/` folder layout
- [x] Define shared request and response envelopes
- [x] Define shared validation result model
- [x] Define error catalog
- [x] Define top-level router contract

## Implementation
- [x] Build first owned request parser
- [x] Build first owned validation primitive
- [x] Build owned handler/service pairs for active sequence, layout, timing, media, effects, elements, and sequencing routes
- [x] Prove one end-to-end capability behind `DesignerIntegration`

## Cleanup
- [x] Keep compatibility seams out of the owned architecture where possible
- [x] Keep xLights-owned automation baseline unless a hook is explicitly required
- [x] Document current naming rules and module boundaries
- [ ] Remove or rewrite stale docs during every API-touching change
- [ ] Keep `API_CURRENT_STATE.md`, `API_EXAMPLES.md`, and smoke coverage synchronized with active route behavior

## Harness
- [x] Add owned endpoint mapping layer
- [x] Add in-process owned API harness
- [x] Document canonical owned endpoint examples
- [x] Add smoke coverage for render-feedback route dispatch
