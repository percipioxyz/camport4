#include "../common/common.hpp"
#include "genicam_utils.hpp"
#include "TYFeatureList.h"
#include "TYCoordinateMapper.h"
#include <vector>

#define MAP_DEPTH_TO_COLOR  1

struct CallbackData {
    int             index;
    TY_DEV_HANDLE   hDevice;
    float           scale_unit;
    bool            isDepthDistortion;

    TY_CAMERA_CALIB_INFO depth_calib;
    TY_CAMERA_CALIB_INFO color_calib;

    std::vector<uint16_t> depthBuffer;
    std::vector<uint8_t>  colorBuffer;
};

static void doRegister(const TY_CAMERA_CALIB_INFO& depth_calib,
                       const TY_CAMERA_CALIB_INFO& color_calib,
                       const uint16_t* depth, int32_t depthW, int32_t depthH,
                       float scale_unit,
                       const uint8_t* color, int32_t colorW, int32_t colorH,
                       std::vector<uint8_t>& out, bool map_depth_to_color)
{
    if (map_depth_to_color) {
        out.resize(colorW * colorH * sizeof(uint16_t));
        ASSERT_OK(TYMapDepthImageToColorCoordinate(
            &depth_calib, depthW, depthH, depth,
            &color_calib, colorW, colorH,
            reinterpret_cast<uint16_t*>(out.data()),
            scale_unit));
    } else {
        out.resize(depthW * depthH * 3);
        ASSERT_OK(TYMapRGBImageToDepthCoordinate(
            &depth_calib, depthW, depthH, depth,
            &color_calib, colorW, colorH, color,
            out.data(),
            scale_unit));
    }
}

void handleFrame(TY_FRAME_DATA* frame, void* userdata)
{
    CallbackData* pData = (CallbackData*)userdata;
    LOGD("=== Get frame %d", ++pData->index);

    TY_IMAGE_DATA* depthImage = nullptr;
    TY_IMAGE_DATA* colorImage = nullptr;
    for (int i = 0; i < frame->validCount; i++) {
        if (frame->image[i].status != TY_STATUS_OK) continue;
        const TYImageInfo img_info = ty_image_info(frame->image[i]);
        uint32_t decodeSize = 0;
        if (TYGetDecodeBufferSize(&img_info, &decodeSize, TY_OUTPUT_FORMAT_BGR) == TY_DECODE_SUCCESS)
            colorImage = &frame->image[i];
        else
            depthImage = &frame->image[i];
    }

    if (depthImage == nullptr) return;

    pData->depthBuffer.resize(depthImage->width * depthImage->height);
    if (pData->isDepthDistortion) {
        TY_IMAGE_DATA src, dst;
        src.width  = depthImage->width;
        src.height = depthImage->height;
        src.size   = depthImage->size;
        src.pixelFormat = TYPixelFormatCoord3D_C16;
        src.buffer = depthImage->buffer;

        std::vector<uint16_t> tmp(depthImage->width * depthImage->height);
        dst.width  = depthImage->width;
        dst.height = depthImage->height;
        dst.size   = src.size;
        dst.pixelFormat = TYPixelFormatCoord3D_C16;
        dst.buffer = tmp.data();

        ASSERT_OK(TYUndistortImage(&pData->depth_calib, &src, NULL, &dst));
        memcpy(pData->depthBuffer.data(), tmp.data(), depthImage->size);
    } else {
        memcpy(pData->depthBuffer.data(), depthImage->buffer, depthImage->size);
    }

    if (colorImage != nullptr) {
        const TYImageInfo color_info = ty_image_info(*colorImage);
        uint32_t colorDestSize = 0;
        TYDecodeResult colorDecode;
        TYDecodeError colorDecodeErr = TYGetDecodeBufferSize(
            &color_info, &colorDestSize, TY_OUTPUT_FORMAT_BGR);

        if (colorDecodeErr == TY_DECODE_SUCCESS) {
            pData->colorBuffer.resize(colorDestSize);
            ASSERT_DEC_OK(TYDecodeImage(&color_info, TY_OUTPUT_FORMAT_BGR,
                                        pData->colorBuffer.data(),
                                        colorDestSize, &colorDecode));

            TY_IMAGE_DATA src, dst;
            src.width  = colorImage->width;
            src.height = colorImage->height;
            src.size   = colorImage->size;
            src.pixelFormat = TYPixelFormatBGR8;
            src.buffer = pData->colorBuffer.data();

            std::vector<uint8_t> undistColor(colorDestSize);
            dst.width  = colorImage->width;
            dst.height = colorImage->height;
            dst.size   = colorImage->size;
            dst.pixelFormat = TYPixelFormatBGR8;
            dst.buffer = undistColor.data();

            ASSERT_OK(TYUndistortImage(&pData->color_calib, &src, NULL, &dst));
            pData->colorBuffer = std::move(undistColor);

            std::vector<uint8_t> out;
            doRegister(pData->depth_calib, pData->color_calib,
                       pData->depthBuffer.data(),
                       depthImage->width, depthImage->height, pData->scale_unit,
                       pData->colorBuffer.data(),
                       colorImage->width, colorImage->height,
                       out, MAP_DEPTH_TO_COLOR);

            if (MAP_DEPTH_TO_COLOR) {
                TYDisplayImage("depth2color",
                               colorImage->width, colorImage->height,
                               TYPixelFormatCoord3D_C16,
                               out.data(), pData->scale_unit);
                TYDisplayImage("undistort color",
                               colorImage->width, colorImage->height,
                               TYPixelFormatBGR8,
                               pData->colorBuffer.data());
            } else {
                TYDisplayImage("depth",
                               depthImage->width, depthImage->height,
                               TYPixelFormatCoord3D_C16,
                               pData->depthBuffer.data(), pData->scale_unit);
                TYDisplayImage("mapped RGB",
                               depthImage->width, depthImage->height,
                               TYPixelFormatBGR8,
                               out.data());
            }
        }
    }

    LOGD("=== Re-enqueue buffer(%p, %d)", frame->userBuffer, frame->bufferSize);
    ASSERT_OK(TYEnqueueBuffer(pData->hDevice, frame->userBuffer, frame->bufferSize));
}

