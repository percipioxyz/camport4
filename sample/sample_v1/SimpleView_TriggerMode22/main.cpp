#include "common.hpp"

void eventCallback(TY_EVENT_INFO *event_info, void *userdata)
{
    if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE) {
        LOGD("=== Event Callback: Device Offline!");
        // Note: 
        //     Please set TY_BOOL_KEEP_ALIVE_ONOFF feature to false if you need to debug with breakpoint!
    } else if (event_info->eventId == TY_EVENT_LICENSE_ERROR) {
        LOGD("=== Event Callback: License Error!");
    }
}

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0){
            ID = argv[++i];
        } else if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        } else if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: SimpleView_TriggerMode22 [-h] [-id <ID>] [-ip <IP>]");
            return 0;
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

    TY_COMPONENT_ID allComps;
    ASSERT_OK( TYGetComponentIDs(hDevice, &allComps) );
    ASSERT_OK( TYDisableComponents(hDevice, allComps) );

    if (allComps & TY_COMPONENT_IR_CAM_LEFT) {
        LOGD("Has IR left camera, open IR left cam");
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_LEFT));
    }


    //depth map pixel format is uint16_t ,which default unit is  1 mm
    //the acutal depth (mm)= PixelValue * ScaleUnit 
    float scale_unit = 1.;

    //try to enable depth map
    LOGD("Configure components, open depth cam");
    if (allComps & TY_COMPONENT_DEPTH_CAM) {
        TY_IMAGE_MODE image_mode;
        ASSERT_OK(get_default_image_mode(hDevice, TY_COMPONENT_DEPTH_CAM, image_mode));
        LOGD("Select Depth Image Mode: %dx%d", TYImageWidth(image_mode), TYImageHeight(image_mode));
        ASSERT_OK(TYSetEnum(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, image_mode));
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_DEPTH_CAM));

        TYGetFloat(hDevice, TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &scale_unit);
    }

    TY_CAMERA_ROTATION cameraRotation;
    TY_CAMERA_INTRINSIC cameraRectifiedIntrinsic;
    TYLensOpticalType lens = TY_LENS_PINHOLE;

    TY_CAMERA_CALIB_INFO ir_calib_info;
    TYGetStruct(hDevice, TY_COMPONENT_IR_CAM_LEFT, TY_STRUCT_CAM_RECTIFIED_ROTATION, &cameraRotation, sizeof(cameraRotation));
    TYGetStruct(hDevice, TY_COMPONENT_IR_CAM_LEFT, TY_STRUCT_CAM_RECTIFIED_INTRI, &cameraRectifiedIntrinsic, sizeof(cameraRectifiedIntrinsic));
    TYGetStruct(hDevice, TY_COMPONENT_IR_CAM_LEFT, TY_STRUCT_CAM_CALIB_DATA, &ir_calib_info, sizeof(ir_calib_info));
    // TYGetEnum(hDevice, TY_COMPONENT_IR_CAM_LEFT, TY_ENUM_LENS_OPTICAL_TYPE, (uint32_t*)&lens);
    LOGD("Prepare image buffer");
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

    LOGD("Register event callback");
    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));

    bool hasTrigger;
    ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &hasTrigger));
    if (hasTrigger) {
        LOGD("Enable trigger mode");
        TY_TRIGGER_PARAM_EX trigger;
        trigger.mode = 22;
        trigger.led_expo = 1088;    // [3, 1649]
        trigger.led_gain = 32;      // [0, 255]
        ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &trigger, sizeof(trigger)));
    }

    LOGD("Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    LOGD("While loop to fetch frame");
    bool exit_main = false;
    TY_FRAME_DATA frame;
    int index = 0;
    while(!exit_main) {
        int err = TY_STATUS_OK;
        while(TY_STATUS_BUSY == (err = TYSendSoftTrigger(hDevice)));
        if (err != TY_STATUS_OK) {
            /*
             * Sometime we got other errors
             *     -1005 before 3.6.53 indicate trigger cmd timeout
             *     new sdk will return -1014 when trigger cmd timeout
             * If we got a timeout err, we do not know whether the
             * deivce missed the cmd or the app missied the ack.
             * We'd better restart the capture
             */
            LOGD("SendSoftTrigger failed with err(%d):%s\n", err, TYErrorString(err));
            break;
        }
        err = TYFetchFrame(hDevice, &frame, 20000);
        if( err == TY_STATUS_OK ) {
            LOGD("Get frame %d", ++index);

            int fps = get_fps();
            if (fps > 0){
                LOGI("fps: %d", fps);
            }

            for (int i = 0; i < frame.validCount; i++){
                if (frame.image[i].status != TY_STATUS_OK) continue;

                if(frame.image[i].componentID == TY_COMPONENT_DEPTH_CAM) {
                    auto win = ty_comp_window_name(frame.image[i].componentID);
                    TYDisplayImage(win.c_str(), frame.image[i].width, frame.image[i].height, TYPixelFormatCoord3D_C16, frame.image[i].buffer, scale_unit);
                }

                if(frame.image[i].componentID == TY_COMPONENT_IR_CAM_LEFT) {
                    uint32_t destSize;
                    std::vector<uint8_t> ir_data;
                    TYImageInfo image_info = ty_image_info(frame.image[i]);

                    TYPixFmt ir_fmt = image_info.format;
                    TYDecodeError err = TYGetDecodeBufferSize(&image_info, &destSize, TY_OUTPUT_FORMAT_AUTO);
                    if(err == TY_DECODE_SUCCESS) {
                        TYDecodeResult retInfo;
                        ir_data.resize(destSize);
                        ASSERT_DEC_OK(TYDecodeImage(&image_info,  TY_OUTPUT_FORMAT_AUTO, (void*)&ir_data[0], destSize, &retInfo));
                        ir_fmt = retInfo.format;
                    } else {
                        ir_data = std::vector<uint8_t>((uint8_t*)frame.image[i].buffer, (uint8_t*)frame.image[i].buffer + frame.image[i].size);
                    }

                    TY_IMAGE_DATA src;
                    src.width = frame.image[i].width;
                    src.height = frame.image[i].height;
                    src.size = frame.image[i].size;
                    src.pixelFormat = ir_fmt;
                    src.buffer = &ir_data[0];

                    std::vector<uint8_t> rectified_ir_data(ir_data.size());
                    TY_IMAGE_DATA dst;
                    dst.width = frame.image[i].width;
                    dst.height = frame.image[i].height;
                    dst.size = frame.image[i].size;;
                    dst.pixelFormat = ir_fmt;
                    dst.buffer = &rectified_ir_data[0];     
                
                    // Distortion rectification
                    ASSERT_OK(TYUndistortImage2 (&ir_calib_info,
                            &src,
                            &cameraRotation,
                            &cameraRectifiedIntrinsic,
                            &dst,
                            lens));

                    TYDisplayImage("rectified ir", dst.width, dst.height, ir_fmt, &ir_data[0]);
                }
            }

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
    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    delete frameBuffer[0];
    delete frameBuffer[1];

    LOGD("Main done!");
    return 0;
}
