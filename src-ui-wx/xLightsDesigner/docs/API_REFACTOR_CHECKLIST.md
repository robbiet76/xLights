# xLightsDesigner API Refactor Checklist

## Audit
- [ ] Inventory all candidate API functionality to move into owned code
- [ ] Classify each item by capability
- [ ] Identify copied legacy logic versus logic worth rewriting cleanly
- [ ] Mark all historical `V2` and `Legacy*` names for removal or containment behind adapters

## Architecture
- [ ] Define initial `api/` folder layout
- [ ] Define shared request and response envelopes
- [ ] Define shared validation result model
- [ ] Define error catalog
- [x] Define top-level router contract

## Implementation
- [ ] Build first owned request parser
- [ ] Build first owned validation primitive
- [ ] Build first handler/service pair
- [x] Prove one end-to-end capability behind `DesignerIntegration`

## Cleanup
- [ ] Keep compatibility seams out of the owned architecture where possible
- [ ] Keep xLights-owned automation baseline unless a hook is explicitly required
- [ ] Document final naming rules and module boundaries

## Harness
- [x] Add owned endpoint mapping layer
- [x] Add in-process owned API harness
- [x] Document canonical owned endpoint examples