void eventCallback(TY_EVENT_INFO* event_info, void* /*userdata*/)
{
    if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE)
        LOGD("=== Event Callback: Device Offline!");
    else if (event_info->eventId == TY_EVENT_LICENSE_ERROR)
        LOGD("=== Event Callback: License Error!");
}

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface  = NULL;
    TY_DEV_HANDLE       hDevice = NULL;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "-id") == 0) ID = argv[++i];
        else if (strcmp(argv[i], "-ip") == 0) IP = argv[++i];
        else if (strcmp(argv[i], "-h")  == 0) {
            LOGI("Usage: sample_view_registration [-h] [-id <ID>] [-ip <IP>]");
            return 0;
        }
    }

    LOGD("=== Init lib");
    ASSERT_OK(TYInitLib());
    TY_VERSION_INFO ver;
    ASSERT_OK(TYLibVersion(&ver));
    LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    std::vector<TY_DEVICE_BASE_INFO> selected;
    ASSERT_OK(selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected));
    ASSERT(selected.size() > 0);

    ASSERT_OK(TYOpenInterface(selected[0].iface.id, &hIface));
    ASSERT_OK(TYOpenDevice(hIface, selected[0].id, &hDevice));

    {
        uint32_t cnt = 0;
        TYEnumGetEntryCount(hDevice, "SourceSelector", &cnt);
        std::vector<TYEnumEntry> entries(cnt);
        TYEnumGetEntryInfo(hDevice, "SourceSelector", entries.data(), cnt, &cnt);
        LOGD("=== Available sources (%u):", cnt);
        for (uint32_t i = 0; i < cnt; i++) {
            LOGD("     - %s", entries[i].name);
            TYEnumSetString(hDevice, "SourceSelector", entries[i].name);
            TYBooleanSetValue(hDevice, "ComponentEnable", false);
        }
    }

    LOGD("=== Enable depth source");
    ASSERT_OK(TYEnumSetString(hDevice, "SourceSelector",
                              TYGetSourceSelectorName(SRC_SEL_DEPTH)));
    ASSERT_OK(TYBooleanSetValue(hDevice, "ComponentEnable", true));

    CallbackData cb_data;
    cb_data.index      = 0;
    cb_data.hDevice    = hDevice;
    cb_data.scale_unit = 1.0f;

    bool hasColorCam = false;
    {
        uint32_t cnt = 0;
        TYEnumGetEntryCount(hDevice, "SourceSelector", &cnt);
        std::vector<TYEnumEntry> entries(cnt);
        TYEnumGetEntryInfo(hDevice, "SourceSelector", entries.data(), cnt, &cnt);
        for (uint32_t i = 0; i < cnt && !hasColorCam; i++) {
            const char* name = entries[i].name;
            if (strcmp(name, TYGetSourceSelectorName(SRC_SEL_DEPTH)) == 0) continue;
            if (strcmp(name, TYGetSourceSelectorName(SRC_SEL_LEFT))  == 0) continue;
            if (strcmp(name, TYGetSourceSelectorName(SRC_SEL_RIGHT)) == 0) continue;
            LOGD("=== Trying colour source: %s", name);
            if (readCalibData(hDevice, name, cb_data.color_calib) == TY_STATUS_OK) {
                ASSERT_OK(TYBooleanSetValue(hDevice, "ComponentEnable", true));
                hasColorCam = true;
                LOGD("=== Colour source enabled: %s", name);
            }
        }
    }

    if (!hasColorCam) {
        LOGE("=== No colour camera found, cannot do registration");
        TYCloseDevice(hDevice);
        TYCloseInterface(hIface);
        TYDeinitLib();
        return -1;
    }

    LOGD("=== Read depth calib");
    ASSERT_OK(readCalibData(hDevice, TYGetSourceSelectorName(SRC_SEL_DEPTH),
                            cb_data.depth_calib));

    {
        double s = 1.0;
        ASSERT_OK(TYEnumSetString(hDevice, "SourceSelector",
                                  TYGetSourceSelectorName(SRC_SEL_DEPTH)));
        ASSERT_OK(TYFloatGetValue(hDevice, TY_DEPTH_SCALE, &s));
        cb_data.scale_unit = (float)s;
        LOGD("     - depth scale unit: %f", cb_data.scale_unit);
    }

    cb_data.isDepthDistortion = false;
    {
        ASSERT_OK(TYEnumSetString(hDevice, "SourceSelector",
                                  TYGetSourceSelectorName(SRC_SEL_DEPTH)));
        TY_ACCESS_MODE access;
        if (TYParamGetAccess(hDevice, "Distortion", &access) == TY_STATUS_OK &&
            (access & TY_ACCESS_READABLE)) {
            cb_data.isDepthDistortion = true;
        }
    }

    LOGD("=== Prepare image buffer");
    uint32_t frameSize = 0;
    ASSERT_OK(TYGetFrameBufferSize(hDevice, &frameSize));
    LOGD("     - frameSize = %u", frameSize);
    std::vector<char> frameBuffer[2];
    frameBuffer[0].resize(frameSize);
    frameBuffer[1].resize(frameSize);
    ASSERT_OK(TYEnqueueBuffer(hDevice, frameBuffer[0].data(), frameSize));
    ASSERT_OK(TYEnqueueBuffer(hDevice, frameBuffer[1].data(), frameSize));

    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

    LOGD("=== Start capture");
    ASSERT_OK(TYStartCapture(hDevice));

    LOGD("=== Waiting for frames (press 'q' to quit)");
    bool exit_main = false;
    while (!exit_main) {
        TY_FRAME_DATA frame;
        int err = TYFetchFrame(hDevice, &frame, -1);
        if (err != TY_STATUS_OK) {
            LOGE("Fetch frame error %d: %s", err, TYErrorString(err));
            break;
        }

        handleFrame(&frame, &cb_data);

        int key = TYWaitKeyEvents();
        switch (key & 0xff) {
            case 0xff: break;
            case 'q':  exit_main = true; break;
            default:   LOGD("Pressed key %d", key); break;
        }
    }

    ASSERT_OK(TYStopCapture(hDevice));
    ASSERT_OK(TYCloseDevice(hDevice));
    ASSERT_OK(TYCloseInterface(hIface));
    ASSERT_OK(TYDeinitLib());

    LOGD("=== Main done!");
    return 0;
}
