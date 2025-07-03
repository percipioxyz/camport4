#include "common.hpp"


//UserSetCurrent
int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-id") == 0){
            ID = argv[++i];
        } else if(strcmp(argv[i], "-ip") == 0) {
            IP = argv[++i];
        } else if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: SimpleView_FetchFrame [-h] [-id <ID>] [-ip <IP>]");
            return 0;
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

    // UserSetLoad: load the selected UserSet to camera.
    // This sample traverse all UserSets and perform UserSet loading operation to verify the influence of UserSetLoad on UserSetCurrent:
    // Expected result: After the UserSet loading operation, The value of UserSetCurrent will update automatically to correspond to the UserSet on which UserSetLoad performs loading.
    char str[1024];
    ASSERT_OK(TYParamGetToolTip(hDevice, "UserSetCurrent", str, sizeof(str)));
    std::cout << "ToolTip:" << str << std::endl;

    ASSERT_OK(TYParamGetDescriptor(hDevice, "UserSetCurrent", str, sizeof(str)));
    std::cout << "Descriptor:" << str << std::endl;
    
    ASSERT_OK(TYParamGetDisplayName(hDevice, "UserSetCurrent", str, sizeof(str)));
    std::cout << "DisplayName:" << str << std::endl;

    TY_VISIBILITY_TYPE visibility;
    ASSERT_OK(TYParamGetVisibility(hDevice, "UserSetCurrent", &visibility));
    std::cout << "Visibility:" << visibility << std::endl;

    ParamType type;
    ASSERT_OK(TYParamGetType(hDevice, "UserSetCurrent", &type));
    ASSERT_OK(Integer != type);

    TY_ACCESS_MODE access;
    ASSERT_OK(TYParamGetAccess(hDevice, "UserSetCurrent", &access));
    ASSERT_OK(access != TY_ACCESS_READABLE);


    uint32_t count = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "UserSetSelector", &count));
    
    std::vector<TYEnumEntry> entrys(count);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "UserSetSelector", &entrys[0], count, &count));
    for(size_t i = 0; i < count; i++) {
      // Select the UserSet
      ASSERT_OK(TYEnumSetValue(hDevice, "UserSetSelector", entrys[i].value));

      ASSERT_OK(TYParamGetAccess(hDevice, "UserSetLoad", &access));

      // Only UserSets that have a set of parameter configurations saved (non-empty) are writable for UserSetLoad.
      if(access & TY_ACCESS_WRITABLE) {
        std::cout << "Load User Set index : " << entrys[i].value << std::endl;
        ASSERT_OK(TYCommandExec(hDevice, "UserSetLoad"));

        int64_t val;
        ASSERT_OK(TYIntegerGetValue(hDevice, "UserSetCurrent", &val));
        std::cout << "Got current User Set index : " << val << std::endl;
      }
    }

    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );

    LOGD("Main done!");
    return 0;
}
