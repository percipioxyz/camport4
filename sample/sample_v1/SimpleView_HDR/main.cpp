#include "common.hpp"

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

static inline int covertToLinear(const uint16_t* raw16, int width, int height, uint32_t *param, std::vector<int32_t>& raw32)
{
    raw32.resize(width * height);
    int *dst = (int *)&raw32[0];
    uint32_t R1 = param[0];
    uint32_t R2 = param[1];
    R1 = 1 << (R1 + 2);
    R2 = 1 << (R2 + 2);
    uint32_t P1 = 1 << param[6];
    uint32_t P2 = 1 << param[7];
    uint32_t Pk = (int)((P2 - P1)/(4.0f*R1)) + P1;
    for (int i = 0; i < width * height; i++) {
        if (raw16[i] <= P1) {
            dst[i] = (int)raw16[i];
        } else if (raw16[i] > P1 && raw16[i] <= Pk) {
            dst[i] = ((int)raw16[i] - P1)* 4* R1 + P1;
        } else {
            dst[i] = ((int)raw16[i] - Pk)* 4* R1 * R2 + P2;
        }
    }
    return 0;
}

void convertInt32ToUint16(const int32_t* data, int width, int height, uint16_t* dst) {
    const int totalPixels = width * height;
    
    for (int i = 0; i < totalPixels; ++i) {
        double value = static_cast<double>(data[i]);
        
        int32_t roundedValue = static_cast<int32_t>(value >= 0 ? value + 0.5 : value - 0.5);
        
        if (roundedValue < 0) {
            dst[i] = 0;
        } else if (roundedValue > 65535) {
            dst[i] = 65535;
        } else {
            dst[i] = static_cast<uint16_t>(roundedValue);
        }
    }
}

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    int32_t color, ir, depth;
    color = ir = depth = 1;
    int R1 = 0, R2 = 0;
    bool hdr_enable = true;
    int expo = -1;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0){
            ID = argv[++i];
        } else if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        } else if(strcmp(argv[i], "-color=off") == 0) {
            color = 0;
        } else if(strcmp(argv[i], "-depth=off") == 0) {
            depth = 0;
        } else if(strcmp(argv[i], "-ir=off") == 0) {
            ir = 0;
        } else if(strcmp(argv[i], "-R1") == 0) {
            R1 = atoi(argv[++i]);
            if (R1 > 2) {
                LOGD("R1 %d is out of range force to 2\n", R1);
                R1 = 2;
            }
        } else if(strcmp(argv[i], "-R2") == 0) {
            R2 = atoi(argv[++i]);
            if (R2 > 2) {
                LOGD("R2 %d is out of range force to 2\n", R2);
                R2 = 2;
            }
        } else if(strcmp(argv[i], "-HDR") == 0) {
            hdr_enable = atoi(argv[++i]) == 0 ? false : true;
        } else if (strcmp(argv[i], "-expo") == 0) {
            expo = atoi(argv[++i]);
        } else if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: SimpleView_HDR [-h] [-id <ID>] [-HDR en] [-R1 r1] [-R2 r2] [-expo ex]");
            return 0;
        }
    }

    if (!color && !depth && !ir) {
        LOGD("At least one component need to be on");
        return -1;
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

    ///try to enable color camera
    if(allComps & TY_COMPONENT_RGB_CAM  && color) {
        LOGD("Has RGB camera, open RGB cam");
        ASSERT_OK( TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM) );
    }

    if (allComps & TY_COMPONENT_IR_CAM_LEFT && ir) {
        LOGD("Has IR left camera, open IR left cam");
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_LEFT));
    }

    if (allComps & TY_COMPONENT_IR_CAM_RIGHT && ir) {
        LOGD("Has IR right camera, open IR right cam");
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_RIGHT));
    }

    //depth map pixel format is uint16_t ,which default unit is  1 mm
    //the acutal depth (mm)= PixelValue * ScaleUnit 
    float scale_unit = 1.;

    //try to enable depth map
    LOGD("Configure components, open depth cam");
    if (allComps & TY_COMPONENT_DEPTH_CAM && depth) {
        TY_IMAGE_MODE image_mode;
        ASSERT_OK(get_default_image_mode(hDevice, TY_COMPONENT_DEPTH_CAM, image_mode));
        LOGD("Select Depth Image Mode: %dx%d", TYImageWidth(image_mode), TYImageHeight(image_mode));
        ASSERT_OK(TYSetEnum(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, image_mode));
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_DEPTH_CAM));

        TYGetFloat(hDevice, TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &scale_unit);
    }
    bool hasHDR = false;
    ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_RGB_CAM, TY_BOOL_HDR, &hasHDR));
    if (hasHDR) {
        ASSERT_OK(TYSetBool(hDevice, TY_COMPONENT_RGB_CAM, TY_BOOL_HDR, hdr_enable));
    }
    ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_RGB_CAM, TY_BYTEARRAY_HDR_PARAMETER, &hasHDR));
    if (hdr_enable && hasHDR) {
        uint32_t hdr_para[8];
        hdr_para[0] = R1;
        hdr_para[1] = R2;
        ASSERT_OK(TYSetByteArray(hDevice, TY_COMPONENT_RGB_CAM, TY_BYTEARRAY_HDR_PARAMETER, (uint8_t *)&hdr_para[0], 32));
        LOGD("set hdr param R1 %d, R2 %d\n", R1, R2);
    }

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
        LOGD("Disable trigger mode");
        TY_TRIGGER_PARAM_EX trigger;
        trigger.mode = TY_TRIGGER_MODE_OFF;
        ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &trigger, sizeof(trigger)));
    }

    if (!(expo < 0)) {
        ASSERT_OK(TYSetInt(hDevice, TY_COMPONENT_RGB_CAM, TY_INT_EXPOSURE_TIME, expo));
    }
    LOGD("Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    LOGD("While loop to fetch frame");
    bool exit_main = false;
    TY_FRAME_DATA frame;
    int index = 0;
    uint32_t hdr_param[8];
    while(!exit_main) {
        int err = TYFetchFrame(hDevice, &frame, -1);
        if( err == TY_STATUS_OK ) {
            LOGD("Get frame %d", ++index);

            int fps = get_fps();
            if (fps > 0){
                LOGI("fps: %d", fps);
            }
            for (int i = 0; i < frame.validCount; i++){
                if (frame.image[i].status != TY_STATUS_OK) continue;

                uint32_t destSize;
                auto win = ty_comp_window_name(frame.image[i].componentID);
                TYImageInfo image_info = ty_image_info(frame.image[i]);
                if(frame.image[i].componentID == TY_COMPONENT_RGB_CAM) {
                    TYDecodeError err = TYGetDecodeBufferSize(&image_info, &destSize, TY_OUTPUT_FORMAT_MONO16);
                    if(err == TY_DECODE_SUCCESS) {
                        TYDecodeResult retInfo;
                        std::vector<uint16_t> mono16(destSize / sizeof(uint16_t));
                        ASSERT_OK(TYDecodeImage(&image_info,  TY_OUTPUT_FORMAT_AUTO, (void*)&mono16[0], destSize, &retInfo));

                        uint32_t _hdr_param[8];
                        memset(_hdr_param, 0, sizeof(_hdr_param));
                        if (hdr_enable && hasHDR) {
                            //May changed first frame after adjust R1 R2
                            //can disabled when is stabled, seems to depends on R1 R2
                            TYGetByteArray(hDevice, TY_COMPONENT_RGB_CAM, TY_BYTEARRAY_HDR_PARAMETER, (uint8_t *)&_hdr_param[0], 32);
                            if (hdr_param[0] != _hdr_param[0] ||
                                hdr_param[1] != _hdr_param[1] ||
                                hdr_param[6] != _hdr_param[6] ||
                                hdr_param[7] != _hdr_param[7]) {
                                LOGD("hdr param changed {%u %u %u %u}->{%u %u %u %u}",
                                hdr_param[0],hdr_param[1],hdr_param[6],hdr_param[7],
                                _hdr_param[0],_hdr_param[1],_hdr_param[6],_hdr_param[7]);
                                memcpy(hdr_param, _hdr_param, sizeof(_hdr_param));
                            }

                            std::vector<int32_t> raw32;
                            covertToLinear(&mono16[0], retInfo.width, retInfo.height, _hdr_param, raw32);
                            convertInt32ToUint16(raw32.data(), retInfo.width, retInfo.height, &mono16[0]);
                        }
                        TYDisplayImage(win.c_str(), retInfo.width, retInfo.height, retInfo.format, &mono16[0]);
                    } else {
                        TYDisplayImage(win.c_str(), frame.image[i].width, frame.image[i].height, frame.image[i].pixelFormat, frame.image[i].buffer);
                    }
                } else {
                    TYDecodeError err = TYGetDecodeBufferSize(&image_info, &destSize, TY_OUTPUT_FORMAT_AUTO);
                    if(err == TY_DECODE_SUCCESS) {
                        TYDecodeResult retInfo;
                        std::vector<uint8_t> image_data(destSize);
                        ASSERT_DEC_OK(TYDecodeImage(&image_info,  TY_OUTPUT_FORMAT_AUTO, (void*)&image_data[0], destSize, &retInfo));
                        TYDisplayImage(win.c_str(), retInfo.width, retInfo.height, retInfo.format, &image_data[0]);
                    } else {
                        TYDisplayImage(win.c_str(), frame.image[i].width, frame.image[i].height, frame.image[i].pixelFormat, frame.image[i].buffer, scale_unit);
                    }
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
