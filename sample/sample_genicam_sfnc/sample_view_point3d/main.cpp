#include <limits>
#include <cassert>
#include <cmath>
#include <thread>
#include <iostream>
#include "../common/common.hpp"
#include "genicam_utils.hpp"
#include "TYFeatureList.h"
#include "TYCoordinateMapper.h"
#include "../../cloud_viewer/cloud_viewer.hpp"
#include <vector>

struct CallbackData {
    int             index;
    TY_DEV_HANDLE   hDevice;
    TY_CAMERA_CALIB_INFO depth_calib;
    TY_CAMERA_CALIB_INFO color_calib;

    float f_depth_scale;
    bool isDepthDistortion;
    bool saveOneFramePoint3d;
    bool exit_main;
    int  fileIndex;
    bool map_depth_to_color;

    std::vector<uint16_t> depthBuffer;
    std::vector<uint8_t> colorBuffer;
};

static CallbackData cb_data;

//////////////////////////////////////////////////
static void handleFrame(TY_FRAME_DATA* frame, void* userdata) {
    CallbackData* pData = (CallbackData*)userdata;
    LOGD("=== Get frame %d", ++pData->index);

    TY_IMAGE_DATA* depthImage = nullptr;
    TY_IMAGE_DATA* colorImage = nullptr;
    for (int i = 0; i < frame->validCount; i++) {
        if (frame->image[i].status != TY_STATUS_OK) continue;
        const TYImageInfo img_info = ty_image_info(frame->image[i]);
        uint32_t decodeSize = 0;
        if (TYGetDecodeBufferSize(&img_info, &decodeSize, TY_OUTPUT_FORMAT_BGR) == TY_DECODE_SUCCESS) {
            colorImage = &frame->image[i];
        } else {
            depthImage = &frame->image[i];
        }
    }

    if (depthImage != nullptr && pData->index == 1) {
        if (pData->depth_calib.intrinsicWidth  != (int32_t)depthImage->width ||
            pData->depth_calib.intrinsicHeight != (int32_t)depthImage->height)
        {
            float sx = (float)pData->depth_calib.intrinsicWidth  / (float)depthImage->width;
            float sy = (float)pData->depth_calib.intrinsicHeight / (float)depthImage->height;
            pData->depth_calib.intrinsic.data[0] /= sx;  // fx
            pData->depth_calib.intrinsic.data[2] /= sx;  // cx
            pData->depth_calib.intrinsic.data[4] /= sy;  // fy
            pData->depth_calib.intrinsic.data[5] /= sy;  // cy
            pData->depth_calib.intrinsicWidth  = (int32_t)depthImage->width;
            pData->depth_calib.intrinsicHeight = (int32_t)depthImage->height;
        }
    }

    if (depthImage != nullptr) {

        int32_t map_width  = depthImage->width;
        int32_t map_height = depthImage->height;

        pData->depthBuffer.resize(depthImage->width * depthImage->height);
        if (pData->isDepthDistortion) {
            TY_IMAGE_DATA src, dst;
            src.width  = depthImage->width;
            src.height = depthImage->height;
            src.size   = depthImage->size;
            src.pixelFormat = depthImage->pixelFormat;
            src.buffer = (void*)depthImage->buffer;

            std::vector<uint16_t> undistortDepthBuffer(depthImage->width * depthImage->height);
            dst.width  = depthImage->width;
            dst.height = depthImage->height;
            dst.size   = src.size;
            dst.buffer = (void*)&undistortDepthBuffer[0];
            dst.pixelFormat = depthImage->pixelFormat;

            ASSERT_OK(TYUndistortImage(&pData->depth_calib, &src, NULL, &dst));
            memcpy(&pData->depthBuffer[0], &undistortDepthBuffer[0], depthImage->size);
        } else {
            memcpy(&pData->depthBuffer[0], depthImage->buffer, depthImage->size);
        }

        uint32_t pointCount = depthImage->width * depthImage->height;
        TY_CAMERA_CALIB_INFO* calib_data_ptr = &pData->depth_calib;

        uint8_t* color_data = nullptr;
        if (colorImage != nullptr) {
            // Scale color_calib from reference size to actual color frame size (once, self-limiting).
            if (pData->color_calib.intrinsicWidth  > 0 &&
                (pData->color_calib.intrinsicWidth  != (int32_t)colorImage->width ||
                 pData->color_calib.intrinsicHeight != (int32_t)colorImage->height))
            {
                float sx = (float)pData->color_calib.intrinsicWidth  / (float)colorImage->width;
                float sy = (float)pData->color_calib.intrinsicHeight / (float)colorImage->height;
                pData->color_calib.intrinsic.data[0] /= sx;  // fx
                pData->color_calib.intrinsic.data[2] /= sx;  // cx
                pData->color_calib.intrinsic.data[4] /= sy;  // fy
                pData->color_calib.intrinsic.data[5] /= sy;  // cy
                pData->color_calib.intrinsicWidth  = (int32_t)colorImage->width;
                pData->color_calib.intrinsicHeight = (int32_t)colorImage->height;
                LOGD("  color_calib scaled to actual frame %dx%d (sx=%.3f sy=%.3f)",
                     colorImage->width, colorImage->height, sx, sy);
            }
            const TYImageInfo color_info = ty_image_info(*colorImage);
            uint32_t colorDestSize = 0;
            TYDecodeResult colorDecode;
            TYDecodeError colorDecodeErr = TYGetDecodeBufferSize(&color_info, &colorDestSize,
                                                                  TY_OUTPUT_FORMAT_RGB);
            if (colorDecodeErr == TY_DECODE_SUCCESS) {
                pData->colorBuffer.resize(colorDestSize);
                ASSERT_DEC_OK(TYDecodeImage(&color_info, TY_OUTPUT_FORMAT_RGB,
                                            (void*)&pData->colorBuffer[0],
                                            colorDestSize, &colorDecode));

                TY_IMAGE_DATA src, dst;
                src.width  = colorImage->width;
                src.height = colorImage->height;
                src.size   = colorImage->size;
                src.pixelFormat = TYPixelFormatRGB8;
                src.buffer = (void*)&pData->colorBuffer[0];

                std::vector<uint8_t> undistortColorBuffer(colorDestSize);
                dst.width  = colorImage->width;
                dst.height = colorImage->height;
                dst.size   = colorImage->size;
                dst.pixelFormat = TYPixelFormatRGB8;
                dst.buffer = (void*)&undistortColorBuffer[0];

                ASSERT_OK(TYUndistortImage(&pData->color_calib, &src, NULL, &dst));
                pData->colorBuffer = std::move(undistortColorBuffer);

                color_data = &pData->colorBuffer[0];

                if (pData->map_depth_to_color) {
                    pointCount = colorImage->width * colorImage->height;
                    std::vector<uint16_t> mappedDepth(pointCount);

                    ASSERT_OK(TYMapDepthImageToColorCoordinate(
                        &pData->depth_calib,
                        depthImage->width, depthImage->height,
                        &pData->depthBuffer[0],
                        &pData->color_calib,
                        colorImage->width, colorImage->height,
                        &mappedDepth[0],
                        pData->f_depth_scale
                    ));

                    pData->depthBuffer = std::move(mappedDepth);
                    calib_data_ptr = &pData->color_calib;
                    map_width  = colorImage->width;
                    map_height = colorImage->height;
                } else {
                    std::vector<uint8_t> mappedColor(pointCount * 3);

                    ASSERT_OK(TYMapRGBImageToDepthCoordinate(
                        &pData->depth_calib,
                        depthImage->width, depthImage->height,
                        &pData->depthBuffer[0],
                        &pData->color_calib,
                        colorImage->width, colorImage->height,
                        &pData->colorBuffer[0],
                        &mappedColor[0],
                        pData->f_depth_scale
                    ));
                    color_data = &mappedColor[0];
                    pData->colorBuffer = std::move(mappedColor);
                }
            }
        }

        std::vector<TY_VECT_3F> p3d(pointCount);
        ASSERT_OK(TYMapDepthImageToPoint3d(
            calib_data_ptr,
            map_width, map_height,
            &pData->depthBuffer[0],
            &p3d[0],
            pData->f_depth_scale
        ));

        if (pData->saveOneFramePoint3d) {
            char file[32];
            snprintf(file, sizeof(file), "points-%d.xyz", pData->fileIndex++);
            writePointCloud((float*)&p3d[0], (const uint8_t*)color_data,
                            p3d.size(), file, PC_FILE_FORMAT_XYZ);
            pData->saveOneFramePoint3d = false;
        }

        std::vector<TY_VECT_3F> validP3d;
        std::vector<uint8_t>    validColor;
        validP3d.reserve(pointCount);
        if (color_data != nullptr) validColor.reserve(pointCount * 3);
        for (size_t idx = 0; idx < p3d.size(); idx++) {
            TY_VECT_3F pt = p3d[idx];
            if (!std::isfinite(pt.x) || !std::isfinite(pt.y) || !std::isfinite(pt.z)) continue;
            pt.y = -pt.y;
            pt.z = -pt.z;
            validP3d.push_back(pt);
            if (color_data != nullptr) {
                validColor.push_back(color_data[idx * 3 + 0]);
                validColor.push_back(color_data[idx * 3 + 1]);
                validColor.push_back(color_data[idx * 3 + 2]);
            }
        }

        GLPointCloudViewer::Update(validP3d.size(), validP3d.data(),
                                   color_data != nullptr ? validColor.data() : nullptr);
    }
}

