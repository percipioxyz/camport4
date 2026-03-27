#include "common.hpp"

#define MAP_DEPTH_TO_COLOR  1

struct CallbackData {
    int             index;
    TY_DEV_HANDLE   hDevice;
    float           scale_unit;
    bool            isDepthDistortion;

    TY_CAMERA_CALIB_INFO depth_calib;
    TY_CAMERA_CALIB_INFO color_calib;

    std::vector<uint16_t> depthBuffer;
    std::vector<uint8_t> colorBuffer;
};

static void doRegister(const TY_CAMERA_CALIB_INFO& depth_calib, const TY_CAMERA_CALIB_INFO& color_calib,
                      const uint16_t* depth, const int32_t depthWidth, const int32_t depthHeight, const float f_scale_unit,
                      const uint8_t* color, const int32_t colorWidth, const int32_t colorHeight, std::vector<uint8_t>&out, bool map_depth_to_color)
{
    if (map_depth_to_color) {
        out.resize(colorWidth * colorHeight * sizeof(uint16_t));
        ASSERT_OK(TYMapDepthImageToColorCoordinate(
            &depth_calib, depthWidth, depthHeight, depth,
            &color_calib, colorWidth, colorHeight, (uint16_t*)&out[0], 
            f_scale_unit));
    } else {
        out.resize(depthWidth * depthHeight * sizeof(uint8_t) * 3);
        ASSERT_OK(TYMapRGBImageToDepthCoordinate(
            &depth_calib, depthWidth, depthHeight, depth,
            &color_calib, colorWidth, colorHeight, color,
            &out[0], 
            f_scale_unit));
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
      
        if (frame->image[i].componentID == TY_COMPONENT_DEPTH_CAM) {
            depthImage = &frame->image[i];
        }
        else if (frame->image[i].componentID == TY_COMPONENT_RGB_CAM) {
            colorImage = &frame->image[i];
        }
    }

    if (depthImage != nullptr) {
        pData->depthBuffer.resize(depthImage->width * depthImage->height);
        if (pData->isDepthDistortion) {
            TY_IMAGE_DATA src, dst;
            src.width = depthImage->width;
            src.height = depthImage->height;
            src.size = depthImage->size; // uint16_t
            src.pixelFormat = TYPixelFormatCoord3D_C16; 
            src.buffer = (void*)depthImage->buffer;

            std::vector<uint16_t> undistortDepthBuffer(depthImage->width * depthImage->height);
            dst.width = depthImage->width;
            dst.height = depthImage->height;
            dst.size = src.size;
            dst.buffer = (void*)&undistortDepthBuffer[0];
            dst.pixelFormat = TYPixelFormatCoord3D_C16;

            ASSERT_OK(TYUndistortImage(&pData->depth_calib, &src, NULL, &dst));
            memcpy(&pData->depthBuffer[0], &undistortDepthBuffer[0], depthImage->size);
        } else {
            memcpy(&pData->depthBuffer[0], depthImage->buffer, depthImage->size);
        }

        if (colorImage != nullptr) {
            const TYImageInfo color_info = ty_image_info(*colorImage);
            uint32_t colorDestSize = 0;
            TYDecodeResult colorDecode;
            TYDecodeError colorDecodeErr = TYGetDecodeBufferSize(&color_info, &colorDestSize, TY_OUTPUT_FORMAT_BGR);
            if (colorDecodeErr == TY_DECODE_SUCCESS) {
                pData->colorBuffer.resize(colorDestSize);
                ASSERT_DEC_OK(TYDecodeImage(&color_info, TY_OUTPUT_FORMAT_BGR, (void*)&pData->colorBuffer[0], colorDestSize, &colorDecode));

                TY_IMAGE_DATA src, dst;
                src.width = colorImage->width;
                src.height = colorImage->height;
                src.size = colorImage->size; // uint16_t
                src.pixelFormat = TYPixelFormatBGR8; 
                src.buffer = (void*)&pData->colorBuffer[0];

                std::vector<uint8_t> undistortColorBuffer(colorDestSize);
                dst.width = colorImage->width;
                dst.height = colorImage->height;
                dst.size = colorImage->size;
                dst.pixelFormat = TYPixelFormatBGR8;
                dst.buffer = (void*)&undistortColorBuffer[0];

                ASSERT_OK(TYUndistortImage(&pData->color_calib, &src, NULL, &dst));
                pData->colorBuffer = std::move(undistortColorBuffer);

                std::vector<uint8_t> out;
                doRegister(pData->depth_calib, pData->color_calib, 
                      pData->depthBuffer.data(), depthImage->width, depthImage->height, pData->scale_unit, 
                      pData->colorBuffer.data(), colorImage->width, colorImage->height, out, MAP_DEPTH_TO_COLOR);

                if(MAP_DEPTH_TO_COLOR) {
                    TYDisplayImage("depth2color", colorImage->width, colorImage->height, TYPixelFormatCoord3D_C16, &out[0], pData->scale_unit);
                    TYDisplayImage("undistort color", colorImage->width, colorImage->height, TYPixelFormatBGR8, &pData->colorBuffer[0]);
                } else {
                    TYDisplayImage("depth", depthImage->width, depthImage->height, TYPixelFormatCoord3D_C16, &pData->depthBuffer[0], pData->scale_unit);
                    TYDisplayImage("mapped RGB", depthImage->width, depthImage->height, TYPixelFormatBGR8, &out[0]);
                }
            }
        }
    }

    LOGD("=== Re-enqueue buffer(%p, %d)", frame->userBuffer, frame->bufferSize);
    ASSERT_OK(TYEnqueueBuffer(pData->hDevice, frame->userBuffer, frame->bufferSize));
}

