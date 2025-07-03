#include "../common/common.hpp"
#include "../common/json11.hpp"

#if _WIN32
#include <conio.h>
#elif __linux__
#include <termio.h>
#define TTY_PATH    "/dev/tty"
#define STTY_DEF    "stty -raw echo -F"
#endif
#include <vector>

using namespace json11;
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

void write_feat(TY_DEV_HANDLE hDevice, const std::string &feat,
    const enum ParamType &type, const std::string &val)
{
    TY_STATUS ret = 0;
    switch(type){
    case Command:{
        ret = TYCommandExec(hDevice, feat.c_str());
        break;
    }
    case Integer:{
        int64_t _v = atoi(val.c_str());
        printf("%s %lld\n", val.c_str(), _v);
        ret = TYIntegerSetValue(hDevice, feat.c_str(), _v);
        break;
    }
    case Float:{
        double _v = atof(val.c_str());
        ret = TYFloatSetValue(hDevice, feat.c_str(), _v);
        break;
    }
    case Boolean: {
        bool _v = false;
        if (atoi(val.c_str()) == 1) {
            _v = true;
        } 
        ret = TYBooleanSetValue(hDevice, feat.c_str(), _v);
        break;
    }
    case Enumeration:{
        int32_t _v = atoi(val.c_str());
        printf("%s %d\n", val.c_str(), _v);
        ret = TYEnumSetValue(hDevice, feat.c_str(), _v);
        break;
    }
    case String:{
        ret = TYStringSetValue(hDevice, feat.c_str(), val.c_str());
        break;
    }
    case ByteArray:
        break;
    default:
        break;
    }
    LOGD("write %s %s ret %d\n", feat.c_str(), val.c_str(), ret);
}

#define CHECK_AND_PRINT(ret,n1, n2, v) do {\
    if (ret == 0) {\
        LOGD("%s: %s, val %s", n1.c_str(), n2, v.c_str()); \
    } else {\
        return; \
    }\
} while(0)
void read_feat(TY_DEV_HANDLE hDevice, const std::string &feat,
    const enum ParamType &type)
{
    TY_STATUS ret = 0;
    switch(type){
    case Command:
        break;
    case Integer:{
        int64_t _v;
        ret = TYIntegerGetValue(hDevice, feat.c_str(), &_v);
        CHECK_AND_PRINT(ret, feat, "val", std::to_string(_v));
        ret = TYIntegerGetMin(hDevice, feat.c_str(), &_v);
        CHECK_AND_PRINT(ret, feat, "min", std::to_string(_v));
        ret = TYIntegerGetMax(hDevice, feat.c_str(), &_v);
        CHECK_AND_PRINT(ret, feat, "max", std::to_string(_v));
        ret = TYIntegerGetStep(hDevice, feat.c_str(), &_v);
        CHECK_AND_PRINT(ret, feat, "step", std::to_string(_v));
        break;
    }
    case Float:{
        double _v;
        ret = TYFloatGetValue(hDevice, feat.c_str(), &_v);
        CHECK_AND_PRINT(ret, feat, "val", std::to_string(_v));
        ret = TYFloatGetMin(hDevice, feat.c_str(), &_v);
        CHECK_AND_PRINT(ret, feat, "min", std::to_string(_v));
        ret = TYFloatGetMax(hDevice, feat.c_str(), &_v);
        CHECK_AND_PRINT(ret, feat, "max", std::to_string(_v));
        ret = TYFloatGetStep(hDevice, feat.c_str(), &_v);
        CHECK_AND_PRINT(ret, feat, "step", std::to_string(_v));
        break;
    }
    case Boolean:{
        bool _v;
        ret = TYBooleanGetValue(hDevice, feat.c_str(), &_v);
        CHECK_AND_PRINT(ret, feat, "val", std::to_string(_v));
        break;
    }
    case Enumeration:{
        int32_t _v = 0;
        ret = TYEnumGetValue(hDevice, feat.c_str(), &_v);
        CHECK_AND_PRINT(ret, feat, "val", std::to_string(_v));
        uint32_t cnt = 0;
        uint32_t filled_cnt = 0;
        TYEnumGetEntryCount(hDevice, feat.c_str(), &cnt);
        std::vector<TYEnumEntry> infos(cnt);
        TYEnumGetEntryInfo(hDevice, feat.c_str(), &infos[0], cnt, &filled_cnt);
        for(uint32_t i = 0; i < filled_cnt; i++) {
            LOGD("===         %14s:     value(%d), desc(%s)", "", infos[i].value, infos[i].name);

        }
        break;
    }
    case String:{
        std::string _v;
        uint32_t len = 0;
        TYStringGetLength(hDevice, feat.c_str(), &len);
        _v.resize(len + 1);
        ret = TYStringGetValue(hDevice, feat.c_str(), &_v[0], len);
        CHECK_AND_PRINT(ret, feat, "val", (_v));
        break;
    }
    case ByteArray:
        break;
    default:
        break;
    }
}