void eventCallback(TY_EVENT_INFO* event_info, void* userdata)
{
    if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE) {
        LOGD("=== Event Callback: Device Offline!");
    } else if (event_info->eventId == TY_EVENT_LICENSE_ERROR) {
        LOGD("=== Event Callback: License Error!");
    }
}

static int FetchOneFrame(CallbackData& cb)
{
    TY_FRAME_DATA frame;
    int err = TYFetchFrame(cb.hDevice, &frame, -1);
    if (err != TY_STATUS_OK) {
        LOGD("... Drop one frame");
        return -1;
    }
    handleFrame(&frame, &cb);
    LOGD("=== Re-enqueue buffer(%p, %d)", frame.userBuffer, frame.bufferSize);
    TYEnqueueBuffer(cb.hDevice, frame.userBuffer, frame.bufferSize);
    return 0;
}

void FetchFrameThreadFunc(CallbackData* d)
{
    CallbackData& cb = *d;
    while (!cb.exit_main) {
        if (FetchOneFrame(cb) != 0) {
            break;
        }
    }
}

bool key_pressed(int key)
{
    if (key == 's') {
        cb_data.saveOneFramePoint3d = true;
        return true;
    }
    return false;
}

static void configureSourceFormat(TY_DEV_HANDLE hDevice, const char* sourceName)
{
    // --- PixelFormat ---
    uint32_t fmtCnt = 0;
    if (TYEnumGetEntryCount(hDevice, "PixelFormat", &fmtCnt) == TY_STATUS_OK && fmtCnt > 0) {
        std::vector<TYEnumEntry> fmts(fmtCnt);
        TYEnumGetEntryInfo(hDevice, "PixelFormat", fmts.data(), fmtCnt, &fmtCnt);
        if (fmtCnt == 1) {
            std::cout << sourceName << " only supports one format: " << fmts[0].name << std::endl;
        } else {
            std::cout << sourceName << " supports the following formats:" << std::endl;
            for (uint32_t i = 0; i < fmtCnt; i++)
                std::cout << "\t" << i << "." << fmts[i].name << std::endl;
            std::cout << "Please select a format according to the above number!" << std::endl;
            int idx = -1;
            do {
                std::cin >> idx;
                cin_clear_rest_line();
                if (idx >= 0 && idx < (int)fmtCnt) break;
                std::cout << "Error, please select again!" << std::endl;
            } while (true);
            std::cout << "======format idx = " << idx << std::endl;
            std::cout << "Select " << fmts[idx].name << std::endl;
            ASSERT_OK(TYEnumSetValue(hDevice, "PixelFormat", fmts[idx].value));
        }
    }

    uint32_t binCnt = 0;
    if (TYEnumGetEntryCount(hDevice, "BinningHorizontal", &binCnt) == TY_STATUS_OK && binCnt > 0) {
        std::vector<TYEnumEntry> bins(binCnt);
        TYEnumGetEntryInfo(hDevice, "BinningHorizontal", bins.data(), binCnt, &binCnt);
        if (binCnt == 1) {
            std::cout << sourceName << " only supports one binning mode: " << bins[0].name << std::endl;
        } else {
            std::cout << sourceName << " supports the following binning mode:" << std::endl;
            for (uint32_t i = 0; i < binCnt; i++)
                std::cout << "\t" << i << "." << bins[i].name << std::endl;
            std::cout << "Please select a binning mode according to the above number!" << std::endl;
            int idx = -1;
            do {
                std::cin >> idx;
                cin_clear_rest_line();
                if (idx >= 0 && idx < (int)binCnt) break;
                std::cout << "Error, please select again!" << std::endl;
            } while (true);
            std::cout << "======binning idx = " << idx << std::endl;
            std::cout << "Select " << bins[idx].name << std::endl;
            ASSERT_OK(TYEnumSetValue(hDevice, "BinningHorizontal", bins[idx].value));
        }
    }

    ASSERT_OK(TYBooleanSetValue(hDevice, "ComponentEnable", true));
}

