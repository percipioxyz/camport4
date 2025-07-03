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
        } else if(strcmp(argv[i], "-h") == 0) {
            LOGI("Usage: SimpleView_FetchFrame [-h] [-id <ID>] [-ip <IP>]");
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

    TY_COMPONENT_ID allComps;
    ASSERT_OK( TYGetComponentIDs(hDevice, &allComps) );

    //-----------------------------------------------------------------------
    //Enumerate all the items that the camera user set can select
    //-----------------------------------------------------------------------
    uint32_t count = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "UserSetSelector", &count)); 
    std::vector<TYEnumEntry> entrys(count);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "UserSetSelector", &entrys[0], count, &count));
    for(int i = 0; i < count; i++) {
        LOGD("UserSetSelector : %s - value : %d", entrys[i].name, entrys[i].value);
    }

    //Set the camera properties and save them to a user set entry
    
    //First: Set the camera features use API:TYIntegerSetValue/TYBooleanSetValue/TYEnumSetValue/TYFloatSetValue 
    //TODO...

    //Second: Select a valid user set entry, and then execute the save operation to save parameters to user set item 8
    ASSERT_OK(TYEnumSetValue(hDevice, "UserSetSelector", 8));
    //Set a name/description for the selected User Set(Use : UserSetSelector)
    ASSERT_OK(TYStringSetValue(hDevice, "UserSetDescription", "HelloUserSet")); 
    ASSERT_OK(TYCommandExec(hDevice, "UserSetSave"));

    //Enumerate all items that can be loaded as default parameters after the camera starts
    //After performing this operation, you can see the saved user set.
    //And you can use the API: TYEnumSetValue/TYEnumSetString to set it as default startup parameters
    count = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "UserSetDefault", &count));
    if(count > 0) {
        entrys.resize(count);
        ASSERT_OK(TYEnumGetEntryInfo(hDevice, "UserSetDefault", &entrys[0], count, &count));
        for(int i = 0; i < count; i++) {
            LOGD("UserSetDefault : %s - value : %d", entrys[i].name, entrys[i].value);
        }
    } else {
        LOGD("No Default User set item!");
    }

    //Read the user set index which is currently in use.
    //This is readonly feature
    //This property is initialized when the camera is powered on and the default user set is loaded. 
    //It changes automatically when the user executes the command UserSetSave
    int64_t current = 0;
    ASSERT_OK(TYIntegerGetValue(hDevice, "UserSetCurrent", &current));
    LOGD("UserSetCurrent = %d", current);


    //Get the name/description of the selected User Set(Use : UserSetSelector)
    uint32_t length;
    ASSERT_OK(TYStringGetLength(hDevice, "UserSetDescription", &length)); 
    LOGD("UserSetDescription max length = %d", length);

    std::vector<char> desc(length);
    ASSERT_OK(TYStringGetValue(hDevice, "UserSetDescription", (char*)&desc[0], length)); 
    LOGD("UserSetDescription  = %s", desc.data());

    LOGD("Main done!");
    return 0;
}
