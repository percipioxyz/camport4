# Camport API Diff Reference (V3 vs V4)

## Companion Docs

- Chinese user version: `API_DIFF_V3_V4.zh-CN.md`
- Migration cookbook with code snippets: `API_MIGRATION_V3_TO_V4.md`

## Scope

This document compares exported API declarations between:

- `camport3/include`
- `camport4/include`

Comparison includes symbols declared with these export macros:

- `TY_CAPI`
- `TYISP_CAPI`
- `TY_DECODE_API`

Notes:

- This is an API surface comparison based on header declarations.

## Summary

- V3 exported API count: `88`
- V4 exported API count: `123`
- V3-only APIs: `16`
- V4-only APIs: `51`

Main change directions:

- Logging moved from append/remove endpoints to configure + enable/disable model.
- V3 software ISP (`TYISP*`For cameras without a hardware ISP only) APIs are removed from V4 headers.
- V4 adds GenICam-style parameter APIs (`TYParameter.h`).
- V4 adds decode pipeline APIs (`TYGetDecodeTargetPixFmt`, `TYGetDecodeBufferSize`, `TYDecodeImage`).
- Several coordinate mapping helpers become exported C APIs in V4.

## V3-only APIs (16) with Function and V4 Alternatives

Legend for "V4 alternative":

- `Yes`: similar capability exists.
- `Partial`: can approximate capability, but not 1:1 behavior.
- `No`: no direct comparable API in V4 headers.

| # | V3 API | Header | What it does | V4 alternative | Notes |
|---|---|---|---|---|---|
| 1 | `TYAppendLogToFile` | `camport3/include/TYApi.h:235` | Add file logger and set level in one call. | `Yes` (`TYCfgLogFile` + `TYSetLogLevel` + `TYEnableLog`) | V4 splits config and enable steps. |
| 2 | `TYRemoveLogFile` | `camport3/include/TYApi.h:245` | Remove file logging target by path. | `Partial` (`TYDisableLog(TY_LOG_TYPE_FILE)`) | V4 disables by type, not by specific path. |
| 3 | `TYAppendLogToServer` | `camport3/include/TYApi.h:263` | Add TCP/UDP log server and set level in one call. | `Yes` (`TYCfgLogServer` + `TYSetLogLevel` + `TYEnableLog`) | V4 uses typed server enum (`TY_SERVER_TYPE`). |
| 4 | `TYRemoveLogServer` | `camport3/include/TYApi.h:280` | Remove log server target by protocol/ip/port. | `Partial` (`TYDisableLog(TY_LOG_TYPE_SERVER)`) | V4 does not expose per-endpoint remove API in headers. |
| 5 | `TYDepthImageFillEmptyRegion` | `camport3/include/TYCoordinateMapper.h:87` | Fill invalid/empty regions in depth map. | `Yes` (`TYDepthImageInpainter`) | V4 replacement is parameterized and typically stronger for hole-filling control. |
| 6 | `TYISPCreate` | `camport3/include/TyIsp.h:71` | Create software ISP handle. | `No` | V4 does not expose TYISP handle model. |
| 7 | `TYISPRelease` | `camport3/include/TyIsp.h:72` | Release software ISP handle. | `No` | No TYISP lifecycle APIs in V4 headers. |
| 8 | `TYISPLoadConfig` | `camport3/include/TyIsp.h:73` | Load ISP config blob. | `Partial` (`TYParameter` typed setters) | V4 can set device params but not load ISP blob via TYISP API. |
| 9 | `TYISPUpdateDevice` | `camport3/include/TyIsp.h:75` | Sync/update ISP-device status. | `No` | No direct TYISP status update API in V4 headers. |
| 10 | `TYISPSetFeature` | `camport3/include/TyIsp.h:77` | Set TYISP feature by ID and raw bytes. | `Partial` (`TYIntegerSetValue`, `TYFloatSetValue`, `TYBooleanSetValue`, `TYEnumSetString`, etc.) | V4 uses feature names and typed APIs (GenICam-style), not TYISP feature IDs. |
| 11 | `TYISPGetFeature` | `camport3/include/TyIsp.h:78` | Get TYISP feature by ID into buffer. | `Partial` (`TYIntegerGetValue`, `TYFloatGetValue`, `TYBooleanGetValue`, `TYEnumGetString`, etc.) | Capability overlaps for many controls but model differs. |
| 12 | `TYISPGetFeatureSize` | `camport3/include/TyIsp.h:79` | Query data size for TYISP feature. | `Partial` (`TYByteArrayGetSize`, `TYStringGetLength`) | Equivalent only for specific value types. |
| 13 | `TYISPHasFeature` | `camport3/include/TyIsp.h:81` | Check whether TYISP feature exists. | `Yes` (`TYParamExist`) | V4 checks by feature name string. |
| 14 | `TYISPGetFeatureInfoList` | `camport3/include/TyIsp.h:82` | Enumerate TYISP feature metadata list. | `Partial` (`TYParamGetType`, `TYParamGetAccess`, `TYParamGetToolTip`, `TYParamGetDescriptor`, `TYParamGetDisplayName`, `TYParamGetVisibility`) | V4 has per-feature metadata queries; no direct full-list API in headers. |
| 15 | `TYISPGetFeatureInfoListSize` | `camport3/include/TyIsp.h:83` | Get required buffer size for TYISP feature list. | `No` | No equivalent list-size API exposed for parameter catalog in V4 headers. |
| 16 | `TYISPProcessImage` | `camport3/include/TyIsp.h:85` | Convert/process Bayer raw to output image via ISP pipeline. | `Partial` (`TYGetDecodeTargetPixFmt`, `TYGetDecodeBufferSize`, `TYDecodeImage`) | V4 decode path covers many conversion cases, but TYISP fine-grained tuning model is not preserved 1:1. |

