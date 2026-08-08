#include "../common/common.hpp"
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

class StreamCtrl
{
    public:
        StreamCtrl(const TY_DEV_HANDLE dev, const char* stream);
        ~StreamCtrl() {};

    private:
        const TY_DEV_HANDLE hDevice;
};

StreamCtrl::StreamCtrl(const TY_DEV_HANDLE dev,  const char* stream)
    : hDevice(dev)
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

    uint32_t m_BinningCnt = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "BinningHorizontal", &m_BinningCnt));
    entrys.resize(m_BinningCnt);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "BinningHorizontal", entrys.data(), m_BinningCnt, &m_BinningCnt));
    if(m_BinningCnt == 1) {
        std::cout << stream << " only supports one binning mode: " << entrys[0].name << std::endl;
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
    }

    ASSERT_OK(TYBooleanSetValue(hDevice, "ComponentEnable", true));
}

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    bool start_stop_test  = false;
    int direct  = 0;
    int round = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-id") == 0) {
          ID = argv[++i];
        } else if (strcmp(argv[i], "-ip") == 0) {
          IP = argv[++i];
        } else if (strcmp(argv[i], "-test") == 0) {
            start_stop_test = true;
        } else if (strcmp(argv[i], "-direct") == 0) {
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
    //ASSERT_OK( TYOpenDevice(hIface, selectedDev.id, &hDevice) );
    ( TYOpenDevice(hIface, selectedDev.id, &hDevice) );

    uint32_t m_Source = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "SourceSelector", &m_Source));

    std::vector<TYEnumEntry> entrys(m_Source);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "SourceSelector", entrys.data(), m_Source, &m_Source));

    bool exit_main = false;
    std::vector<std::shared_ptr<StreamCtrl>> Streams;
    if (direct) {
        goto direct_start;
    }
    std::cout << "The current camera  supports the following component:" << std::endl;
    for(size_t i = 0; i < m_Source && !exit_main; i++) {
        std::cout << entrys[i].name << std::endl;
        
        std::cout << "Do you want to enable the above components (Y/N)?" << std::endl;

        bool wait_cmd = true;
        do {
            char c;
            std::cin >> c;
            cin_clear_rest_line();
            switch(c) {
                case 'y':
                case 'Y':
                {
                    Streams.push_back(std::shared_ptr<StreamCtrl>(new StreamCtrl(hDevice, entrys[i].name)));
                    wait_cmd = false;
                    break; 
                }
                case 'n':
                case 'N':
                {
                    std::cout << "Disable : " << entrys[i].name << std::endl;
                    ASSERT_OK(TYEnumSetString(hDevice, "SourceSelector", entrys[i].name));
                    ASSERT_OK(TYBooleanSetValue(hDevice, "ComponentEnable", false));
                    wait_cmd = false;
                    break;
                }
                case 'q':
                case 'Q':
                {
                    exit_main = true;
                    wait_cmd = false;
                    break;
                }
                default:
                    std::cout << "Error, please select again!" << std::endl;
                    break; 
            }
        } while(wait_cmd);
    }
direct_start:
    double depth_scale_unit = 1.0;
    if(TY_STATUS_OK == TYEnumSetValue(hDevice, "SourceSelector", SRC_SEL_DEPTH)) {
        ASSERT_OK(TYFloatGetValue(hDevice, TY_DEPTH_SCALE, &depth_scale_unit));
    }

    LOGD("%d-th round\n", round++);
    std::vector<char> frameBuffer[2];
    if (!exit_main) {
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
    /*
        bool hasTrigger;
        ASSERT_OK(TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &hasTrigger));
        if (hasTrigger) {
            LOGD("Disable trigger mode");
            TY_TRIGGER_PARAM_EX trigger;
            trigger.mode = TY_TRIGGER_MODE_OFF;
            ASSERT_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM_EX, &trigger, sizeof(trigger)));
        }
    */
        LOGD("Start capture");
        ASSERT_OK( TYStartCapture(hDevice) );

        LOGD("While loop to fetch frame");
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

            if (start_stop_test && index == 10) {
                break;
            }
        }
        ASSERT_OK( TYStopCapture(hDevice) );
        //stop will not release all buffers, need clear
        ASSERT_OK( TYClearBufferQueue(hDevice) );

        if (!exit_main) {
            goto direct_start;
        }
    }

    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    LOGD("Main done!");
    return 0;
}
