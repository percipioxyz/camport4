#include "common.hpp"

//UserSetSave
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
    ASSERT_OK(TYParamGetToolTip(hDevice, "UserSetSave", str, sizeof(str)));
    std::cout << "ToolTip:" << str << std::endl;

    ASSERT_OK(TYParamGetDescriptor(hDevice, "UserSetSave", str, sizeof(str)));
    std::cout << "Descriptor:" << str << std::endl;
    
    ASSERT_OK(TYParamGetDisplayName(hDevice, "UserSetSave", str, sizeof(str)));
    std::cout << "DisplayName:" << str << std::endl;

    TY_VISIBILITY_TYPE visibility;
    ASSERT_OK(TYParamGetVisibility(hDevice, "UserSetSave", &visibility));
    std::cout << "Visibility:" << visibility << std::endl;

    ParamType type;
    ASSERT_OK(TYParamGetType(hDevice, "UserSetSave", &type));
    ASSERT_OK(Command != type);

    // This sample saves the current set of camera parameter configurations to the UserSet selected by UserSetSelector. 
    // If the selected UserSet is a system-defined UserSet, the UserSetSave operation is invalid because system-defined UserSets cannot be overwritten.
    uint32_t count = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "UserSetSelector", &count));

    std::vector<TYEnumEntry> entrys(count);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "UserSetSelector", &entrys[0], count, &count));

    for(size_t i = 0; i < count; i++) {
      uint32_t val = entrys[i].value;
      ASSERT_OK(TYEnumSetValue(hDevice, "UserSetSelector", val));

      if(val < 8) {
        TY_ACCESS_MODE access;
        ASSERT_OK(TYParamGetAccess(hDevice, "UserSetSave", &access));

        // val < 8: system-defined UerSet, cannot be overwritten
        ASSERT_OK(access & TY_ACCESS_READABLE);
        std::cout << "The default user set(" << val << ") is protected and cannot be modified." << std::endl;
      } else {
        char desc[200];
        sprintf(desc, "Custom_%d", i - 8);
        // Set Description of the selected User Set content
        ASSERT_OK(TYStringSetValue(hDevice, "UserSetDescription", desc));

        // val >= 8: user-defined UserSet, after set description, writable.
        TY_ACCESS_MODE access;
        ASSERT_OK(TYParamGetAccess(hDevice, "UserSetSave", &access));
        ASSERT_OK(!(access & TY_ACCESS_WRITABLE));

        ASSERT_OK(TYCommandExec(hDevice, "UserSetSave"));
        std::cout << "Success: save current camera config to user set idx(" << val << ")." << std::endl;

        // After successfully saving the current camera configuration to a specified UserSet, the camera needs to be rebooted and reopened. Then use UserSetSelector + UserSetLoad to reload the same UserSet. Observe whether the image output matches the expected result to verify the success of the UserSetSave operation.
      }
    }
    
    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );

    LOGD("Main done!");
    return 0;
}