int main(int argc, char* argv[])
{
    GLPointCloudViewer::GlInit();
    std::string ID, IP;
    std::string depthSourceArg;
    TY_INTERFACE_HANDLE hIface  = NULL;
    TY_DEV_HANDLE       hDevice = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-id") == 0) {
            ID = argv[++i];
        } else if (strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        } else if (strcmp(argv[i], "-depth") == 0) {
            depthSourceArg = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: sample_view_point3d [-h] [-id <ID>] [-ip <IP>] [-depth <Depth>]");
            return 0;
        }
    }

    ASSERT_OK(TYInitLib());
    TY_VERSION_INFO ver;
    ASSERT_OK(TYLibVersion(&ver));
    LOGD("=== lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    std::vector<TY_DEVICE_BASE_INFO> selected;
    ASSERT_OK(selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected));
    ASSERT(selected.size() > 0);
    TY_DEVICE_BASE_INFO& selectedDev = selected[0];

    ASSERT_OK(TYOpenInterface(selectedDev.iface.id, &hIface));
    ASSERT_OK(TYOpenDevice(hIface, selectedDev.id, &hDevice));


    std::string depthSourceName;
    {
        const char* depthCandidates[] = {
            TYGetSourceSelectorName(SRC_SEL_DEPTH),
        };
        uint32_t srcCnt = 0;
        TYEnumGetEntryCount(hDevice, "SourceSelector", &srcCnt);
        std::vector<TYEnumEntry> srcEntrys(srcCnt);
        TYEnumGetEntryInfo(hDevice, "SourceSelector", srcEntrys.data(), srcCnt, &srcCnt);

        std::vector<std::string> candidates;
        for (auto& e : srcEntrys) {
            for (auto& c : depthCandidates) {
                if (strcmp(e.name, c) == 0) {
                    candidates.push_back(e.name);
                    break;
                }
            }
        }
        ASSERT(candidates.size() > 0);

        if (!depthSourceArg.empty()) {
            bool found = false;
            for (auto& c : candidates) {
                if (c == depthSourceArg) { found = true; break; }
            }
            if (!found) {
                std::cout << "Error: depth source '" << depthSourceArg << "' not available. Valid options:";
                for (auto& c : candidates) std::cout << " " << c;
                std::cout << std::endl;
                return -1;
            }
            depthSourceName = depthSourceArg;
            std::cout << "=== Depth source (from -depth arg): " << depthSourceName << std::endl;
        } else if (candidates.size() == 1) {
            depthSourceName = candidates[0];
            std::cout << "=== Depth source (auto): " << depthSourceName << std::endl;
        } else {
            std::cout << "Available depth sources:" << std::endl;
            for (size_t i = 0; i < candidates.size(); i++) {
                std::cout << "  " << i << ". " << candidates[i] << std::endl;
            }
            std::cout << "Please select a depth source according to the above number!" << std::endl;
            int idx = -1;
            do {
                std::cin >> idx;
                cin_clear_rest_line();
                if (idx >= 0 && idx < (int)candidates.size()) break;
                std::cout << "Error, please select again!" << std::endl;
            } while (true);
            depthSourceName = candidates[idx];
            std::cout << "=== Depth source selected: " << depthSourceName << std::endl;
        }
    }

    {
        uint32_t srcCnt = 0;
        TYEnumGetEntryCount(hDevice, "SourceSelector", &srcCnt);
        std::vector<TYEnumEntry> srcEntrys(srcCnt);
        TYEnumGetEntryInfo(hDevice, "SourceSelector", srcEntrys.data(), srcCnt, &srcCnt);
        for (uint32_t i = 0; i < srcCnt; i++) {
            TYEnumSetString(hDevice, "SourceSelector", srcEntrys[i].name);
            TYBooleanSetValue(hDevice, "ComponentEnable", false);
        }
    }

    LOGD("=== Configure components, open depth cam: %s", depthSourceName.c_str());
    ASSERT_OK(TYEnumSetString(hDevice, "SourceSelector", depthSourceName.c_str()));
    configureSourceFormat(hDevice, depthSourceName.c_str());

    double depth_scale_unit = 1.0;
    {
        double s = 1.0;
        if (TYFloatGetValue(hDevice, TY_DEPTH_SCALE, &s) == TY_STATUS_OK) {
            depth_scale_unit = s;
        }
    }
    cb_data.f_depth_scale = (float)depth_scale_unit;
    LOGD("     - depth scale unit: %f", depth_scale_unit);

    bool isDepthDistortion = false;
    {
        TY_ACCESS_MODE access;
        if (TYParamGetAccess(hDevice, "Distortion", &access) == TY_STATUS_OK &&
            (access & TY_ACCESS_READABLE)) {
            isDepthDistortion = true;
        }
    }

    LOGD("=== Read depth calibration data");
    ASSERT_OK(readCalibData(hDevice, depthSourceName.c_str(), cb_data.depth_calib));

    bool hasColorCam = false;

    if (depthSourceName == TYGetSourceSelectorName(SRC_SEL_DEPTH)) {
        uint32_t srcCnt = 0;
        TYEnumGetEntryCount(hDevice, "SourceSelector", &srcCnt);
        std::vector<TYEnumEntry> srcEntrys(srcCnt);
        TYEnumGetEntryInfo(hDevice, "SourceSelector", srcEntrys.data(), srcCnt, &srcCnt);
        for (uint32_t i = 0; i < srcCnt && !hasColorCam; i++) {
            const char* name = srcEntrys[i].name;
            if (strcmp(name, TYGetSourceSelectorName(SRC_SEL_DEPTH)) == 0) continue;
            if (strcmp(name, TYGetSourceSelectorName(SRC_SEL_LEFT))  == 0) continue;
            if (strcmp(name, TYGetSourceSelectorName(SRC_SEL_RIGHT)) == 0) continue;
            if (strcmp(name, depthSourceName.c_str()) == 0) continue;
            std::cout << name << std::endl;
            std::cout << "Do you want to enable the above components (Y/N)?" << std::endl;
            bool wait_cmd = true;
            do {
                char c;
                std::cin >> c;
                cin_clear_rest_line();
                switch (c) {
                case 'y': case 'Y':
                    if (readCalibData(hDevice, name, cb_data.color_calib) == TY_STATUS_OK) {
                        configureSourceFormat(hDevice, name);
                        hasColorCam = true;
                    } else {
                        std::cout << name << " has no valid calibration, skipping" << std::endl;
                    }
                    wait_cmd = false;
                    break;
                case 'n': case 'N':
                    std::cout << "Disable : " << name << std::endl;
                    wait_cmd = false;
                    break;
                default:
                    std::cout << "Error, please select again!" << std::endl;
                    break;
                }
            } while (wait_cmd);
        }
    }

    LOGD("=== Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK(TYGetFrameBufferSize(hDevice, &frameSize));
    std::vector<char> frameBuffer[2];
    frameBuffer[0].resize(frameSize);
    frameBuffer[1].resize(frameSize);
    ASSERT_OK(TYEnqueueBuffer(hDevice, frameBuffer[0].data(), frameSize));
    ASSERT_OK(TYEnqueueBuffer(hDevice, frameBuffer[1].data(), frameSize));

    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

    LOGD("=== Start capture");
    ASSERT_OK(TYStartCapture(hDevice));

    cb_data.index               = 0;
    cb_data.hDevice             = hDevice;
    cb_data.saveOneFramePoint3d = false;
    cb_data.fileIndex           = 0;
    cb_data.isDepthDistortion   = isDepthDistortion;
    cb_data.exit_main           = false;
    cb_data.map_depth_to_color  = hasColorCam;

    std::thread fetch_thread(FetchFrameThreadFunc, &cb_data);

    GLPointCloudViewer::ResetViewTranslate();
    GLPointCloudViewer::RegisterKeyCallback(key_pressed);
    GLPointCloudViewer::EnterMainLoop();
    cb_data.exit_main = true;
    fetch_thread.join();

    ASSERT_OK(TYStopCapture(hDevice));
    ASSERT_OK(TYCloseDevice(hDevice));
    ASSERT_OK(TYCloseInterface(hIface));
    ASSERT_OK(TYDeinitLib());

    LOGD("=== Main done!");
    GLPointCloudViewer::Deinit();
    return 0;
}
