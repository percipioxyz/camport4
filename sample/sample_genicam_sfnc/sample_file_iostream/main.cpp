#include "../common/common.hpp"
#include "FileAccessControl.hpp"

int main(int argc, char* argv[]) {
    std::string ID, IP;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
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

    ASSERT_OK( TYOpenInterface(selectedDev.iface.id, &hIface) );
    ASSERT_OK( TYOpenDevice(hIface, selectedDev.id, &hDevice) );
    
    percipio::FileAccessControl fileStream(hDevice);
    std::vector<char> buffer(1024);
    std::string content;
    // Open the file for reading.
    if (fileStream.open(percipio::FILE_SEL_DEFAULT0, std::ios::in)) {
        int count = 0;
        while (fileStream.read(buffer.data(), buffer.size())) {
            content.append(buffer.data(), buffer.size());
            count++;
        }
        if (fileStream.gcount() > 0) {
            content.append(buffer.data(), fileStream.gcount());
        }
        std::cout << "File content: " << content << std::endl;
        fileStream.close();
    }
    
    // Open the file for writing.
    if (fileStream.open(percipio::FILE_SEL_USER_SET0, std::ios::out)) {
        fileStream << content << std::endl;
        fileStream.close();
    }

    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    
    LOGD("Main done!");
    return 0;
}