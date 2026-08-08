#include "Utils.hpp"

static void readDoubleOrFloatArray(TY_DEV_HANDLE hDevice, const char* feat,
                                   float* out, int count)
{
    uint32_t byteSize = 0;
    if (TYByteArrayGetSize(hDevice, feat, &byteSize) != TY_STATUS_OK) return;

    if (byteSize == (uint32_t)(sizeof(double) * count)) {
        std::vector<double> tmp(count, 0.0);
        TYByteArrayGetValue(hDevice, feat,
                            reinterpret_cast<uint8_t*>(tmp.data()),
                            (uint32_t)(sizeof(double) * count));
        for (int i = 0; i < count; i++) out[i] = (float)tmp[i];
    } else if (byteSize == (uint32_t)(sizeof(float) * count)) {
        TYByteArrayGetValue(hDevice, feat,
                            reinterpret_cast<uint8_t*>(out),
                            (uint32_t)(sizeof(float) * count));
    } else {
        LOGD("Unexpected byte size %u for feature %s (expected %d or %d bytes)",
             byteSize, feat,
             (int)(sizeof(float) * count), (int)(sizeof(double) * count));
    }
}

static TY_STATUS readCalibData(TY_DEV_HANDLE hDevice, const char* source,
                                TY_CAMERA_CALIB_INFO& calib)
{
    memset(&calib, 0, sizeof(calib));

    ASSERT_OK(TYEnumSetString(hDevice, "SourceSelector", source));

    TY_ACCESS_MODE access;
    ASSERT_OK(TYParamGetAccess(hDevice, "IntrinsicWidth", &access));
    if (!(access & TY_ACCESS_READABLE)) {
        LOGD("  [%s] IntrinsicWidth not readable", source);
        return TY_STATUS_ERROR;
    }

    int64_t w = 0, h = 0;
    ASSERT_OK(TYIntegerGetValue(hDevice, "IntrinsicWidth",  &w));
    ASSERT_OK(TYIntegerGetValue(hDevice, "IntrinsicHeight", &h));
    calib.intrinsicWidth  = (int32_t)w;
    calib.intrinsicHeight = (int32_t)h;

    readDoubleOrFloatArray(hDevice, "Intrinsic", calib.intrinsic.data, 9);
    
    if (calib.intrinsic.data[0] < 1.0f || calib.intrinsic.data[4] < 1.0f) {
        LOGD("  [%s] focal length near-zero, calibration invalid", source);
        return TY_STATUS_ERROR;
    }

    ASSERT_OK(TYParamGetAccess(hDevice, "Distortion", &access));
    if (access & TY_ACCESS_READABLE) {
        readDoubleOrFloatArray(hDevice, "Distortion", calib.distortion.data, 12);
    }

    calib.extrinsic.data[0]  = 1.0f;
    calib.extrinsic.data[5]  = 1.0f;
    calib.extrinsic.data[10] = 1.0f;
    calib.extrinsic.data[15] = 1.0f;
    ASSERT_OK(TYParamGetAccess(hDevice, "Extrinsic", &access));
    if (access & TY_ACCESS_READABLE)
        readDoubleOrFloatArray(hDevice, "Extrinsic", calib.extrinsic.data, 16);

    return TY_STATUS_OK;
}
