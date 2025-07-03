
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
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

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    int32_t color, ir, depth;
    color = ir = depth = 1;
    std::string config_file;
    std::string output_file;
    bool config_file_valid = true;

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
        } else if(strcmp(argv[i], "-s") == 0) {
            config_file = argv[++i];
        } else if(strcmp(argv[i], "-o") == 0) {
            output_file = argv[++i];
        } else if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: %s [-h] [-id <ID>] [-ip <IP>] [-s <config_file>] [-o <dump_file>]", argv[0]);
            LOGI("\t[-h] Show this help info");
            LOGI("\t[-id <ID>] select camera sn to work with");
            LOGI("\t[-ip <IP>] select camera IP to work with");
            LOGI("\t[-s <config_file>] save config_file to camera");
            LOGI("\t[-o <dump_file>] save configs read from camera to dump_file");
            LOGI("\t[-ir=off/-color=off/-depth=off] disable ir/color/depth");
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

    if (!config_file.empty()) {
        ASSERT_OK(write_parameters_to_storage(hDevice,  config_file));
        LOGD("Write Config Done!");
        ASSERT_OK( TYCloseDevice(hDevice));
        ASSERT_OK( TYCloseInterface(hIface) );
        ASSERT_OK( TYDeinitLib() );
        return 0;
    }
    
    if(!output_file.empty()){
        std::string js_data;
        ASSERT_OK(load_parameters_from_storage(hDevice, js_data));
        if (!js_data.empty()) {
            std::ofstream file(output_file, std::ios::out);
            if (!file.is_open()) {
                throw std::runtime_error("Unable to open file");
            }
            file << js_data;
            file.close();
        }
    }

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

    DepthViewer depthViewer("Depth");
    if (allComps & TY_COMPONENT_DEPTH_CAM && depth) {
        LOGD("Has depth camera, open depth cam");
        ASSERT_OK(TYEnableComponents(hDevice, TY_COMPONENT_DEPTH_CAM));

        //depth map pixel format is uint16_t ,which default unit is  1 mm
        //the acutal depth (mm)= PixelValue * ScaleUnit 
        float scale_unit = 1.;
        TYGetFloat(hDevice, TY_COMPONENT_DEPTH_CAM, TY_FLOAT_SCALE_UNIT, &scale_unit);
        depthViewer.depth_scale_unit = scale_unit;
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
    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    delete frameBuffer[0];
    delete frameBuffer[1];

    LOGD("Main done!");
    return 0;
}
