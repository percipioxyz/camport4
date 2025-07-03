#include "common.hpp"

//UserSetLoad
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
    ASSERT_OK(TYParamGetToolTip(hDevice, "UserSetLoad", str, sizeof(str)));
    std::cout << "ToolTip:" << str << std::endl;

    ASSERT_OK(TYParamGetDescriptor(hDevice, "UserSetLoad", str, sizeof(str)));
    std::cout << "Descriptor:" << str << std::endl;
    
    ASSERT_OK(TYParamGetDisplayName(hDevice, "UserSetLoad", str, sizeof(str)));
    std::cout << "DisplayName:" << str << std::endl;

    TY_VISIBILITY_TYPE visibility;
    ASSERT_OK(TYParamGetVisibility(hDevice, "UserSetLoad", &visibility));
    std::cout << "Visibility:" << visibility << std::endl;

    ParamType type;
    ASSERT_OK(TYParamGetType(hDevice, "UserSetLoad", &type));
    ASSERT_OK(Command != type);

    // This sample traverses all UserSets and check UserSetLoad access permission one by one. 
    // If the access permission is writable, it indicates that the camera can load the current UserSet. If not, it means the current UserSet is empty.
    // To test whether UserSetLoad takes effect: verify that the actual image output corresponds to the parameter settings defined in the UserSet.
    
    uint32_t count = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "UserSetSelector", &count));

    std::vector<TYEnumEntry> entrys(count);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "UserSetSelector", &entrys[0], count, &count));

    for(size_t i = 0; i < count; i++) {
      uint32_t val = entrys[i].value;
      ASSERT_OK(TYEnumSetValue(hDevice, "UserSetSelector", val));

      TY_ACCESS_MODE access;
      ASSERT_OK(TYParamGetAccess(hDevice, "UserSetLoad", &access));

      if(access & TY_ACCESS_WRITABLE) {
        // Writable: indicates that the current UserSet is not empty and the camera can load the current UserSet
        ASSERT_OK(TYCommandExec(hDevice, "UserSetLoad"));
        std::cout << "The user set (IDX:" << entrys[i].value << ") is available and has been loaded successfully." << std::endl;
      } else {
        // Non-writable: the current UserSet is empty
        std::cout << "The user set (IDX:" << entrys[i].value << ") is invalid." << std::endl;
      }
    }
    
    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );

    LOGD("Main done!");
    return 0;
}