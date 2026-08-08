/**
 * @file    main.cpp
 * @brief   Percipio Camera Region Output Control Demo (GenICam SFNC Standard)
 *
 * This demo demonstrates how to use the Percipio SDK's GenICam SFNC interface to control
 * Percipio cameras that support the Region output feature, including multi-Region ROI
 * configuration, image acquisition, calibration correction, and RGBD alignment.
 *
 * === Feature Overview ===
 *
 * 1. Region Output Control
 *    - Uses GenICam SFNC standard nodes RegionSelector / RegionMode to select and enable
 *      multiple Regions on the camera
 *    - Supports independent ROI configuration per Region: OffsetX, OffsetY, Width, Height
 *    - Region output allows the camera to transmit only the specified area, effectively
 *      reducing bandwidth and increasing frame rate
 *    - In this demo, Region output is enabled for the Depth stream and disabled for Color
 *
 * 2. Pixel Format Selection (PixelFormat)
 *    - Switches the active stream via SourceSelector (Depth / Texture)
 *    - Enumerates and interactively selects the pixel format for each stream
 *
 * 3. Binning Mode Selection
 *    - Enumerates and interactively selects the Binning mode for each stream
 *      (pixel binning / downsampling)
 *    - Binning parameters affect calibration correction calculations
 *
 * 4. Calibration Correction
 *    - Adjusts the camera's original calibration parameters based on Binning and Region
 *      Crop settings via TYAdjustCalibInfoByBinningCrop, ensuring accurate coordinate
 *      mapping between depth and color images
 *    - Performs undistortion on color images (TYUndistortImage)
 *
 * 5. RGBD Alignment (Optional)
 *    - Maps depth images to the color coordinate system (TYMapDepthImageToColorCoordinate)
 *    - Depth images from multiple Regions are individually mapped and merged into a
 *      unified color coordinate space
 *
 * === Program Execution Flow ===
 *
 *  1. Initialize SDK (TYInitLib), search for and open a camera device
 *  2. Configure Depth stream: select pixel format -> select Binning mode -> configure Region (enabled)
 *  3. Configure Color stream: select pixel format -> select Binning mode -> configure Region (disabled, full-frame output)
 *  4. Disable ComponentEnable for Left/Right IR streams
 *  5. Read Depth Scale Unit and calibration data
 *  6. Prompt user to choose whether to enable RGBD alignment
 *  7. Allocate frame buffers, register event callback, start capture
 *  8. Loop to fetch frame data:
 *     - Decode color images and apply undistortion
 *     - If alignment enabled: map each Region's depth to color coordinates and merge for display
 *     - If alignment disabled: display each Region's depth image directly
 *  9. Press 'q' to exit, stop capture and release resources
 *
 * === GenICam SFNC Nodes Used ===
 *
 *  | Node Name           | Description                                           |
 *  |---------------------|-------------------------------------------------------|
 *  | SourceSelector      | Select the active stream (Depth/Texture/Left/Right)   |
 *  | PixelFormat         | Set/query pixel format                                |
 *  | BinningHorizontal   | Set horizontal Binning mode                           |
 *  | RegionSelector      | Select the Region to configure                        |
 *  | RegionMode          | Enable/disable the selected Region (On/Off)           |
 *  | OffsetX / OffsetY   | Region offset                                         |
 *  | Width / Height      | Region dimensions                                     |
 *  | WidthMax / HeightMax| Maximum allowed dimensions for the Region             |
 *  | ComponentEnable     | Enable/disable stream component output                |
 *  | TY_DEPTH_SCALE      | Depth value scale factor                              |
 *
 * === Build & Run ===
 *
 *  See the project root CMakeLists.txt for build configuration.
 *  Use -id to specify device serial number, -ip to specify device IP address.
 *
 * === Notes ===
 *
 *  - Region feature requires camera hardware support; a message is shown if unsupported
 *  - With multi-Region output, frame data contains multiple images distinguished by regionID
 *  - RGBD alignment depends on accurate calibration; Binning and Region Crop affect calibration correction
 *  - This demo is interactive and requires user input from the terminal
 */

#include "../common/common.hpp"
#include "genicam_utils.hpp"
#include <vector>

