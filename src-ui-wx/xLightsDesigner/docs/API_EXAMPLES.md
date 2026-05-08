# xLightsDesigner API Examples

These examples exercise the owned in-process endpoint layer.
They do not require xLights-owned automation routing.

## Sequence

`GET /xlightsdesigner/api/sequence/open`

```json
{}
```

## Sequence Settings

`GET /xlightsdesigner/api/sequence/settings`

```json
{}
```

`POST /xlightsdesigner/api/sequence/settings`

```json
{
  "supportsModelBlending": true
}
```

## Render Current Sequence

`POST /xlightsdesigner/api/sequence/render-current`

```json
{}
```

## Rendered Sequence Samples

`POST /xlightsdesigner/api/sequence/render-samples`

Call `sequence/render-current` first after sequence edits so samples reflect the latest applied revision.

```json
{
  "startMs": 44000,
  "endMs": 62000,
  "maxFrames": 5,
  "channelRanges": [
    { "startChannel": 1, "channelCount": 150 },
    { "startChannel": 1000, "channelCount": 150 }
  ]
}
```

## Layout Scene

`GET /xlightsdesigner/api/layout/scene`

```json
{}
```

## Create Custom Model

`POST /xlightsdesigner/api/layout/models/custom`

```json
{
  "name": "CustomProbe",
  "startChannel": "30001",
  "layoutGroup": "Default",
  "width": 1,
  "height": 1,
  "depth": 1,
  "stringCount": 1,
  "positionX": 0,
  "positionY": 0,
  "dryRun": true,
  "nodes": [
    { "x": 0, "y": 0, "z": 0, "node": 1, "string": 1 }
  ]
}
```

Set `dryRun` to `false` or omit it to create the model. With `dryRun: true`, the route validates the request and returns model metadata without mutating the layout.

## Timing Tracks

`GET /xlightsdesigner/api/timing/tracks`

```json
{}
```

## Timing Marks

`GET /xlightsdesigner/api/timing/marks?track=XD:%20Song%20Structure&startMs=0&endMs=90000`

```json
{}
```

## Ensure Timing Track

`POST /xlightsdesigner/api/timing/ensure-track`

```json
{
  "track": "XD: Song Structure",
  "subType": "section"
}
```

## Add Timing Marks

`POST /xlightsdesigner/api/timing/add-marks`

```json
{
  "track": "XD: Song Structure",
  "subType": "section",
  "replaceExisting": true,
  "marks": [
    { "startMs": 0, "endMs": 8000, "label": "Intro" },
    { "startMs": 44000, "endMs": 62000, "label": "Chorus 1" }
  ]
}
```

## Effect Window Read

`GET /xlightsdesigner/api/effects/window?element=Snowman&startMs=44000&endMs=62000`

```json
{}
```

## Clear Effect Window

`POST /xlightsdesigner/api/effects/clear-window`

```json
{
  "element": "Snowman",
  "layer": 0,
  "startMs": 44000,
  "endMs": 62000
}
```

## Add Effect

`POST /xlightsdesigner/api/effects/add-effect`

```json
{
  "element": "Snowman",
  "layer": 0,
  "effectName": "Color Wash",
  "startMs": 44000,
  "endMs": 62000,
  "settings": "",
  "palette": ""
}
```

## Update Effect

`POST /xlightsdesigner/api/effects/update`

Select by `effectId`, or by `element` + `layer` + `startMs` + `endMs` with optional `effectName`.

```json
{
  "element": "Snowman",
  "layer": 0,
  "startMs": 44000,
  "endMs": 62000,
  "effectName": "Color Wash",
  "newLayer": 1,
  "newStartMs": 44500,
  "newEndMs": 62000
}
```

## Delete Effects

`POST /xlightsdesigner/api/effects/delete`

```json
{
  "element": "Snowman",
  "layer": 1,
  "startMs": 44500,
  "endMs": 62000,
  "effectName": "Color Wash"
}
```

## Clone Effects

`POST /xlightsdesigner/api/effects/clone`

```json
{
  "sourceElement": "Star",
  "sourceLayer": 0,
  "sourceStartMs": 1000,
  "sourceEndMs": 5000,
  "targetElement": "MegaTree",
  "targetLayer": 1,
  "targetStartMs": 8000,
  "mode": "copy",
  "dryRun": false
}
```

Use `targetModels` with an array of element names for multi-target clones. Set `mode` to `move` to delete the matched source effects after clone creation succeeds.

The clone route allocates missing destination layers before creation. It also preflights the full destination set before mutating the sequence; if any requested target layer/time window overlaps existing effects, the job fails with `TARGET_WINDOW_OCCUPIED` and returns conflict details. Callers should choose an open layer or explicitly delete/update existing effects before retrying.

## Layer Stack Edits

`POST /xlightsdesigner/api/effects/reorder-layer`

```json
{
  "element": "Snowman",
  "fromLayer": 1,
  "toLayer": 0
}
```

`POST /xlightsdesigner/api/effects/delete-layer`

```json
{
  "element": "Snowman",
  "layer": 1,
  "force": true
}
```

`POST /xlightsdesigner/api/effects/compact-layers`

```json
{
  "element": "Snowman"
}
```

## Display Element Order

`GET /xlightsdesigner/api/elements/display-order`

```json
{}
```

`POST /xlightsdesigner/api/elements/display-order`

```json
{
  "orderedIds": ["Lyrics", "XD: Song Structure", "All Models", "Snowman"]
}
```

## Apply Window Plan

`POST /xlightsdesigner/api/sequencing/apply-window-plan`

```json
{
  "track": "XD: Song Structure",
  "subType": "section",
  "marks": [
    { "startMs": 44000, "endMs": 62000, "label": "Chorus 1" }
  ],
  "replaceExistingMarks": false,
  "element": "Snowman",
  "layer": 0,
  "effectName": "Color Wash",
  "effectStartMs": 44000,
  "effectEndMs": 62000,
  "settings": "",
  "palette": ""
}
```