void eventCallback(TY_EVENT_INFO *event_info, void *userdata)
{
    if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE) {
        LOGD("=== Event Callback: Device Offline!");
        // Note: 
        //     Please set TY_BOOL_KEEP_ALIVE_ONOFF feature to false if you need to debug with breakpoint!
    }
    else if (event_info->eventId == TY_EVENT_LICENSE_ERROR) {
        LOGD("=== Event Callback: License Error!");
    }
}

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;

    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "-id") == 0){
            ID = argv[++i];
        } else if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        }else if(strcmp(argv[i], "-h") == 0){
            LOGI("Usage: SimpleView_Registration [-h] [-id <ID>]");
            return 0;
        }
    }
    
    LOGD("=== Init lib");
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

    TY_COMPONENT_ID allComps;
    ASSERT_OK( TYGetComponentIDs(hDevice, &allComps) );
    ASSERT_OK( TYDisableComponents(hDevice, allComps) );
    
    if(!(allComps & TY_COMPONENT_RGB_CAM)){
        LOGE("=== Has no RGB camera, cant do registration");
        return -1;
    }
    
    LOGD("=== Configure components");
    TY_COMPONENT_ID componentIDs = TY_COMPONENT_DEPTH_CAM | TY_COMPONENT_RGB_CAM;
    ASSERT_OK( TYEnableComponents(hDevice, componentIDs) );

    // ASSERT_OK( TYSetEnum(hDevice, TY_COMPONENT_RGB_CAM, TY_ENUM_IMAGE_MODE, TY_IMAGE_MODE_YUYV_640x480) );
    bool hasUndistortSwitch, hasDistortionCoef;
    ASSERT_OK( TYHasFeature(hDevice, TY_COMPONENT_RGB_CAM, TY_BOOL_UNDISTORTION, &hasUndistortSwitch) );
    ASSERT_OK( TYHasFeature(hDevice, TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_DISTORTION, &hasDistortionCoef) );
    if (hasUndistortSwitch) {
      ASSERT_OK( TYSetBool(hDevice, TY_COMPONENT_RGB_CAM, TY_BOOL_UNDISTORTION, true) );
    }

    LOGD("=== Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
    LOGD("     - Get size of framebuffer, %d", frameSize);
    LOGD("     - Allocate & enqueue buffers");
    char* frameBuffer[2];
    frameBuffer[0] = new char[frameSize];
    frameBuffer[1] = new char[frameSize];
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize) );
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize) );

    LOGD("=== Register event callback");
    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

    bool hasTriggerParam = false;
    ASSERT_OK( TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &hasTriggerParam) );
    if (hasTriggerParam) {
        LOGD("=== Disable trigger mode");
        TY_TRIGGER_PARAM_EX trigger;
        trigger.mode = TY_TRIGGER_MODE_OFF;
        ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &trigger, sizeof(trigger)));
    }

    CallbackData cb_data;
    cb_data.index = 0;
    cb_data.hDevice = hDevice;

    float scale_unit = 1.;
    TYGetFloat(hDevice, TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &scale_unit);
    cb_data.scale_unit = scale_unit;

    LOGD("=== Read depth calib info");
    ASSERT_OK( TYGetStruct(hDevice, TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA
          , &cb_data.depth_calib, sizeof(cb_data.depth_calib)) );

    LOGD("=== Read color calib info");
    ASSERT_OK( TYGetStruct(hDevice, TY_COMPONENT_RGB_CAM, TY_STRUCT_CAM_CALIB_DATA
          , &cb_data.color_calib, sizeof(cb_data.color_calib)) );

    ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_DISTORTION, &cb_data.isDepthDistortion));

    LOGD("=== Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    LOGD("=== Wait for callback");
    bool exit_main = false;
    while(!exit_main){
        TY_FRAME_DATA frame;
        int err = TYFetchFrame(hDevice, &frame, -1);
        if( err != TY_STATUS_OK ) {
            LOGE("Fetch frame error %d: %s", err, TYErrorString(err));
            break;
        }

        handleFrame(&frame, &cb_data);
        int key = TYWaitKeyEvents();
        switch(key & 0xff){
            case 0xff:
                break;
            case 'q':
                exit_main = true;
                break;
            default:
                LOGD("Pressed key %d", key);
        }
    }

    ASSERT_OK( TYStopCapture(hDevice) );
    ASSERT_OK( TYCloseDevice(hDevice) );
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    delete frameBuffer[0];
    delete frameBuffer[1];

    LOGD("=== Main done!");
    return 0;
}