struct RegionCallbackData {
    TY_DEV_HANDLE   hDevice;
    float           scale_unit;
    int             index;

    bool            do_alignment;

    TY_CAMERA_CALIB_INFO depth_calib;
    TY_CAMERA_CALIB_INFO color_calib;

    int32_t depth_binningX;
    int32_t depth_binningY;
    
    int32_t color_binningX;
    int32_t color_binningY;

    std::vector<uint16_t> depthBuffer;
    std::vector<uint8_t>  colorBuffer;
};

void eventCallback(TY_EVENT_INFO *event_info, void *userdata)
{
    if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE) {
        LOGD("=== Event Callback: Device Offline!");
    }
    else if (event_info->eventId == TY_EVENT_LICENSE_ERROR) {
        LOGD("=== Event Callback: License Error!");
    }
}

void selectFormat(TY_DEV_HANDLE hDevice, const char* stream)
{
    ASSERT_OK(TYEnumSetString(hDevice, "SourceSelector", stream));

    uint32_t m_FmtCnt = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "PixelFormat", &m_FmtCnt));

    std::vector<TYEnumEntry> entrys(m_FmtCnt);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "PixelFormat", entrys.data(), m_FmtCnt, &m_FmtCnt));
    if(m_FmtCnt == 1) {
        std::cout << stream << " only supports one format: " << entrys[0].name << std::endl;
    } else {
        std::cout << stream << " supports the following formats:" << std::endl;
        for(size_t i = 0; i < m_FmtCnt; i++) {
            std::cout << "\t" << std::dec << i << "." << entrys[i].name << std::endl;
        }

        std::cout << "Please select a format according to the above number!" << std::endl;

        int idx = -1;
        do {
            std::cin >> idx;
            cin_clear_rest_line();
            if(idx >= 0 && idx < (int)m_FmtCnt) break;
            else std::cout << "Error, please select again!" << std::endl;
        } while(true);

        std::cout << "======format idx = " << idx << std::endl;
        ASSERT_OK(idx >= m_FmtCnt);
        std::cout << "Select " << entrys[idx].name << std::endl;
        ASSERT_OK(TYEnumSetValue(hDevice, "PixelFormat", entrys[idx].value));
    }
}

int32_t selectBinning(TY_DEV_HANDLE hDevice, const char* stream)
{
    ASSERT_OK(TYEnumSetString(hDevice, "SourceSelector", stream));

    uint32_t m_BinningCnt = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "BinningHorizontal", &m_BinningCnt));
    std::vector<TYEnumEntry> entrys(m_BinningCnt);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "BinningHorizontal", entrys.data(), m_BinningCnt, &m_BinningCnt));
    if(m_BinningCnt == 1) {
        std::cout << stream << " only supports one binning mode: " << entrys[0].name << std::endl;
        ASSERT_OK(TYEnumSetValue(hDevice, "BinningHorizontal", entrys[0].value));
        return (int32_t)entrys[0].value;
    } else {
        std::cout << stream << " supports the following binning mode:" << std::endl;
        for(uint32_t i = 0; i < m_BinningCnt; i++) {
            std::cout << "\t" << std::dec << i << "." << entrys[i].name << std::endl;
        }

        std::cout << "Please select a binning mode according to the above number!" << std::endl;
        int idx = -1;
        do {
            std::cin >> idx;
            cin_clear_rest_line();
            if(idx >= 0 && idx < (int)m_BinningCnt) break;
            else std::cout << "Error, please select again!" << std::endl;
        }while(true);

        std::cout << "======binning idx = " << idx << std::endl;
        ASSERT_OK(idx >= m_BinningCnt);
        std::cout << "Select " << entrys[idx].name << std::endl;
        ASSERT_OK(TYEnumSetValue(hDevice, "BinningHorizontal", entrys[idx].value));
        return (int32_t)entrys[idx].value;
    }
}

