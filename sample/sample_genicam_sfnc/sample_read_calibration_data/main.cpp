#include <iomanip>
#include "../common/common.hpp"

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
        else if (strcmp(argv[i], "-direct") == 0) {
          direct = 1;
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

    uint32_t m_src_num = 0;
    ASSERT_OK(TYEnumGetEntryCount(hDevice, "SourceSelector", &m_src_num));

    std::vector<TYEnumEntry> entrys(m_src_num);
    ASSERT_OK(TYEnumGetEntryInfo(hDevice, "SourceSelector", entrys.data(), m_src_num, &m_src_num));

    for(auto& iter : entrys) {
        ASSERT_OK(TYEnumSetString(hDevice, "SourceSelector", iter.name));

        std::cout << iter.name << std::endl;
        TY_ACCESS_MODE access;
        ASSERT_OK(TYParamGetAccess(hDevice, "IntrinsicWidth", &access));
        if(access & TY_ACCESS_READABLE) {
            int64_t width;
            ASSERT_OK(TYIntegerGetValue(hDevice, "IntrinsicWidth", &width));
            std::cout << "\tIntrinsicWidth: " << width << std::endl;
        }

        ASSERT_OK(TYParamGetAccess(hDevice, "IntrinsicHeight", &access));
        if(access & TY_ACCESS_READABLE) {
            int64_t height;
            ASSERT_OK(TYIntegerGetValue(hDevice, "IntrinsicHeight", &height));
            std::cout << "\tIntrinsicHeight: " << height << std::endl;
        }

        ASSERT_OK(TYParamGetAccess(hDevice, "Intrinsic", &access));
        if(access & TY_ACCESS_READABLE) {
            char description[512];
            ASSERT_OK(TYParamGetDescriptor(hDevice, "Intrinsic", description, sizeof(description)));

            double mat_intr_3x3[9];
            ASSERT_OK(TYByteArrayGetValue(hDevice, "Intrinsic", reinterpret_cast<uint8_t*>(mat_intr_3x3), sizeof(mat_intr_3x3)));
            std::cout << "\tIntrinsic:" << description << std::endl;

            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_intr_3x3[0]
                    << std::setw(12) << mat_intr_3x3[1]
                    << std::setw(12) << mat_intr_3x3[2] << std::endl;
            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_intr_3x3[3]
                    << std::setw(12) << mat_intr_3x3[4]
                    << std::setw(12) << mat_intr_3x3[5] << std::endl;
            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_intr_3x3[6]
                    << std::setw(12) << mat_intr_3x3[7]
                    << std::setw(12) << mat_intr_3x3[8] << std::endl;
        } else {
            std::cout << "\tNo intrinsic data." << std::endl; 
        }

        ASSERT_OK(TYParamGetAccess(hDevice, "Intrinsic2", &access));
        if(access & TY_ACCESS_READABLE) {
            char description[512];
            ASSERT_OK(TYParamGetDescriptor(hDevice, "Intrinsic2", description, sizeof(description)));

            double mat_intr_3x3[9];
            ASSERT_OK(TYByteArrayGetValue(hDevice, "Intrinsic2", reinterpret_cast<uint8_t*>(mat_intr_3x3), sizeof(mat_intr_3x3)));
            std::cout << "\tRectifyed intrinsic:" << description << std::endl;

            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_intr_3x3[0]
                    << std::setw(12) << mat_intr_3x3[1]
                    << std::setw(12) << mat_intr_3x3[2] << std::endl;
            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_intr_3x3[3]
                    << std::setw(12) << mat_intr_3x3[4]
                    << std::setw(12) << mat_intr_3x3[5] << std::endl;
            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_intr_3x3[6]
                    << std::setw(12) << mat_intr_3x3[7]
                    << std::setw(12) << mat_intr_3x3[8] << std::endl;
        } else {
            std::cout << "\tNo rectifyed intrinsic data." << std::endl; 
        }

        ASSERT_OK(TYParamGetAccess(hDevice, "Rotation", &access));
        if(access & TY_ACCESS_READABLE) {
            char description[512];
            ASSERT_OK(TYParamGetDescriptor(hDevice, "Rotation", description, sizeof(description)));

            double mat_rotation_3x3[9];
            ASSERT_OK(TYByteArrayGetValue(hDevice, "Rotation", reinterpret_cast<uint8_t*>(mat_rotation_3x3), sizeof(mat_rotation_3x3)));
            std::cout << "\tRotation:" << description << std::endl;

            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_rotation_3x3[0]
                    << std::setw(12) << mat_rotation_3x3[1]
                    << std::setw(12) << mat_rotation_3x3[2] << std::endl;
            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_rotation_3x3[3]
                    << std::setw(12) << mat_rotation_3x3[4]
                    << std::setw(12) << mat_rotation_3x3[5] << std::endl;
            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_rotation_3x3[6]
                    << std::setw(12) << mat_rotation_3x3[7]
                    << std::setw(12) << mat_rotation_3x3[8] << std::endl;
        } else {
            std::cout << "\tNo rotation data." << std::endl; 
        }

        ASSERT_OK(TYParamGetAccess(hDevice, "Distortion", &access));
        if(access & TY_ACCESS_READABLE) {
            char description[512];
            ASSERT_OK(TYParamGetDescriptor(hDevice, "Distortion", description, sizeof(description)));

            double mat_distortion_12x1[12];
            ASSERT_OK(TYByteArrayGetValue(hDevice, "Distortion", reinterpret_cast<uint8_t*>(mat_distortion_12x1), sizeof(mat_distortion_12x1)));
            std::cout << "\tDistortion:" << description << std::endl;
            std::cout << std::fixed << std::setprecision(6) << "\t\t";
            for(size_t i = 0; i < sizeof(mat_distortion_12x1) / sizeof(double); i++) {
                 std::cout << std::setw(12) << mat_distortion_12x1[i];
            }
            std::cout << std::endl;
        } else {
            std::cout << "\tNo distortion data." << std::endl; 
        }
    
        ASSERT_OK(TYParamGetAccess(hDevice, "Extrinsic", &access));
        if(access & TY_ACCESS_READABLE) {
            char description[512];
            ASSERT_OK(TYParamGetDescriptor(hDevice, "Extrinsic", description, sizeof(description)));

            double mat_extrinsic_4x4[16];
            ASSERT_OK(TYByteArrayGetValue(hDevice, "Extrinsic", reinterpret_cast<uint8_t*>(mat_extrinsic_4x4), sizeof(mat_extrinsic_4x4)));
            std::cout << "\tExtrinsic:" << description << std::endl;

            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_extrinsic_4x4[0]
                    << std::setw(12) << mat_extrinsic_4x4[1]
                    << std::setw(12) << mat_extrinsic_4x4[2] 
                    << std::setw(12) << mat_extrinsic_4x4[3] << std::endl;
            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_extrinsic_4x4[4]
                    << std::setw(12) << mat_extrinsic_4x4[5]
                    << std::setw(12) << mat_extrinsic_4x4[6]
                    << std::setw(12) << mat_extrinsic_4x4[7] << std::endl;
            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_extrinsic_4x4[8]
                    << std::setw(12) << mat_extrinsic_4x4[9]
                    << std::setw(12) << mat_extrinsic_4x4[10]
                    << std::setw(12) << mat_extrinsic_4x4[11] << std::endl;
            std::cout << "\t\t" << std::setw(12) << std::setfill(' ') << std::fixed << std::setprecision(6) << mat_extrinsic_4x4[12]
                    << std::setw(12) << mat_extrinsic_4x4[13]
                    << std::setw(12) << mat_extrinsic_4x4[14]
                    << std::setw(12) << mat_extrinsic_4x4[15] << std::endl;
        } else {
            std::cout << "\tNo extrinsic data." << std::endl; 
        }
    }
    
    ASSERT_OK( TYCloseDevice(hDevice));
    ASSERT_OK( TYCloseInterface(hIface) );
    ASSERT_OK( TYDeinitLib() );

    LOGD("Main done!");
    return 0;
}
