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

//Get depth data from XYZ48 frames
static  void parseXYZ48(int16_t* xyz48, int16_t* depth, int width, int height)
{
    for (int pix = 0; pix < width*height; pix++)
        depth[pix] =*(xyz48 + 3*pix + 2);
}

//Estimate new calibration data
//When the lens distortion is very serious, using the old calibration data to correct the distortion will lose a lot of field of view data. 
//Using the new internal parameter to estimate the lens field of view Angle to correct the distortion can greatly reduce the wide-angle data loss.
//The value range of the parameter coef is 0 - 1, in general, the more severe the distortion, the smaller the coefficient should be.
static void TYGenerateNewDepthCalibData(TY_CAMERA_CALIB_INFO& calib, float coef, TY_CAMERA_CALIB_INFO& new_calib)
{
  memcpy(&new_calib, &calib, sizeof(TY_CAMERA_CALIB_INFO));
  new_calib.intrinsic.data[0] *= coef;
  new_calib.intrinsic.data[4] *= coef;
  return;
}

TY_CAMERA_INTRINSIC operator * (const TY_CAMERA_INTRINSIC & intrinsic, float scale) 
{
  TY_CAMERA_INTRINSIC rsz_intrinsic;
  rsz_intrinsic.data[0] = intrinsic.data[0] * scale;
  rsz_intrinsic.data[1] = 0;
  rsz_intrinsic.data[2] = intrinsic.data[2] * scale;
  rsz_intrinsic.data[3] = 0;
  rsz_intrinsic.data[4] = intrinsic.data[4] * scale;
  rsz_intrinsic.data[5] = intrinsic.data[5] * scale;
  rsz_intrinsic.data[6] = 0;
  rsz_intrinsic.data[7] = 0;
  rsz_intrinsic.data[8] = 1;

  return rsz_intrinsic; 
}

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    int32_t color, ir, depth;
    color = ir = depth = 1;

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
        } else if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: SimpleView_XYZ48 [-h] [-id <ID>] [-ip <IP>]");
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

    //try to enable depth map
    LOGD("Configure components, open depth cam");
    if (allComps & TY_COMPONENT_DEPTH_CAM && depth) {
        bool b_support_xyz48_fmt = false;
        std::vector<TY_ENUM_ENTRY> image_mode_list;
        ASSERT_OK(get_feature_enum_list(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, image_mode_list));
        for(size_t i = 0; i < image_mode_list.size(); i++) {
          if((image_mode_list[i].value & TY_PIXEL_FORMAT_XYZ48) == TY_PIXEL_FORMAT_XYZ48) {
            b_support_xyz48_fmt = true;
            ASSERT_OK(TYSetEnum(hDevice, TY_COMPONENT_DEPTH_CAM, TY_ENUM_IMAGE_MODE, image_mode_list[i].value));
          }
        }
        ASSERT(b_support_xyz48_fmt);
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_DEPTH_CAM));
    }

    TY_CAMERA_CALIB_INFO depth_calib, new_depth_calib; 
    ASSERT_OK(TYGetStruct(hDevice, TY_COMPONENT_DEPTH_CAM, TY_STRUCT_CAM_CALIB_DATA, &depth_calib, sizeof(depth_calib)));
    TYGenerateNewDepthCalibData(depth_calib, 0.7f, new_depth_calib);
    
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

            int fps = get_fps();
            if (fps > 0){
                LOGI("fps: %d", fps);
            }
            
            for (int i = 0; i < frame.validCount; i++){
                if (frame.image[i].status != TY_STATUS_OK) continue;

                if(frame.image[i].componentID == TY_COMPONENT_DEPTH_CAM) {
                    ASSERT(frame.image[i].pixelFormat == TYPixelFormatCoord3D_ABC16);
                    int32_t width = frame.image[i].width;
                    int32_t height = frame.image[i].height;
                    std::vector<int16_t> depth(width * height);

                    parseXYZ48((int16_t*)frame.image[i].buffer, &depth[0], width, height);
                    
                    TY_IMAGE_DATA src;
                    src.width = width;
                    src.height = height;
                    src.size = width * height * 2;
                    src.pixelFormat = TYPixelFormatCoord3D_C16;
                    src.buffer = &depth[0];

                    std::vector<int16_t> undistort_depth(width * height);
                    TY_IMAGE_DATA dst;
                    dst.width = width;
                    dst.height = height;
                    dst.size = width * height * 2;
                    dst.buffer = &undistort_depth[0];
                    dst.pixelFormat = TYPixelFormatCoord3D_C16;

                    float f_depth_scale = (1.f * src.width / new_depth_calib.intrinsicWidth);
                    TY_CAMERA_INTRINSIC rsz_intrinsic = new_depth_calib.intrinsic * f_depth_scale;
                    ASSERT_OK(TYUndistortImage(&depth_calib, &src, &rsz_intrinsic, &dst));

                    auto win = ty_comp_window_name(frame.image[i].componentID);
                    TYDisplayImage(win.c_str(), width, height, TYPixelFormatCoord3D_C16, &undistort_depth[0]);
                } else { 
                    uint32_t destSize;
                    auto win = ty_comp_window_name(frame.image[i].componentID);
                    TYImageInfo image_info = ty_image_info(frame.image[i]);
                    TYDecodeError err = TYGetDecodeBufferSize(&image_info, &destSize, TY_OUTPUT_FORMAT_AUTO);
                    if(err == TY_DECODE_SUCCESS) {
                        TYDecodeResult retInfo;
                        std::vector<uint8_t> image_data(destSize);
                        ASSERT_DEC_OK(TYDecodeImage(&image_info,  TY_OUTPUT_FORMAT_AUTO, (void*)&image_data[0], destSize, &retInfo));
                        TYDisplayImage(win.c_str(), retInfo.width, retInfo.height, retInfo.format, &image_data[0]);
                    } else {
                        TYDisplayImage(win.c_str(), frame.image[i].width, frame.image[i].height, frame.image[i].pixelFormat, frame.image[i].buffer);
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