void configRegion(TY_DEV_HANDLE hDevice, const char* stream, const bool region_enable)
{
    ASSERT_OK(TYEnumSetString(hDevice, "SourceSelector", stream));

    bool support_region = false;
    ASSERT_OK(TYParamExist(hDevice, "RegionSelector", &support_region));
    if(!support_region) {
        std::cout << stream << " does not support region control!" << std::endl;
        return;
    }

    uint32_t region_count = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "RegionSelector", &region_count));
    if(!region_count) {
        std::cout << stream << " does not support region control!" << std::endl;
        return;
    }

    std::vector<TYEnumEntry> region_lists(region_count);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "RegionSelector", region_lists.data(), region_count, &region_count));
    if(!region_count) {
        std::cout << stream << " does not support region control!" << std::endl;
        return;
    }

    std::cout << "=== Clearing all region modes for " << stream << " ===" << std::endl;
    for(size_t i = 0; i < region_count; i++) {
        ASSERT_OK(TYEnumSetString(hDevice, "RegionSelector", region_lists[i].name));

        TY_ACCESS_MODE _access = 0;
        ASSERT_OK(TYParamGetAccess(hDevice, "RegionMode", &_access));
        if(_access & TY_ACCESS_WRITABLE) {
            ASSERT_OK(TYEnumSetString(hDevice, "RegionMode", "Off"));
        }
    }

    if(!region_enable) {
        std::cout << "=== Region control is disabled for " << stream << " ===" << std::endl;
        ASSERT_OK(TYBooleanSetValue(hDevice, "ComponentEnable", true));
        return;
    }

    std::cout << stream << " supports the following regions:" << std::endl;
    for(size_t i = 0; i < region_count; i++) {
        std::cout << "\t" << std::dec << i << "." << region_lists[i].name << std::endl;
    }

    int32_t num_regions = 0;
    std::cout << "How many regions do you want to enable for " << stream
              << "? (1 ~ " << region_count << "):" << std::endl;
    do {
        std::cin >> num_regions;
        cin_clear_rest_line();
        if(num_regions >= 1 && num_regions <= (int32_t)region_count) break;
        std::cout << "Error: please input a number between 1 and " << region_count << "!" << std::endl;
    } while(true);

    for(int32_t r = 0; r < num_regions; r++) {
        ASSERT_OK(TYEnumSetString(hDevice, "RegionSelector", region_lists[r].name));
        std::cout << "=== Configuring " << stream << " region " << r
                  << " (" << region_lists[r].name << ") ===" << std::endl;

        int64_t max_width = 0;
        int64_t max_height = 0;
        ASSERT_OK(TYIntegerGetValue(hDevice, "WidthMax", &max_width));
        ASSERT_OK(TYIntegerGetValue(hDevice, "HeightMax", &max_height));
        std::cout << stream << " Region " << r << " WidthMax = " << max_width << std::endl;
        std::cout << stream << " Region " << r << " HeightMax = " << max_height << std::endl;

        int64_t offset_x = 0, offset_y = 0, width = 0, height = 0;

        std::cout << "Please input OffsetX for " << stream << " region " << r
                  << " (0 ~ " << max_width - 1 << "):" << std::endl;
        do {
            std::cin >> offset_x;
            cin_clear_rest_line();
            if(offset_x >= 0 && offset_x < max_width) break;
            std::cout << "Error: OffsetX must be in [0, " << max_width - 1 << "], please input again!" << std::endl;
        } while(true);

        std::cout << "Please input OffsetY for " << stream << " region " << r
                  << " (0 ~ " << max_height - 1 << "):" << std::endl;
        do {
            std::cin >> offset_y;
            cin_clear_rest_line();
            if(offset_y >= 0 && offset_y < max_height) break;
            std::cout << "Error: OffsetY must be in [0, " << max_height - 1 << "], please input again!" << std::endl;
        } while(true);

        std::cout << "Please input Width for " << stream << " region " << r
                  << " (1 ~ " << max_width - offset_x << "):" << std::endl;
        do {
            std::cin >> width;
            cin_clear_rest_line();
            if(width > 0 && (offset_x + width) <= max_width) break;
            std::cout << "Error: OffsetX + Width must not exceed WidthMax (" << max_width << "), please input again!" << std::endl;
        } while(true);

        std::cout << "Please input Height for " << stream << " region " << r
                  << " (1 ~ " << max_height - offset_y << "):" << std::endl;
        do {
            std::cin >> height;
            cin_clear_rest_line();
            if(height > 0 && (offset_y + height) <= max_height) break;
            std::cout << "Error: OffsetY + Height must not exceed HeightMax (" << max_height << "), please input again!" << std::endl;
        } while(true);

        std::cout << stream << " Region " << r << " ROI: OffsetX=" << offset_x
                  << " OffsetY=" << offset_y
                  << " Width=" << width
                  << " Height=" << height << std::endl;

        ASSERT_OK(TYIntegerSetValue(hDevice, "OffsetX", offset_x));
        ASSERT_OK(TYIntegerSetValue(hDevice, "OffsetY", offset_y));
        ASSERT_OK(TYIntegerSetValue(hDevice, "Width", width));
        ASSERT_OK(TYIntegerSetValue(hDevice, "Height", height));

        ASSERT_OK(TYEnumSetString(hDevice, "RegionMode", "On"));
        std::cout << stream << " Region " << r << " RegionMode set to On" << std::endl;
    }

    ASSERT_OK(TYBooleanSetValue(hDevice, "ComponentEnable", true));
    std::cout << stream << " ComponentEnable set to true" << std::endl;
}

