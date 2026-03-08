# Camport V3 -> V4 Migration Cookbook

## Purpose

This guide provides practical migration snippets for the main API model changes from V3 to V4.

## 1) Logging Migration

### V3 style

```cpp
// V3
TYSetLogLevel(TY_LOG_LEVEL_INFO);
TYAppendLogToFile("cam.log", TY_LOG_LEVEL_INFO);
TYAppendLogToServer("udp", "127.0.0.1", 9000, TY_LOG_LEVEL_WARNING);

// ... run ...

TYRemoveLogFile("cam.log");
TYRemoveLogServer("udp", "127.0.0.1", 9000);
```

### V4 style

```cpp
// V4: configure -> set level -> enable
TYCfgLogFile("cam.log", 10 * 1024 * 1024, 10);
TYSetLogLevel(TY_LOG_TYPE_FILE, TY_LOG_LEVEL_INFO);
TYEnableLog(TY_LOG_TYPE_FILE);

TYCfgLogServer(TY_SERVER_TYPE_UDP, "127.0.0.1", 9000);
TYSetLogLevel(TY_LOG_TYPE_SERVER, TY_LOG_LEVEL_WARNING);
TYEnableLog(TY_LOG_TYPE_SERVER);

// ... run ...

// V4: disable by type
TYDisableLog(TY_LOG_TYPE_FILE);
TYDisableLog(TY_LOG_TYPE_SERVER);
```

### Migration note

- V3 remove APIs work on specific targets (path/ip/port).
- V4 disable APIs work on logger type, not per endpoint.

## 2) Depth Hole Filling Migration

### V3 style

```cpp
// depth: uint16_t*, width, height
TYDepthImageFillEmptyRegion(depth, width, height);
```

### V4 style

```cpp
TY_IMAGE_DATA depthImage = TYInitImageData(
    width * height * sizeof(uint16_t),
    depth,
    width,
    height);
depthImage.pixelFormat = TY_PIXEL_FORMAT_DEPTH16;

DepthInpainterParameters p = DepthInpainterParameters_Initializer;
p.kernel_size = 5;
p.max_internal_hole = 50;

TY_STATUS st = TYDepthImageInpainter(&depthImage, &p);
if (st != TY_STATUS_OK) {
    // handle error
}
```

### Migration note

- V4 provides tunable hole-filling behavior via parameters.
- Tune `kernel_size` and `max_internal_hole` per scene quality and performance needs.

## 3) ISP Pipeline Migration (`TYISPProcessImage`)

### V3 style

```cpp
TY_ISP_HANDLE isp = nullptr;
TYISPCreate(&isp);

// optional tuning
// TYISPSetFeature(isp, ...);

TY_IMAGE_DATA out = TYInitImageData(outSize, outBuffer, w, h);
out.pixelFormat = TY_PIXEL_FORMAT_BGR;
TYISPProcessImage(isp, &inRaw, &out);

TYISPRelease(&isp);
```

### V4 style

```cpp
TYImageInfo input{};
input.width = inRaw.width;
input.height = inRaw.height;
input.format = static_cast<TYPixFmt>(inRaw.pixelFormat);
input.dataSize = static_cast<uint32_t>(inRaw.size);
input.data = inRaw.buffer;

uint32_t outSize = 0;
TYDecodeError e = TYGetDecodeBufferSize(&input, &outSize, TY_OUTPUT_FORMAT_AUTO);
if (e == TY_DECODE_SUCCESS) {
    std::vector<uint8_t> outBuf(outSize);
    TYDecodeResult result{};
    TYDecodeError de = TYDecodeImage(
        &input,
        TY_OUTPUT_FORMAT_AUTO,
        outBuf.data(),
        outSize,
        &result);
    if (de == TY_DECODE_SUCCESS) {
        // result.format / result.width / result.height / outBuf
    }
}
```

### Migration note

- For feature tuning, move to `TYParameter.h` typed APIs where available.

## 4) Feature Access -> TYParameter Typed Access

### V3 style (feature ID model)

```cpp
float gamma = 1.0f;
TYISPSetFeature(isp, TY_ISP_FEATURE_GAMMA, gamma);

int autoBright = 0;
TYISPGetFeature(isp, TY_ISP_FEATURE_AUTOBRIGHT, &autoBright);
```

### V4 style (feature name + type model)

```cpp
// Feature names depend on device XML / SFNC support.
// Check existence and access first.
bool exist = false;
TYParamExist(hDevice, "ExposureTime", &exist);
if (exist) {
    TY_ACCESS_MODE access;
    TYParamGetAccess(hDevice, "ExposureTime", &access);
    if (access & TY_ACCESS_WRITABLE) {
        TYFloatSetValue(hDevice, "ExposureTime", 3000.0);
    }

    double exposure = 0.0;
    TYFloatGetValue(hDevice, "ExposureTime", &exposure);
}

bool gainExist = false;
TYParamExist(hDevice, "Gain", &gainExist);
if (gainExist) {
    TY_ACCESS_MODE access;
    TYParamGetAccess(hDevice, "Gain", &access);
    if (access & TY_ACCESS_WRITABLE) {
        TYFloatSetValue(hDevice, "Gain", 1.0);
    }
}
```

### Migration note

- Use `TYParamExist` + `TYParamGetAccess` before read/write.
- Prefer enum string setters/getters (`TYEnumSetString`, `TYEnumGetString`) for readability.

## 5) Known Compatibility Risks

1. `TYSetLogLevel` signature changed in V4.
2. TYISP lifecycle APIs are removed from V4 headers.

## 6) Recommended Migration Workflow

1. Replace logging calls first (low risk).
2. Replace image conversion path (`TYISPProcessImage` -> decode APIs， for the one camera without a hardware ISP only).
3. Migrate tuning and parameter controls to `TYParameter.h` APIs.
4. Keep fallback logic for legacy devices that still require old parameter style.

## Related References

- API diff detail: `API_DIFF_V3_V4.md`
- Chinese summary: `API_DIFF_V3_V4.zh-CN.md`
