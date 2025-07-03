#include "../common/common.hpp"


#if _WIN32
#include <conio.h>
#elif __linux__
#include <termio.h>
#define TTY_PATH    "/dev/tty"
#define STTY_DEF    "stty -raw echo -F"
#endif

static char GetChar(void)
{
    char ch = -1;
    do {
#if _WIN32
    if (kbhit()) //check fifo
    {
        ch = getche(); //read fifo
    }
#elif __linux__
    fd_set rfds;
    struct timeval tv;
    system(STTY_DEF TTY_PATH);
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 10;
    if (select(1, &rfds, NULL, NULL, &tv) > 0)
    {
        ch = getchar();
    }
#endif
    }while(ch == '\n' || ch == '\r' || ch == ' ');
    return ch;
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

static void it_select_enum(TY_DEV_HANDLE hDevice, std::string name, uint32_t &selected_val){
    uint32_t m_Source = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, name.c_str(), &m_Source));
    std::vector<TYEnumEntry> modes(m_Source);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, name.c_str(), modes.data(), m_Source, &m_Source));

    std::cout << " supports the following "<< name <<":" << std::endl;
    for(size_t i = 0; i < m_Source; i++) {
        std::cout << "\t" << std::dec << i << "." << modes[i].name << std::endl;
    }

    std::cout << "Please select a "<< name <<" according to the above number!" << std::endl;
    char idx = -1;
    do {
        idx = GetChar() & 0xff;
        if(idx == -1) continue;
        idx -= '0';
        if(idx >= 0 && idx < m_Source) break;
        else std::cout << "Error, please select again!" << std::endl;
    } while(true);

    std::cout << "====== idx = " << (int)(idx) << std::endl;
    ASSERT_OK(idx >= m_Source);
    std::cout << "Select " << modes[idx].name << std::endl;
    ASSERT_OK(TYEnumSetValue(hDevice, name.c_str(), modes[idx].value));
    selected_val = modes[idx].value;
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

    uint32_t m_Source = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "SourceSelector", &m_Source));

    std::vector<TYEnumEntry> entrys(m_Source);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "SourceSelector", entrys.data(), m_Source, &m_Source));

    bool exit_main = false;

    ASSERT_OK(TYEnumSetString(hDevice, "SourceSelector", entrys[0].name));
    ASSERT_OK(TYBooleanSetValue(hDevice, "ComponentEnable", true));
    uint32_t mode = 2; //continues
    it_select_enum(hDevice, "AcquisitionMode", mode);
    uint32_t source = 0;
    if (mode == 0) {
        it_select_enum(hDevice, "TriggerSource", source);
    } else if (mode == 2) {
        std::cout << "Enable Frame Rate Mode ?" << std::endl;
        bool en = false;
        char idx = -1;
        do {
            idx = GetChar() & 0xff;
            if(idx == -1) continue;
            idx -= '0';
            if(idx != 0){ en = true;}
            break;
        }while(true);
        std::cout << (en ? "true" : "false") << std::endl;
        int ret = TYBooleanSetValue(hDevice, "AcquisitionFrameRateEnable", en);
        if (en){
            double min = 0, max = 0, fps = 1;
            TYFloatGetMin(hDevice, "AcquisitionFrameRate", &min);
            TYFloatGetMax(hDevice, "AcquisitionFrameRate", &max);
            TYFloatGetValue(hDevice, "AcquisitionFrameRate", &fps);
            std::cout << fps << "in (" << min << "," << max <<")" << std::endl;
            std::cout << "input fps want set:" << std::endl;
            std::cin >> fps;
            std::cout << "set fps :" << fps << std::endl;
            TYFloatSetValue(hDevice, "AcquisitionFrameRate", fps);
        }

    }
    char* frameBuffer[2] = {0};
    if (!exit_main) {
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
        DepthViewer depthViewer("Depth");
        LOGD("Start capture");
        ASSERT_OK( TYStartCapture(hDevice) );

        LOGD("While loop to fetch frame");
        TY_FRAME_DATA frame;
        int index = 0;
        while(!exit_main) {
            if (source == 8) {
                ASSERT_OK(TYCommandExec(hDevice, "TriggerSoftware"));
            }
            int err = TYFetchFrame(hDevice, &frame, -1);
            if( err == TY_STATUS_OK ) {
                LOGD("Get frame %d", ++index);

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
        ASSERT_OK( TYStopCapture(hDevice) );
        //stop will not release all buffers, need clear
        ASSERT_OK( TYClearBufferQueue(hDevice) );
        delete frameBuffer[0];
        delete frameBuffer[1];
    }

    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    //if(frameBuffer[0]) delete frameBuffer[0];
    //if(frameBuffer[1]) delete frameBuffer[1];

    LOGD("Main done!");
    return 0;
}
