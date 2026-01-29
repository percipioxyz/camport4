#include "../common/common.hpp"


static void use_new_apis() {
    LOGD("This is a new device. We have provided GenICam style API in TYParameter.h to get/set parameters.");
 }

static void use_old_apis() {
    LOGD("This is a old device. Please use the API in TYApi.h to get/set parameters");
}

int main(int argc, char* argv[])
{
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    int direct  = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-id") == 0) {
          ID = argv[++i];
        }
        else if (strcmp(argv[i], "-ip") == 0) {
          IP = argv[++i];
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

    if (TYIsNetworkInterface(selectedDev.iface.type)) {
                LOGD("    - device %s:", selectedDev.id);
                if (strlen(selectedDev.userDefinedName) != 0) {
                    LOGD("          vendor     : %s", selectedDev.userDefinedName);
                } else {
                    LOGD("          vendor     : %s", selectedDev.vendorName);
                }
                LOGD("          model      : %s", selectedDev.modelName);

                LOGD("          device MAC : %s", selectedDev.netInfo.mac);
                LOGD("          device IP  : %s", selectedDev.netInfo.ip);
                LOGD("          TL version : %s", selectedDev.netInfo.tlversion);
                if (strcmp(selectedDev.netInfo.tlversion, "Gige_2_1") == 0) {
                    use_new_apis();
                } else {
                    use_old_apis();
                }
    } else {
                TY_DEV_HANDLE handle;
                ASSERT_OK( TYOpenInterface(selectedDev.iface.id, &hIface) );
                int32_t ret = TYOpenDevice(hIface, selectedDev.id, &handle);
                if (ret == 0) {
                    TYGetDeviceInfo(handle, &selectedDev);
                    TYCloseDevice(handle);
                    LOGD("    - device %s:", selectedDev.id);
                } else {
                    LOGD("    - device %s(open failed, error: %d)", selectedDev.id, ret);
                }
                TYCloseInterface(hIface);
                if (strlen(selectedDev.userDefinedName) != 0) {
                    LOGD("          vendor     : %s", selectedDev.userDefinedName);
                } else {
                    LOGD("          vendor     : %s", selectedDev.vendorName);
                }
                LOGD("          model      : %s", selectedDev.modelName);

                if (strcmp(selectedDev.usbInfo.tlversion, "USB3Vision_1_2") == 0) {
                    use_new_apis();
                } else {
                    use_old_apis();
                }
    }

    ASSERT_OK( TYDeinitLib() );

    LOGD("Main done!");
    return 0;
}
