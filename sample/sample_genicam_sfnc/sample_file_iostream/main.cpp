#include "../common/common.hpp"
#include "FileAccessControl.hpp"

int main(int argc, char* argv[]) {
    std::string ID, IP, r_file_name, w_file_name, DATA;
    int r_flag = 0, w_flag = 0, list_flag = 0;
    TY_INTERFACE_HANDLE hIface = NULL;
    TY_DEV_HANDLE hDevice = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-id") == 0) {
          ID = argv[++i];
        }
        else if (strcmp(argv[i], "-ip") == 0) {
          IP = argv[++i];
        }
        else if (strcmp(argv[i], "-read") == 0) {
          r_flag = 1;
          list_flag = 1;
          r_file_name = argv[++i];
        }
        // write will start writing the file from the beginning.
        else if (strcmp(argv[i], "-write") == 0) {
          w_flag = 1;
          list_flag = 1;
          w_file_name = argv[++i]; 
        }
        // trunc will first clear the file and then write to it.
        else if (strcmp(argv[i], "-trunc") == 0) {
          w_flag = 2;
          list_flag = 1;
          w_file_name = argv[++i]; 
        }
        else if (strcmp(argv[i], "-data") == 0) {
          DATA = argv[++i];
        }
        else if (strcmp(argv[i], "-list") == 0) {
          list_flag = 1;
        }
        else if (strcmp(argv[i], "-h") == 0) {
          // You can refer to the getFileMaps() of the header file FileAccessControl.hpp for the file_name
          LOGI("Usage: sample_file_iostream [-h] [-id <ID>] [-ip <IP>] [-list] [-read <file_name>] [-write <file_name>] [-trunc <file_name>] [-data <data>]");
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
    
    if (list_flag) {
        uint32_t _cnt = 0;
        ASSERT_OK(TYEnumGetEntryCount(hDevice, "FileSelector", &_cnt));

        std::vector<TYEnumEntry> _cnt_entry(_cnt);
        ASSERT_OK(TYEnumGetEntryInfo(hDevice, "FileSelector", _cnt_entry.data(), _cnt, &_cnt));

        for (int i = 0; i < _cnt; i++) {
            std::cout << "  Name : " << _cnt_entry[i].name << " value : " << _cnt_entry[i].value << std::endl;
        }
    }

    percipio::FileAccessControl fileStream(hDevice);
    std::vector<char> buffer(256);
    std::string content = "";
    if (r_flag) {
        if (fileStream.open(r_file_name.c_str(), std::ios::in)) {
            int count = 0;
            while (fileStream.read(buffer.data(), buffer.size())) {
                content.append(buffer.data(), buffer.size());
                count++;
            }
            if (fileStream.gcount() > 0) {
                content.append(buffer.data(), fileStream.gcount());
            }
            printf("Data content (hex):\n");
            for (uint32_t i = 0; i < content.size(); ++i) {
                if (i % 16 == 0) {
                    printf("\n%04X: ", i); 
                }
                printf("%02X ", static_cast<unsigned char>(content[i]));
            }
            printf("\n");
            fileStream.close();
        }
    }
    if (w_flag) {
        if ((w_flag == 1 && fileStream.open(w_file_name.c_str(), std::ios::out))
            || (w_flag == 2 && fileStream.open(w_file_name.c_str(), std::ios::trunc))) {
            if (DATA.size() > 0) {
                fileStream << DATA;
            } else if (r_flag) {
                fileStream << content;
            } else {
                fileStream << "Test";
            }
            fileStream.flush();
            fileStream.close();
        }
    }

    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );
    
    LOGD("Main done!");
    return 0;
}
