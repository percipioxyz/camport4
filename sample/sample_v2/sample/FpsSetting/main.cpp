#include "Device.hpp"

using namespace percipio_layer;


class FpsSettingCamera : public FastCamera
{
    public:
        FpsSettingCamera() : FastCamera() {};
        ~FpsSettingCamera() {}; 

        TY_STATUS SetFps(float fps);
};


TY_STATUS FpsSettingCamera::SetFps(float fps)
{
    TY_DEV_HANDLE _handle = handle();
    TY_STATUS status;
    if(!_handle) {
        std::cout << "Invalid camera handle!" << std::endl;
        return TY_STATUS_INVALID_HANDLE;
    }

    status = TYBooleanSetValue(_handle, "AcquisitionFrameRateEnable", true);
    if (status != TY_STATUS_OK) {
        std::cout << "Enable fps failed!" << std::endl;
        return TY_STATUS_INVALID_PARAMETER;
    }

    status = TYFloatSetValue(_handle, "AcquisitionFrameRate", fps);
    if (status != TY_STATUS_OK) {
        std::cout << "Invalid fps!" << std::endl;
        return TY_STATUS_INVALID_PARAMETER;
    }

    return TY_STATUS_OK;
}


int main(int argc, char* argv[])
{
    std::string ID;
    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0) {
            ID = argv[++i];
        } else if(strcmp(argv[i], "-h") == 0) {
            std::cout << "Usage: " << argv[0] << "   [-h] [-id <ID>]" << std::endl;
            return 0;
        }
    }

    FpsSettingCamera camera;
    if(TY_STATUS_OK != camera.open(ID.c_str())) {
        std::cout << "open camera failed!" << std::endl;
        return -1;
    }

    float fps = 1;
    TY_STATUS status = camera.SetFps(fps);
    std::cout << "set fps to " << fps << ",  ret = " << status << std::endl;

    bool process_exit = false;
    TYFrameParser parser;
    parser.RegisterKeyBoardEventCallback([](int key, void* data) {
        if(key == 'q' || key == 'Q') {
            *(bool*)data = true;
            std::cout << "Exit..." << std::endl; 
        }
    }, &process_exit);

    status = camera.stream_enable(FastCamera::stream_depth);
    std::cout << "enable depth stream,  ret = " << status << std::endl;

    if(TY_STATUS_OK != camera.start()) {
        std::cout << "stream start failed!" << std::endl;
        return -1;
    }
    
    while(!process_exit) {
        auto frame = camera.tryGetFrames(2000);
        if(frame) parser.update(frame);
    }
    
    std::cout << "Main done!" << std::endl;
    return 0;
}
