# xLightsDesigner API Examples

These examples exercise the owned in-process endpoint layer.
They do not require xLights-owned automation routing.

## Sequence

`GET /xlightsdesigner/api/sequence/open`

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
