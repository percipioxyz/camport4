#include "../common/common.hpp"
#include "TYFeatureList.h"
#include "../common/ImageSpeckleFilter.hpp"
int InitSpeckleFilter(TY_DEV_HANDLE hDevice, DepthSpeckleFilter &speckle_filter)
{
  ASSERT_OK(TYEnumSetValue(hDevice, "SourceSelector", SRC_SEL_DEPTH));
  TY_ACCESS_MODE access;
  ASSERT_OK(TYParamGetAccess(hDevice, "IntrinsicWidth", &access));
  if(access & TY_ACCESS_READABLE) {
    int64_t width;
    ASSERT_OK(TYIntegerGetValue(hDevice, "IntrinsicWidth", &width));
    speckle_filter.depth_cali_width = width;
  }

  ASSERT_OK(TYParamGetAccess(hDevice, "IntrinsicHeight", &access));
  if(access & TY_ACCESS_READABLE) {
    int64_t height;
    ASSERT_OK(TYIntegerGetValue(hDevice, "IntrinsicHeight", &height));
    speckle_filter.depth_cali_height = height;
  }

  ASSERT_OK(TYParamGetAccess(hDevice, "Intrinsic", &access));
  if(access & TY_ACCESS_READABLE) {
      ASSERT_OK(TYByteArrayGetValue(hDevice, "Intrinsic", reinterpret_cast<uint8_t*>(speckle_filter.depth_cali_intri), sizeof(speckle_filter.depth_cali_intri)));
  }
  ASSERT_OK(TYParamGetAccess(hDevice, TY_DEPTH_SCALE, &access));
  if(access & TY_ACCESS_READABLE) {
    //if scale uint is changged, This val need update too
    ASSERT_OK(TYFloatGetValue(hDevice, TY_DEPTH_SCALE, &speckle_filter.depth_scale_unit));
  }
  //speckle_filter.max_physical_size = You Want to Set; //0 means disable
  //If Physical size filter needed, should init above codes. Can skip them otherwise.

    //Set speckle_filter.max_diff = You Want to Set;
    //Set speckle_filter.max_speckle_size = You Want to Set;
    //Set speckle_filter.new_val = You Want to Set;
    int64_t depth_width, depth_height;
    int32_t binning_h = 1, binning_v = 1;
    ASSERT_OK(TYIntegerGetValue(hDevice, TY_SENSOR_W, &depth_width));
    ASSERT_OK(TYIntegerGetValue(hDevice, TY_SENSOR_H, &depth_height));
    ASSERT_OK(TYEnumGetValue(hDevice, TY_BIN_V, &binning_v));
    ASSERT_OK(TYEnumGetValue(hDevice, TY_BIN_H, &binning_h));

    speckle_filter.init(depth_width/binning_v, depth_height/binning_h);
    return 0;
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
    int direct  = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-id") == 0) {
          ID = argv[++i];
        }
        else if (strcmp(argv[i], "-ip") == 0) {
          IP = argv[++i];
        }
        else if (strcmp(argv[i], "-direct") == 0) {
          direct = 1;
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

    ASSERT_OK(TYEnumSetValue(hDevice, "SourceSelector", SRC_SEL_DEPTH));
    ASSERT_OK(TYBooleanSetValue(hDevice, "ComponentEnable", true));
    

    LOGD("Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
    LOGD("     - Get size of framebuffer, %d", frameSize);

    char* frameBuffer[2] = {0};
    LOGD("     - Allocate & enqueue buffers");
    frameBuffer[0] = new char[frameSize];
    frameBuffer[1] = new char[frameSize];
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize) );
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize) );

    LOGD("Register event callback");
    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));
    DepthSpeckleFilter filter;
    InitSpeckleFilter(hDevice, filter);
    DepthViewer depthViewer("Depth");
    LOGD("Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    LOGD("While loop to fetch frame");
    TY_FRAME_DATA frame;
    int index = 0;
    bool exit_main = false;
    while(!exit_main) {
        int err = TYFetchFrame(hDevice, &frame, -1);
        if( err == TY_STATUS_OK ) {
            LOGD("Get frame %d", ++index);

            int fps = get_fps();
            if (fps > 0){
                LOGI("fps: %d", fps);
            }

            cv::Mat depth;
            parseFrame(frame, &depth, nullptr, nullptr, nullptr);
            if(!depth.empty()){
                filter.Compute(depth.ptr<uint16_t>());
                depthViewer.show(depth);
            }
            
            int key = cv::waitKey(1);
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
    if(frameBuffer[0]) delete frameBuffer[0];
    if(frameBuffer[1]) delete frameBuffer[1];

    LOGD("Main done!");
    return 0;
}
