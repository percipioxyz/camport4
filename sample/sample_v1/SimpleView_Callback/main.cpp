#include <thread>
#include "common.hpp"
#include <vector>

struct CallbackData {
    int             index;
    TY_DEV_HANDLE   hDevice;
    float           f_depth_scale_unit = 1.f;
    bool            exit;
};

class CallbackWrapper
{
public:
    typedef void(*TY_FRAME_CALLBACK) (TY_FRAME_DATA*, void* userdata);

    CallbackWrapper(){
        _hDevice = NULL;
        _cb = NULL;
        _userdata = NULL;
        _exit = true;
    }

    TY_STATUS TYRegisterCallback(TY_DEV_HANDLE hDevice, TY_FRAME_CALLBACK v, void* userdata)
    {
        _hDevice = hDevice;
        _cb = v;
        _userdata = userdata;
        _exit = false;
        _cbThread = std::thread(&CallbackWrapper::workerThread, this);
        return TY_STATUS_OK;
    }

    void TYUnregisterCallback()
    {
        if (!_exit) {
            _exit = true;
            if (_cbThread.joinable()) {
                _cbThread.join();
            }
        }
    }

private:
    void workerThread()
    {
        TY_FRAME_DATA frame;

        while (!_exit)
        {
            int err = TYFetchFrame(_hDevice, &frame, 100);
            if (!err) {
                _cb(&frame, _userdata);
            }
        }
        LOGI("frameCallback exit!");
    }

    TY_DEV_HANDLE _hDevice;
    TY_FRAME_CALLBACK _cb;
    void* _userdata;

    bool _exit;
    std::thread _cbThread;
};

void frameCallback(TY_FRAME_DATA* frame, void* userdata)
{
    CallbackData* pData = (CallbackData*)userdata;
    LOGD("=== Get frame %d", ++pData->index);

    float fps = get_fps();
    if (fps > 0){
        LOGI("fps: %.2f", fps);
    }

    for (int i = 0; i < frame->validCount; i++){
        if (frame->image[i].status != TY_STATUS_OK) continue;
        decode_and_display_image(frame->image[i], pData->f_depth_scale_unit);
    }

    int key = TYWaitKeyEvents();
    switch (key & 0xff) {
    case 0xff: break;
    case 'q':
        pData->exit = true;
        // have to call TYUnregisterCallback to release thread
        break;
    default: LOGD("Unmapped key %d", key);
    }
    LOGD("=== Callback: Re-enqueue buffer(%p, %d)", frame->userBuffer, frame->bufferSize);
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
    int32_t color, ir, depth;
    color = ir = depth = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-id") == 0) {
            ID = argv[++i];
        }
        else if (strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        }
        else if (strcmp(argv[i], "-color=off") == 0) {
            color = 0;
        }
        else if (strcmp(argv[i], "-depth=off") == 0) {
            depth = 0;
        }
        else if (strcmp(argv[i], "-ir=off") == 0) {
            ir = 0;
        }
        else if (strcmp(argv[i], "-h") == 0) {
          LOGI("Usage: SimpleView_Callback [-h] [-id <ID>] [-ip <IP>]");
          return 0;
        }
    }

    if (!color && !depth && !ir) {
        LOGD("At least one component need to be on");
        return -1;
    }

    LOGD("Init lib");
    ASSERT_OK(TYInitLib());
    TY_VERSION_INFO ver;
    ASSERT_OK(TYLibVersion(&ver));
    LOGD("     - lib version: %d.%d.%d", ver.major, ver.minor, ver.patch);

    std::vector<TY_DEVICE_BASE_INFO> selected;
    ASSERT_OK(selectDevice(TY_INTERFACE_ALL, ID, IP, 1, selected));
    ASSERT(selected.size() > 0);
    TY_DEVICE_BASE_INFO& selectedDev = selected[0];

    ASSERT_OK(TYOpenInterface(selectedDev.iface.id, &hIface));
    ASSERT_OK(TYOpenDevice(hIface, selectedDev.id, &hDevice));

    TY_COMPONENT_ID allComps;
    ASSERT_OK(TYGetComponentIDs(hDevice, &allComps));
    ASSERT_OK(TYDisableComponents(hDevice, allComps));

    if (allComps & TY_COMPONENT_RGB_CAM  && color) {
        LOGD("Has RGB camera, open RGB cam");
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM));
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

    LOGD("Prepare image buffer");
    uint32_t frameSize;
    ASSERT_OK(TYGetFrameBufferSize(hDevice, &frameSize));
    LOGD("     - Get size of framebuffer, %d", frameSize);

    LOGD("     - Allocate & enqueue buffers");
    std::vector<char> frameBuffer[2];
    frameBuffer[0].resize(frameSize);
    frameBuffer[1].resize(frameSize);
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0].data(), frameSize);
    ASSERT_OK(TYEnqueueBuffer(hDevice, frameBuffer[0].data(), frameSize));
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1].data(), frameSize);
    ASSERT_OK(TYEnqueueBuffer(hDevice, frameBuffer[1].data(), frameSize));

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

    // Create callback thread
    CallbackData cb_data;
    cb_data.index = 0;
    cb_data.hDevice = hDevice;
    cb_data.f_depth_scale_unit = scale_unit;
    cb_data.exit = false;
    
    TY_COMPONENT_ID componentIDs = 0;
    ASSERT_OK(TYGetEnabledComponents(hDevice, &componentIDs));

    LOGD("Start capture");
    ASSERT_OK(TYStartCapture(hDevice));

    // Register Callback
    CallbackWrapper cbWrapper;
    cbWrapper.TYRegisterCallback(hDevice, frameCallback, &cb_data);
    
    LOGD("While loop to fetch frame");
    while (!cb_data.exit) {
        MSLEEP(1000*10);
    }

    cbWrapper.TYUnregisterCallback();
    ASSERT_OK(TYStopCapture(hDevice));
    ASSERT_OK(TYCloseDevice(hDevice));
    ASSERT_OK(TYCloseInterface(hIface));
    ASSERT_OK(TYDeinitLib());

    LOGD("Main done!");
    return 0;
}