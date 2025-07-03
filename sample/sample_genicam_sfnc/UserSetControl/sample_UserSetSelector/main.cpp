#include "common.hpp"

//UserSetSelector
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

    char str[1024];
    ASSERT_OK(TYParamGetToolTip(hDevice, "UserSetSelector", str, sizeof(str)));
    std::cout << "ToolTip:" << str << std::endl;

    ASSERT_OK(TYParamGetDescriptor(hDevice, "UserSetSelector", str, sizeof(str)));
    std::cout << "Descriptor:" << str << std::endl;
    
    ASSERT_OK(TYParamGetDisplayName(hDevice, "UserSetSelector", str, sizeof(str)));
    std::cout << "DisplayName:" << str << std::endl;

    TY_VISIBILITY_TYPE visibility;
    ASSERT_OK(TYParamGetVisibility(hDevice, "UserSetSelector", &visibility));
    std::cout << "Visibility:" << visibility << std::endl;

    ParamType type;
    ASSERT_OK(TYParamGetType(hDevice, "UserSetSelector", &type));
    ASSERT_OK(Enumeration != type);

    TY_ACCESS_MODE access;
    ASSERT_OK(TYParamGetAccess(hDevice, "UserSetSelector", &access));
    ASSERT_OK(access != (TY_ACCESS_READABLE | TY_ACCESS_WRITABLE));

    //Traverse "UserSetSelector" and enumerate all UserSets supported by the camera
    uint32_t count = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "UserSetSelector", &count));

    std::vector<TYEnumEntry> entrys(count);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "UserSetSelector", &entrys[0], count, &count));

    std::cout << "UserSetSelector support:" << std::endl;
    for(size_t i = 0; i < count; i++) {
      std::cout << "\t-" << entrys[i].name << std::endl;
      std::cout << "\t\t" << entrys[i].tooltip << std::endl;
      std::cout << "\t\t" << entrys[i].description << std::endl;
      std::cout << "\t\t" << entrys[i].displayName << std::endl;
      std::cout << "\t\tvalue: " << entrys[i].value << std::endl;
    }

    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );

    LOGD("Main done!");
    return 0;
}