static void read_write_feature(TY_DEV_HANDLE hDevice, std::string feat, std::string val, int op)
{
    enum ParamType type;
    CHECK_RET(TYParamGetType(hDevice, feat.c_str(), &type));
    TY_ACCESS_MODE access;
    CHECK_RET(TYParamGetAccess(hDevice, feat.c_str(), &access));
    if (op) {
        //write
        if (!(access&TY_ACCESS_WRITABLE)) {
            LOGD("%s Not writeable", feat.c_str());
        } else {
            write_feat(hDevice, feat, type, val);
        }
    } else {
        //read
        if (!(access&TY_ACCESS_READABLE)) {
            LOGD("%s Not readable", feat.c_str());
        } else {
            read_feat(hDevice, feat, type);
        }
    }
}

class param{
public:
    param(std::string f, std::string v, int rw): feat(f), val(v), op(rw){}
    std::string feat;
    std::string val;
    int op;
};

static void parse_input_file(std::string fileName, std::vector<param> &list)
{
    /* sample json code
    {
        "param": [
            {
                "name": "UserSetSelector",
                "val": "8"
            },
            {
                "name": "UserSetSave",
                "val":"1"
            }
        ]
    }
     */
    std::ifstream ifs(fileName, std::ios::binary | std::ios::ate);//open as binary, ate opened and seek to the end of the file
    if (!ifs.is_open()) {
        LOGE("Unable to open file %s", fileName.c_str());
        return;
    }
    size_t size = ifs.tellg();
    char* buffer = new char[size + 1];
    buffer[size] = '\0';
    ifs.seekg(0, std::ios::beg);
    ifs.read(buffer, size);
    std::string jscode = std::string(buffer, size);
    delete[] buffer;
    ifs.close();
    std::string err;
    const auto json = Json::parse(jscode, err);
    Json params = json["param"];
    if(!params.is_array()) {
        LOGE("Json format err!");
    }
    for (auto &k : params.array_items()) {
        const Json& name = k["name"];
        const Json& val = k["val"];
        int op = 0;
        std::string _val;
        if(!name.is_string()) continue;
        if(val.is_string()) {
            op = 1;
            _val = val.string_value();
        }
        list.push_back(param(name.string_value(), _val, op));
    }
}

int main(int argc, char* argv[])
{
    std::string ID, IP, feat, val;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    std::string input_file;
    std::string refresh_config_file;
    //0 is read, 1 is write
    int op = 0;
    bool display = true;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-id") == 0) {
          ID = argv[++i];
        }
        else if (strcmp(argv[i], "-ip") == 0) {
          IP = argv[++i];
        }
        else if (strcmp(argv[i], "-input") == 0) {
          input_file = argv[++i];
        }
        else if (strcmp(argv[i], "-refresh") == 0) {
          refresh_config_file = argv[++i];
        }
        else if (strcmp(argv[i], "-feat") == 0) {
            feat = argv[++i];
        }
        else if (strcmp(argv[i], "-no_gui") == 0) {
            display = false;
        }
        else if (strcmp(argv[i], "-val") == 0) {
            val = argv[++i];
            op = 1;
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
    if (!input_file.empty()) {
        std::vector<param> param_list;
        parse_input_file(input_file, param_list);
        for (auto p:param_list) {
            LOGD("################ %s", p.feat.c_str());
            read_write_feature(hDevice, p.feat, p.val, p.op);
        }
    } else {
        read_write_feature(hDevice, feat, val, op);
    }
    char* frameBuffer[2] = {0};
    uint32_t frameSize;
    TY_FRAME_DATA frame;
    int index = 0;
    bool exit_main = false;
    DepthViewer depthViewer("Depth");
    if(!display) {
        goto out;
    }
    LOGD("Prepare image buffer");
    ASSERT_OK( TYGetFrameBufferSize(hDevice, &frameSize) );
    LOGD("     - Get size of framebuffer, %d", frameSize);

    LOGD("     - Allocate & enqueue buffers");

    frameBuffer[0] = new char[frameSize];
    frameBuffer[1] = new char[frameSize];
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[0], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[0], frameSize) );
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer[1], frameSize);
    ASSERT_OK( TYEnqueueBuffer(hDevice, frameBuffer[1], frameSize) );
    LOGD("Start capture");
    ASSERT_OK( TYStartCapture(hDevice) );

    LOGD("While loop to fetch frame");
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
            case 'r':
            case 'R':
                if (!refresh_config_file.empty()) {
                    std::vector<param> param_list;
                    parse_input_file(refresh_config_file, param_list);
                    for (auto p:param_list) {
                        read_write_feature(hDevice, p.feat, p.val, p.op);
                    }
                }
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
out:
    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    LOGD("Main done!");
    return 0;
}