static bool adjustCalibFromImage(const TY_IMAGE_DATA& image, const int32_t binningX, const int32_t binningY, const TY_CAMERA_CALIB_INFO& srcCalib,
                                 TY_CAMERA_CALIB_INFO& dstCalib)
{
    dstCalib = srcCalib;
    TY_STATUS ret = TYAdjustCalibInfoByBinningCrop(&srcCalib
        , binningX
        , binningY
        , image.cropOffsetX
        , image.cropOffsetY
        , image.width
        , image.height
        , &dstCalib
    );
    if (ret != TY_STATUS_OK) {
        LOGE("TYAdjustCalibInfoByBinningCrop failed: %d(%s)", ret, TYErrorString(ret));
        LOGE("RegionID = %d, binningX = %d, binningY = %d, cropOffsetX = %d, cropOffsetY = %d, width = %d, height = %d", image.regionID,
             binningX, binningY, image.cropOffsetX, image.cropOffsetY, image.width, image.height);
        return false;
    } else {
        LOGD("TYAdjustCalibInfoByBinningCrop OK");
        LOGD("RegionID = %d, binningX = %d, binningY = %d, cropOffsetX = %d, cropOffsetY = %d, width = %d, height = %d",
             image.regionID, binningX, binningY, image.cropOffsetX, image.cropOffsetY, image.width, image.height);
    }
    return true;
}

