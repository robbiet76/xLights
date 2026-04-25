# xLightsDesigner API Examples

These examples exercise the owned in-process endpoint layer.
They do not require xLights-owned automation routing.

## Sequence

`GET /xlightsdesigner/api/sequence/open`

```json
{}
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
