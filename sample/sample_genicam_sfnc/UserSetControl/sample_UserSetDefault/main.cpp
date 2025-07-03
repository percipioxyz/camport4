#include "common.hpp"

//UserSetDefault
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
    ASSERT_OK(TYParamGetToolTip(hDevice, "UserSetDefault", str, sizeof(str)));
    std::cout << "ToolTip:" << str << std::endl;

    ASSERT_OK(TYParamGetDescriptor(hDevice, "UserSetDefault", str, sizeof(str)));
    std::cout << "Descriptor:" << str << std::endl;
    
    ASSERT_OK(TYParamGetDisplayName(hDevice, "UserSetDefault", str, sizeof(str)));
    std::cout << "DisplayName:" << str << std::endl;

    TY_VISIBILITY_TYPE visibility;
    ASSERT_OK(TYParamGetVisibility(hDevice, "UserSetDefault", &visibility));
    std::cout << "Visibility:" << visibility << std::endl;

    ParamType type;
    ASSERT_OK(TYParamGetType(hDevice, "UserSetDefault", &type));
    ASSERT_OK(Enumeration != type);

    // UserSetDefault: Defines the default UserSet to load after the camrea reboots.
    // This sample enumerates all UserSets that can be set by the user as UserSetDefault. After successful setting, reboot the camera, then observe whether the image effects matche the expected results to verify if UserSetDefault was successfully applied.
    uint32_t count = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "UserSetDefault", &count));

    std::vector<TYEnumEntry> entrys(count);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "UserSetDefault", &entrys[0], count, &count));
    ASSERT_OK(count == 0);

    std::cout << "Select a user set to load at startup:" << std::endl;
    for(size_t i = 0; i < count; i++) {
      std::cout << "\t" << i << "\tVal: " << entrys[i].value << "\t" << entrys[i].name << std::endl;
    }

    int index = 0;
    while(true) {
      std::cin >> index;
      if(index >= count || index < 0) {
        std::cout << "ERR: Out of range, retry!" << std::endl;
      } else {
        break;
      }
    }
    
    ASSERT_OK(TYEnumSetValue(hDevice, "UserSetDefault", entrys[index].value));

    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );

    LOGD("Main done!");
    return 0;
}