void handleFrame(TY_FRAME_DATA* frame, void* userdata)
{
    RegionCallbackData* pData = (RegionCallbackData*)userdata;
    LOGD("=== Get frame %d", ++pData->index);

    std::vector<TY_IMAGE_DATA*> depthImages;
    std::vector<TY_IMAGE_DATA*> colorImages;

    for (int i = 0; i < frame->validCount; i++) {
        if (frame->image[i].status != TY_STATUS_OK) continue;

        if(frame->image[i].componentID == TY_COMPONENT_DEPTH_CAM) {
            depthImages.push_back(&frame->image[i]);
        } else if(frame->image[i].componentID == TY_COMPONENT_RGB_CAM) {
            colorImages.push_back(&frame->image[i]);
        }
    }

    for(size_t i = 0; i < colorImages.size(); i++) {
        TY_CAMERA_CALIB_INFO adjusted_color_calib;
        if (!adjustCalibFromImage(*colorImages[i], pData->color_binningX, pData->color_binningY, pData->color_calib, adjusted_color_calib)) {
            continue;
        }

        const TYImageInfo color_info = ty_image_info(*colorImages[i]);
        uint32_t colorDestSize = 0;
        TYDecodeResult colorDecode;
        if (TYGetDecodeBufferSize(&color_info, &colorDestSize, TY_OUTPUT_FORMAT_BGR) == TY_DECODE_SUCCESS) {
            pData->colorBuffer.resize(colorDestSize);
            ASSERT_DEC_OK(TYDecodeImage(&color_info, TY_OUTPUT_FORMAT_BGR,
                                        pData->colorBuffer.data(),
                                        colorDestSize, &colorDecode));

            TY_IMAGE_DATA src, dst;
            src.width  = colorImages[i]->width;
            src.height = colorImages[i]->height;
            src.size   = colorDestSize;
            src.pixelFormat = TYPixelFormatBGR8;
            src.buffer = pData->colorBuffer.data();

            std::vector<uint8_t> undistColor(colorDestSize);
            dst.width  = colorImages[i]->width;
            dst.height = colorImages[i]->height;
            dst.size   = colorDestSize;
            dst.pixelFormat = TYPixelFormatBGR8;
            dst.buffer = undistColor.data();

            ASSERT_OK(TYUndistortImage(&adjusted_color_calib, &src, NULL, &dst));
            pData->colorBuffer = std::move(undistColor);

            char color_win[260];
            sprintf(color_win, "undistort_color_region_%d", (int)i);
            TYDisplayImage(color_win,
                               colorImages[i]->width, colorImages[i]->height,
                               TYPixelFormatBGR8,
                               pData->colorBuffer.data());
        }
    }

    if(pData->do_alignment && colorImages.size() > 0) {
        int mappedW = colorImages[0]->width;
        int mappedH = colorImages[0]->height;

        std::vector<uint16_t> out(mappedW * mappedH, 0);
        for(size_t i = 0; i < depthImages.size(); i++) {
            TY_CAMERA_CALIB_INFO adjusted_depth_calib;
            TY_CAMERA_CALIB_INFO adjusted_color_calib;
            if (!adjustCalibFromImage(*depthImages[i], pData->depth_binningX, pData->depth_binningY, pData->depth_calib, adjusted_depth_calib)) {
                continue;
            }
            if (!adjustCalibFromImage(*colorImages[0], pData->color_binningX, pData->color_binningY, pData->color_calib, adjusted_color_calib)) {
                continue;
            }

            std::vector<uint16_t> region_depth(mappedW * mappedH, 0);
            ASSERT_OK(TYMapDepthImageToColorCoordinate(
                &adjusted_depth_calib, depthImages[i]->width, depthImages[i]->height, (const uint16_t*)depthImages[i]->buffer,
                &adjusted_color_calib, mappedW, mappedH,
                reinterpret_cast<uint16_t*>(region_depth.data()),
                pData->scale_unit));

            for(size_t idx = 0; idx < mappedW * mappedH; idx++) {
                if(region_depth[idx] != 0) {
                    out[idx] = region_depth[idx];
                }
            }
        }
        
        char depth_win[260];
        sprintf(depth_win, "mapped_depth_region");
        TYDisplayImage(depth_win,
                        mappedW, mappedH,
                        TYPixelFormatCoord3D_C16,
                        reinterpret_cast<uint8_t*>(out.data()),
                        pData->scale_unit);

    } else {
        for(size_t i = 0; i < depthImages.size(); i++) {
            char depth_win[260];
            sprintf(depth_win, "depth_region_%d", (int)i);
            TYDisplayImage(depth_win,
                               depthImages[i]->width, depthImages[i]->height,
                               TYPixelFormatCoord3D_C16,
                               depthImages[i]->buffer,
                               pData->scale_unit);
        }
    }
}

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-id") == 0) {
          ID = argv[++i];
        } else if (strcmp(argv[i], "-ip") == 0) {
          IP = argv[++i];
        }
    }

    LOGD("Init lib");
    ASSERT_OK( TYInitLib() );
    TY_VERSION_INFO ver;
    ASSERT_OK( TYLibVersion(&ver) );
    LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    std::vector<TY_DEVICE_BASE_INFO> selected;
    ASSERT_OK( selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected) );
    ASSERT(selected.size() > 0);
    TY_DEVICE_BASE_INFO& selectedDev = selected[0];

    ASSERT_OK( TYOpenInterface(selectedDev.iface.id, &hIface) );
    ASSERT_OK( TYOpenDevice(hIface, selectedDev.id, &hDevice) );

    TY_CAMERA_CALIB_INFO depth_calib, color_calib;

    std::cout << "=== Configuring Depth stream ===" << std::endl;
    selectFormat(hDevice, "Depth");
    int32_t depth_binningX = selectBinning(hDevice, "Depth");
    int32_t depth_binningY = depth_binningX;
    configRegion(hDevice, "Depth", true);

    std::cout << "=== Configuring Color stream ===" << std::endl;
    selectFormat(hDevice, "Texture");
    int32_t color_binningX = selectBinning(hDevice, "Texture");
    int32_t color_binningY = color_binningX;
    configRegion(hDevice, "Texture", false);

    uint32_t m_Source = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "SourceSelector", &m_Source));
    std::vector<TYEnumEntry> entrys(m_Source);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "SourceSelector", entrys.data(), m_Source, &m_Source));
    for(size_t i = 0; i < m_Source; i++) {
        if(strcmp(entrys[i].name, "Left") == 0 || strcmp(entrys[i].name, "Right") == 0) {
            ASSERT_OK(TYEnumSetString(hDevice, "SourceSelector", entrys[i].name));
            ASSERT_OK(TYBooleanSetValue(hDevice, "ComponentEnable", false));
            std::cout << "Disabled: " << entrys[i].name << std::endl;
        }
    }

    double depth_scale_unit = 1.0;
    if(TY_STATUS_OK == TYEnumSetValue(hDevice, "SourceSelector", SRC_SEL_DEPTH)) {
        ASSERT_OK(TYFloatGetValue(hDevice, TY_DEPTH_SCALE, &depth_scale_unit));
    }

    bool do_alignment = false;
    std::cout << "Do you want to perform RGBD alignment on the region output? (Y/N):" << std::endl;
    do {
        char c;
        std::cin >> c;
        cin_clear_rest_line();
        if (c == 'y' || c == 'Y') {
            do_alignment = true;
            break;
        } else if (c == 'n' || c == 'N') {
            do_alignment = false;
            break;
        } else {
            std::cout << "Error: please input Y/y/N/n!" << std::endl;
        }
    } while(true);

    std::cout << "RGBD alignment: " << (do_alignment ? "Enabled" : "Disabled") << std::endl;

    RegionCallbackData cb_data;
    cb_data.hDevice    = hDevice;
    cb_data.scale_unit = static_cast<float>(depth_scale_unit);
    cb_data.index = 0;

    cb_data.depth_binningX = depth_binningX;
    cb_data.depth_binningY = depth_binningY;
    
    cb_data.color_binningX = color_binningX;
    cb_data.color_binningY = color_binningY;

    cb_data.do_alignment = do_alignment;

    readCalibData(hDevice, "Depth",  cb_data.depth_calib);
    readCalibData(hDevice, "Texture", cb_data.color_calib);
    
    LOGD("Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
    LOGD("     - Get size of framebuffer, %d", frameSize);

    std::vector<char> frameBuffer[2];
    LOGD("     - Allocate & enqueue buffers");
    frameBuffer[0].resize(frameSize);
    frameBuffer[1].resize(frameSize);
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0].data(), frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[0].data(), frameSize) );
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1].data(), frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[1].data(), frameSize) );

    LOGD("Register event callback");
    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

    LOGD("Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    LOGD("While loop to fetch frame");
    bool exit_main = false;
    TY_FRAME_DATA frame;
    int index = 0;
    while(!exit_main) {
        int err = TYFetchFrame(hDevice, &frame, -1);
        if( err == TY_STATUS_OK ) {
            LOGD("Get frame %d", ++index);

            float fps = get_fps();
            if (fps > 0){
                LOGI("fps: %.2f", fps);
            }

            handleFrame(&frame, &cb_data);

            int key = TYWaitKeyEvents();
            switch(key & 0xff) {
            case 0xff:
                break;
            case 'q':
                exit_main = true;
                break;
            default:
                LOGD("Unmapped key %d", key);
            }
            LOGD("Re-enqueue buffer(%p, %d)"
                , frame.userBuffer, frame.bufferSize);
            ASSERT_OK( TYEnqueueBuffer(hDevice, frame.userBuffer, frame.bufferSize) );
        }
    }
    ASSERT_OK( TYStopCapture(hDevice) );
    ASSERT_OK( TYClearBufferQueue(hDevice) );

    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );

    LOGD("Main done!");
    return 0;
}
