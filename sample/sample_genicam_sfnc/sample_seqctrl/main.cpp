#include "../common/common.hpp"
#include "SequenceCtrl.hpp"
#include <vector>

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
    bool start_stop_test  = false;
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
    const std::vector<double> exposures = {2000.0, 10000.0};
    SequencerController seqCtrl(hDevice, exposures);
    seqCtrl.setup();

    double depth_scale_unit = 1.0;
    if(TY_STATUS_OK == TYEnumSetValue(hDevice, "SourceSelector", SRC_SEL_DEPTH)) {
        ASSERT_OK(TYFloatGetValue(hDevice, TY_DEPTH_SCALE, &depth_scale_unit));
    }

    std::vector<char> frameBuffer[2];
    LOGD("Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
    LOGD("     - Get size of framebuffer, %d", frameSize);

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
    TY_FRAME_DATA frame;
    int index = 0;
    bool exit_main = false;
    while(!exit_main) {
        //If never fetch image. camera may work in trigger mode, try add TYCommandExec("TriggerSoftware")
        int err = TYFetchFrame(hDevice, &frame, -1);
        if( err == TY_STATUS_OK ) {
            LOGD("Get frame %d", ++index);

            float fps = get_fps();
            if (fps > 0){
                LOGI("fps: %.2f", fps);
            }
            
            for (int i = 0; i < frame.validCount; i++){
                decode_and_display_image(frame.image[i], depth_scale_unit);
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
    seqCtrl.disable();
    //stop will not release all buffers, need clear
    ASSERT_OK( TYClearBufferQueue(hDevice) );
    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    LOGD("Main done!");
    return 0;
}