## V4-only APIs (51) by Capability

### 1) Logging rework

- `TYCfgLogFile`
- `TYCfgLogServer`
- `TYEnableLog`
- `TYDisableLog`

### 2) GenICam-style parameter APIs (`TYParameter.h`)

- Introspection/metadata:
  - `TYParamExist`
  - `TYParamGetToolTip`
  - `TYParamGetDescriptor`
  - `TYParamGetDisplayName`
  - `TYParamGetType`
  - `TYParamGetAccess`
  - `TYParamGetVisibility`
- Command:
  - `TYCommandExec`
- Integer:
  - `TYIntegerSetValue`
  - `TYIntegerGetValue`
  - `TYIntegerGetMin`
  - `TYIntegerGetMax`
  - `TYIntegerGetStep`
  - `TYIntegerGetUnit`
- Float:
  - `TYFloatSetValue`
  - `TYFloatGetValue`
  - `TYFloatGetMin`
  - `TYFloatGetMax`
  - `TYFloatGetStep`
  - `TYFloatGetUnit`
- Boolean:
  - `TYBooleanSetValue`
  - `TYBooleanGetValue`
- Enum:
  - `TYEnumSetValue`
  - `TYEnumSetString`
  - `TYEnumGetValue`
  - `TYEnumGetString`
  - `TYEnumGetEntryCount`
  - `TYEnumGetEntryInfo`
- String:
  - `TYStringSetValue`
  - `TYStringGetLength`
  - `TYStringGetValue`
- ByteArray:
  - `TYByteArrayGetSize`
  - `TYByteArraySetValue`
  - `TYByteArrayGetValue`

### 3) Decode/image algorithm APIs (`TYImageProc.h`)

- `TYGetDecodeTargetPixFmt`
- `TYGetDecodeBufferSize`
- `TYDecodeImage`
- `TYGetImageAlgorithmVersion`
- `TYDepthImageInpainter`
- `TYUndistortImage2`

### 4) Exported coordinate mapping helpers (`TYCoordinateMapper.h`)

- `TYMapDepthToColorCoordinate`
- `TYMapDepthImageToColorCoordinate`
- `TYMapRGBPixelsToDepthCoordinate`
- `TYMapRGBImageToDepthCoordinate`
- `TYMapRGB48ImageToDepthCoordinate`
- `TYMapMono16ImageToDepthCoordinate`
- `TYMapMono8ImageToDepthCoordinate`

## Compatibility Notes

1. API inclusion is almostly backward compatibility.
   - V4 does not include V3 TYISP API family(TYISP is for one old cameras model without a hardware ISP only).

2. Functional equivalence is mixed.
   - Logging and hole filling have workable migration routes.
   - TYISP advanced pipeline control is not preserved as a direct API model.

3. Same-name API can still be source-incompatible.
   - `TYSetLogLevel` signature changed in V4.

## Practical Migration Hints

1. Logging migration (V3 -> V4)
   - Replace append/remove calls with:
     - configure (`TYCfgLogFile` / `TYCfgLogServer`)
     - set level (`TYSetLogLevel(type, lvl)`)
     - enable/disable (`TYEnableLog` / `TYDisableLog`)

2. ISP pipeline migration
   - Replace `TYISPProcessImage` flow with decode flow:
     - `TYGetDecodeTargetPixFmt`
     - `TYGetDecodeBufferSize`
     - `TYDecodeImage`
   - Move tunable controls to `TYParameter.h` where available.

3. Parameter access migration
   - For new GenICam/SFNC devices, use typed parameter APIs in `TYParameter.h`.
   - Keep fallback path to old `TYApi.h` parameter APIs when needed by legacy devices.

## Related Files

- `camport3/include/TYApi.h`
- `camport3/include/TYCoordinateMapper.h`
- `camport3/include/TyIsp.h`
- `camport4/include/TYApi.h`
- `camport4/include/TYCoordinateMapper.h`
- `camport4/include/TYImageProc.h`
- `camport4/include/TYParameter.h`
- `camport4/README.md`
