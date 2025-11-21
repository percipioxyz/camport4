#include "../common/common.hpp"
static bool offline = false;
static int offline_cnt = 0;

void eventCallback(TY_EVENT_INFO *event_info, void *userdata)
{
    if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE) {
        LOGD("=== Event Callback: Device Offline!");
        offline = true;
        offline_cnt++;
    }
}

int main(int argc, char* argv[])
{
    std::string ID, IP;
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
    int index = 0;
    bool exit_main = false;
do_open:
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    std::vector<TY_DEVICE_BASE_INFO> selected;
    int ret = selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected) ;
    if (ret < 0) {
        LOGD("Discover Device failed, retry after 2s!\n");
        MSleep(2000);
        goto do_open;
    }
    ASSERT(selected.size() > 0);
    TY_DEVICE_BASE_INFO& selectedDev = selected[0];

    ASSERT_OK( TYOpenInterface(selectedDev.iface.id, &hIface) );
    TY_FW_ERRORCODE err_code;
    ret = TYOpenDevice(hIface, selectedDev.id, &hDevice, &err_code);
    if (ret < 0) {
        LOGD("Open device %s failed ret %d(%s)!", selectedDev.id, ret, TYErrorString(ret));
        if (ret == TY_STATUS_FIRMWARE_ERROR) {
            LOGD("FW init err code %d!", err_code);
            parse_firmware_errcode(err_code);
            ASSERT_OK(TYCloseDevice(hDevice));
        }
        ASSERT_OK(TYCloseInterface(hIface));
        goto do_open;
    }

    offline = false;

    char* frameBuffer[2] = {0};
    LOGD("Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
    LOGD("     - Get size of framebuffer, %d", frameSize);

    LOGD("     - Allocate & enqueue buffers");
    
    frameBuffer[0] = new char[frameSize];
    frameBuffer[1] = new char[frameSize];
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize) );
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize) );

    LOGD("Register event callback");
    ASSERT_OK(TYRegisterEventCallback(hDevice, eventCallback, NULL));
    DepthViewer depthViewer("Depth");
    LOGD("Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    LOGD("While loop to fetch frame");
    TY_FRAME_DATA frame;
    while(!exit_main && !offline) {
        int err = TYFetchFrame(hDevice, &frame, 20000);
        if( err == TY_STATUS_OK ) {
            LOGD("Get frame %d, offline cnt %d", ++index, offline_cnt);

            int fps = get_fps();
            if (fps > 0){
                LOGI("fps: %d", fps);
            }

            cv::Mat depth, irl, irr, color;
            parseFrame(frame, &depth, &irl, &irr, &color);
            if(!depth.empty()){
                depthViewer.show(depth);
            }
            if(!irl.empty()){ cv::imshow("LeftIR", irl); }
            if(!irr.empty()){ cv::imshow("RightIR", irr); }
            if(!color.empty()){ cv::imshow("Color", color); }

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
    //Offline stop will return err ignore here
    ( TYStopCapture(hDevice) );
    //stop will not release all buffers, need clear
    ( TYClearBufferQueue(hDevice) );
    delete frameBuffer[0];
    delete frameBuffer[1];

    //Offline close will return err ignore here
    ( TYCloseDevice(hDevice));
    if (!exit_main && offline) {
      goto do_open;
    }
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    //if(frameBuffer[0]) delete frameBuffer[0];
    //if(frameBuffer[1]) delete frameBuffer[1];
    LOGD("Main done!");
    return 0;
}
